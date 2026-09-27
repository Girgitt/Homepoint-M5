#include "WebPortal.h"

#include <LittleFS.h>
#include <Update.h>
#include <utility>

#include "Pages.h"

namespace homepoint::web {

WebPortal::WebPortal() = default;

void WebPortal::begin(
    config::PersistentSettingsStore* persistentStore,
    config::BootstrapSettings* bootstrap,
    config::ConfigStore* configStore,
    network::WifiManager* wifi,
    network::MqttManager* mqtt,
    std::function<void()> reloadCallback,
    std::function<void(bool)> debugUiChangedCallback) {
  persistentStore_ = persistentStore;
  bootstrap_ = bootstrap;
  configStore_ = configStore;
  wifi_ = wifi;
  mqtt_ = mqtt;
  reloadCallback_ = std::move(reloadCallback);
  debugUiChangedCallback_ = std::move(debugUiChangedCallback);

  installRoutes();
  installCaptiveRoutes();
  server_.begin();
}

bool WebPortal::authenticate(AsyncWebServerRequest* request) const {
  if (!bootstrap_ || !bootstrap_->configured) return true;
  if (request->authenticate(
          bootstrap_->webUsername.c_str(),
          bootstrap_->webPassword.c_str())) {
    return true;
  }
  request->requestAuthentication();
  return false;
}

void WebPortal::installRoutes() {
  server_.on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!bootstrap_ || !bootstrap_->configured) {
      request->send_P(200, "text/html; charset=utf-8", kSetupPage);
      return;
    }
    if (!authenticate(request)) return;
    request->send_P(200, "text/html; charset=utf-8", kAdminPage);
  });

  server_.on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest* request) {
    request->send(200, "application/json", wifi_->scanNetworksJson());
  });

  server_.on("/api/setup", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!bootstrap_ || !persistentStore_) {
      request->send(500, "text/plain", "Storage unavailable");
      return;
    }

    auto readParam = [request](const char* name) -> String {
      return request->hasParam(name, true) ? request->getParam(name, true)->value() : "";
    };

    const String ssid = readParam("ssid");
    const String webUsername = readParam("webUsername");
    const String webPassword = readParam("webPassword");
    if (ssid.isEmpty() || webUsername.isEmpty() || webPassword.isEmpty()) {
      request->send(400, "text/plain", "SSID, web username and web password are required");
      return;
    }

    String hostname = readParam("hostname");
    if (hostname.isEmpty()) hostname = "homepoint-m5";
    if (hostname.length() > 32u) {
      request->send(400, "text/plain", "Hostname must be no longer than 32 characters");
      return;
    }

    if (configStore_ && configStore_->mounted()) {
      if (!configStore_->ensureDefaultConfig()) {
        request->send(500, "text/plain", "Could not create config.json");
        return;
      }
      String configError;
      if (!configStore_->setHostname(hostname, configError)) {
        request->send(500, "text/plain", String("Could not save hostname to config.json: ") + configError);
        return;
      }
    }

    bootstrap_->configured = true;
    bootstrap_->wifiSsid = ssid;
    bootstrap_->wifiPassword = readParam("wifiPassword");
    // Retained only as a migration/recovery fallback for older bootstrap
    // records. Normal operation reads hostname from config.json.
    bootstrap_->hostname = hostname;
    bootstrap_->webUsername = webUsername;
    bootstrap_->webPassword = webPassword;

    if (!persistentStore_->save(*bootstrap_)) {
      request->send(500, "text/plain", "Could not save bootstrap settings");
      return;
    }

    request->send(200, "text/plain", "Saved. Rebooting into station mode.");
    requestRestart();
  });

  server_.on("/api/hostname", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!authenticate(request)) return;
    request->send(200, "text/plain", wifi_ ? wifi_->effectiveHostname() : String());
  });

  server_.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!authenticate(request)) return;
    const String hostname = wifi_ ? wifi_->effectiveHostname() : String();
    String body = "Host: " + hostname;
    body += " | Wi-Fi: " + wifi_->statusText() + " (" + wifi_->ipAddress() + ")";
    body += " | MQTT: " + mqtt_->statusText();
    body += " | FS: " + configStore_->statusText();
    body += String(" | UI: ") + (bootstrap_ && bootstrap_->debugUi ? "DEBUG" : "USER");
    request->send(200, "text/plain", body);
  });

  server_.on("/api/ui/debug", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!authenticate(request)) return;
    const bool enabled = bootstrap_ && bootstrap_->debugUi;
    request->send(200, "application/json", enabled ? "{\"debug\":true}" : "{\"debug\":false}");
  });

  server_.on("/api/ui/debug", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!authenticate(request)) return;
    if (!bootstrap_ || !persistentStore_) {
      request->send(500, "text/plain", "Storage unavailable");
      return;
    }
    if (!request->hasParam("enabled")) {
      request->send(400, "text/plain", "Missing enabled parameter");
      return;
    }

    String value = request->getParam("enabled")->value();
    value.toLowerCase();
    const bool enabled = value == "1" || value == "true" || value == "yes" || value == "on";
    const bool previous = bootstrap_->debugUi;
    bootstrap_->debugUi = enabled;
    if (!persistentStore_->save(*bootstrap_)) {
      bootstrap_->debugUi = previous;
      request->send(500, "text/plain", "Could not save display mode");
      return;
    }

    if (debugUiChangedCallback_) debugUiChangedCallback_(enabled);
    request->send(200, "text/plain", enabled ? "Debug display enabled" : "End-user display enabled");
  });

  server_.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!authenticate(request)) return;
    if (!configStore_->mounted()) {
      request->send(503, "text/plain", "LittleFS unavailable");
      return;
    }
    request->send(200, "application/json", configStore_->readConfigText());
  });

  server_.on(
      "/api/config", HTTP_POST,
      [this](AsyncWebServerRequest* request) {
        if (!authenticate(request)) return;
      },
      nullptr,
      [this](AsyncWebServerRequest* request, std::uint8_t* data,
             std::size_t len, std::size_t index, std::size_t total) {
        if (!authenticate(request)) return;

        if (index == 0) {
          request->_tempObject = new String();
          static_cast<String*>(request->_tempObject)->reserve(total + 1);
        }
        auto* body = static_cast<String*>(request->_tempObject);
        if (!body) return;
        body->concat(reinterpret_cast<const char*>(data), len);

        if (index + len == total) {
          String error;
          const bool ok = configStore_->saveConfigAtomically(*body, error);
          delete body;
          request->_tempObject = nullptr;

          bool hostnameChanged = false;
          if (ok && wifi_) {
            model::AppConfig savedConfig;
            String loadError;
            if (configStore_->load(savedConfig, loadError) &&
                !savedConfig.hostname.isEmpty() &&
                savedConfig.hostname != wifi_->requestedHostname()) {
              hostnameChanged = true;
            }
          }

          if (!ok) {
            request->send(400, "text/plain", error);
          } else if (hostnameChanged) {
            request->send(200, "text/plain", "Configuration saved; hostname changed; rebooting");
            requestRestart(1200);
          } else {
            request->send(200, "text/plain", "Configuration saved");
          }
        }
      });

  server_.on("/api/files", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!authenticate(request)) return;
    request->send(200, "application/json", configStore_->listFilesJson());
  });

  server_.on("/api/file", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!authenticate(request)) return;
    if (!request->hasParam("path")) {
      request->send(400, "text/plain", "Missing path");
      return;
    }
    String path = request->getParam("path")->value();
    if (!path.startsWith("/")) path = "/" + path;
    if (!config::ConfigStore::safePath(path) || !LittleFS.exists(path)) {
      request->send(404, "text/plain", "Not found");
      return;
    }
    request->send(LittleFS, path, "application/octet-stream", true);
  });

  server_.on("/api/file", HTTP_DELETE, [this](AsyncWebServerRequest* request) {
    if (!authenticate(request)) return;
    if (!request->hasParam("path")) {
      request->send(400, "text/plain", "Missing path");
      return;
    }
    String path = request->getParam("path")->value();
    if (!path.startsWith("/")) path = "/" + path;
    const bool ok = configStore_->removeFile(path);
    request->send(ok ? 200 : 400, "text/plain", ok ? "Deleted" : "Delete refused");
  });

  server_.on(
      "/api/upload", HTTP_POST,
      [this](AsyncWebServerRequest* request) {
        if (!authenticate(request)) return;
        request->send(200, "text/plain", "Uploaded");
      },
      [this](AsyncWebServerRequest* request, const String& filename,
             std::size_t index, std::uint8_t* data, std::size_t len, bool final) {
        if (!bootstrap_ || !request->authenticate(
                bootstrap_->webUsername.c_str(), bootstrap_->webPassword.c_str())) {
          return;
        }
        if (!configStore_->mounted()) return;

        if (index == 0) {
          const String path = normalizeUploadPath(filename);
          if (!config::ConfigStore::safePath(path)) return;
          request->_tempFile = LittleFS.open(path, "w");
        }
        if (len && request->_tempFile) request->_tempFile.write(data, len);
        if (final && request->_tempFile) {
          request->_tempFile.flush();
          request->_tempFile.close();
        }
      });

  server_.on("/api/reload", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!authenticate(request)) return;
    if (reloadCallback_) reloadCallback_();
    request->send(200, "text/plain", "Configuration reload requested");
  });

  server_.on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!authenticate(request)) return;
    request->send(200, "text/plain", "Rebooting");
    requestRestart();
  });

  server_.on("/api/bootstrap/clear", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!authenticate(request)) return;
    if (!persistentStore_->clear()) {
      request->send(500, "text/plain", "Could not clear bootstrap settings");
      return;
    }
    request->send(200, "text/plain", "Bootstrap settings cleared; rebooting");
    requestRestart();
  });

  server_.on("/api/fs/format", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!authenticate(request)) return;
    const bool ok = configStore_->format();
    const String message = ok
        ? String("LittleFS formatted; ") + configStore_->statusText()
        : String("Format failed; ") + configStore_->statusText();
    request->send(ok ? 200 : 500, "text/plain", message);
  });

  server_.on(
      "/api/ota", HTTP_POST,
      [this](AsyncWebServerRequest* request) {
        if (!authenticate(request)) return;
        const bool ok = !Update.hasError();
        request->send(ok ? 200 : 500, "text/plain", ok ? "OTA complete; rebooting" : "OTA failed");
        if (ok) requestRestart(1200);
      },
      [this](AsyncWebServerRequest* request, const String&, std::size_t index,
             std::uint8_t* data, std::size_t len, bool final) {
        if (!bootstrap_ || !request->authenticate(
                bootstrap_->webUsername.c_str(), bootstrap_->webPassword.c_str())) {
          return;
        }

        if (index == 0) {
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            Update.printError(Serial);
          }
        }
        if (len && Update.write(data, len) != len) {
          Update.printError(Serial);
        }
        if (final && !Update.end(true)) {
          Update.printError(Serial);
        }
      });
}

void WebPortal::installCaptiveRoutes() {
  auto captive = [this](AsyncWebServerRequest* request) {
    if (!bootstrap_ || !bootstrap_->configured) {
      request->redirect("/");
    } else if (wifi_->recoveryApActive()) {
      if (!authenticate(request)) return;
      request->redirect("/");
    } else {
      request->send(404, "text/plain", "Not found");
    }
  };

  server_.on("/generate_204", HTTP_GET, captive);
  server_.on("/hotspot-detect.html", HTTP_GET, captive);
  server_.on("/fwlink", HTTP_GET, captive);

  server_.onNotFound([this](AsyncWebServerRequest* request) {
    if (!bootstrap_ || !bootstrap_->configured || wifi_->recoveryApActive()) {
      request->redirect("/");
      return;
    }
    request->send(404, "text/plain", "Not found");
  });
}

String WebPortal::normalizeUploadPath(const String& filename) {
  String clean = filename;
  clean.replace("\\", "/");
  const int slash = clean.lastIndexOf('/');
  if (slash >= 0) clean = clean.substring(slash + 1);
  if (!clean.startsWith("/")) clean = "/" + clean;
  return clean;
}

void WebPortal::requestRestart(std::uint32_t delayMs) {
  restartPending_ = true;
  restartAt_ = millis() + delayMs;
}

void WebPortal::tick() {
  if (restartPending_ &&
      static_cast<std::int32_t>(millis() - restartAt_) >= 0) {
    ESP.restart();
  }
}

}  // namespace homepoint::web
