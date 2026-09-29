#include "WebPortal.h"

#include <LittleFS.h>
#include <Update.h>
#include <utility>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <esp_heap_caps.h>

#include "Pages.h"

namespace homepoint::web {
namespace {

constexpr std::size_t kMaxEditableTextBytes = 128u * 1024u;

struct UploadStatus {
  int statusCode;
  bool completed;
  char message[192];
};

void setUploadStatus(UploadStatus* status, int code, const String& message) {
  if (!status) return;
  status->statusCode = code;
  status->completed = true;
  message.toCharArray(status->message, sizeof(status->message));
}

void* allocateRequestBuffer(std::size_t bytes) {
  // ESPAsyncWebServer releases request->_tempObject with free() if a request
  // is aborted. Keep temporary request bodies malloc-compatible; do not store
  // objects allocated with C++ new in _tempObject.
  void* buffer = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buffer) buffer = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
  return buffer;
}

String uploadStagePath(const AsyncWebServerRequest* request) {
  char suffix[24];
  snprintf(
      suffix,
      sizeof(suffix),
      "%lx",
      static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(request)));
  return String("/.__hpm5_upload_") + suffix + ".tmp";
}

String jsonError(const String& message) {
  JsonDocument document;
  document["error"] = message.c_str();
  String body;
  serializeJson(document, body);
  return body;
}

int dashboardStatusCode(config::DashboardResult result) {
  switch (result) {
    case config::DashboardResult::Ok: return 200;
    case config::DashboardResult::InvalidRequest: return 400;
    case config::DashboardResult::StorageUnavailable: return 503;
    case config::DashboardResult::Conflict: return 409;
    case config::DashboardResult::InsufficientMemory: return 507;
    case config::DashboardResult::IoError: return 500;
  }
  return 500;
}

}  // namespace

WebPortal::WebPortal() = default;

void WebPortal::begin(
    config::PersistentSettingsStore* persistentStore,
    config::BootstrapSettings* bootstrap,
    config::ConfigStore* configStore,
    network::WifiManager* wifi,
    network::MqttManager* mqtt,
    core::DisplayCapture* displayCapture,
    std::function<void()> reloadCallback,
    std::function<void(bool)> debugUiChangedCallback) {
  persistentStore_ = persistentStore;
  bootstrap_ = bootstrap;
  configStore_ = configStore;
  wifi_ = wifi;
  mqtt_ = mqtt;
  displayCapture_ = displayCapture;
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
        if (request->contentLength() == 0) {
          request->send(400, "text/plain", "Configuration body must not be empty");
        }
      },
      nullptr,
      [this](AsyncWebServerRequest* request, std::uint8_t* data,
             std::size_t len, std::size_t index, std::size_t total) {
        if (!authenticate(request)) return;
        const bool bodyRangeInvalid = index > total || len > total - index;
        if (total == 0 || total > kMaxEditableTextBytes || bodyRangeInvalid) {
          if (index == 0) {
            request->send(
                total > kMaxEditableTextBytes ? 413 : 400,
                "text/plain",
                total > kMaxEditableTextBytes
                    ? "Configuration is limited to 128 KiB"
                    : "Invalid configuration request body");
          }
          return;
        }

        if (index == 0) {
          auto* body = static_cast<char*>(allocateRequestBuffer(total + 1u));
          if (!body) {
            request->send(507, "text/plain", "Not enough memory to buffer configuration");
            return;
          }
          body[total] = '\0';
          request->_tempObject = body;
        }

        auto* body = static_cast<char*>(request->_tempObject);
        if (!body) return;
        std::memcpy(body + index, data, len);

        if (index + len == total) {
          String error;
          const bool ok = configStore_->saveConfigAtomically(body, total, error);
          free(body);
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

  server_.on("/api/dashboard", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!authenticate(request)) return;
    String body;
    String error;
    const auto result = configStore_->getDashboardJson(body, error);
    if (result != config::DashboardResult::Ok) {
      request->send(dashboardStatusCode(result), "application/json", jsonError(error));
      return;
    }
    request->send(200, "application/json", body);
  });

  server_.on(
      "/api/dashboard/validate", HTTP_POST,
      [this](AsyncWebServerRequest* request) {
        if (!authenticate(request)) return;
        if (request->contentLength() == 0) {
          request->send(
              400, "application/json", jsonError("Dashboard body must not be empty"));
        }
      },
      nullptr,
      [this](AsyncWebServerRequest* request, std::uint8_t* data,
             std::size_t len, std::size_t index, std::size_t total) {
        handleDashboardBody(request, data, len, index, total, false);
      });

  server_.on(
      "/api/dashboard", HTTP_PUT,
      [this](AsyncWebServerRequest* request) {
        if (!authenticate(request)) return;
        if (request->contentLength() == 0) {
          request->send(
              400, "application/json", jsonError("Dashboard body must not be empty"));
        }
      },
      nullptr,
      [this](AsyncWebServerRequest* request, std::uint8_t* data,
             std::size_t len, std::size_t index, std::size_t total) {
        handleDashboardBody(request, data, len, index, total, true);
      });

  server_.on("/api/layouts", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!authenticate(request)) return;
    String body;
    String error;
    const auto result = configStore_->getLayoutsJson(body, error);
    if (result != config::DashboardResult::Ok) {
      request->send(dashboardStatusCode(result), "application/json", jsonError(error));
      return;
    }
    request->send(200, "application/json", body);
  });

  server_.on("/api/layout", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!authenticate(request)) return;
    if (!request->hasParam("file")) {
      request->send(400, "application/json", jsonError("Missing layout file"));
      return;
    }
    const String filename = request->getParam("file")->value();
    String body;
    String error;
    const auto result = configStore_->getLayoutJson(filename, body, error);
    if (result != config::DashboardResult::Ok) {
      request->send(dashboardStatusCode(result), "application/json", jsonError(error));
      return;
    }
    request->send(200, "application/json", body);
  });

  server_.on(
      "/api/layout/validate", HTTP_POST,
      [this](AsyncWebServerRequest* request) {
        if (!authenticate(request)) return;
        if (!request->hasParam("file") || !request->hasParam("name")) {
          request->send(400, "application/json", jsonError("Layout file and name are required"));
          return;
        }
        if (request->contentLength() == 0) {
          request->send(400, "application/json", jsonError("Layout body must not be empty"));
        }
      },
      nullptr,
      [this](AsyncWebServerRequest* request, std::uint8_t* data,
             std::size_t len, std::size_t index, std::size_t total) {
        handleLayoutBody(request, data, len, index, total, false);
      });

  server_.on(
      "/api/layout", HTTP_PUT,
      [this](AsyncWebServerRequest* request) {
        if (!authenticate(request)) return;
        if (!request->hasParam("file") || !request->hasParam("name")) {
          request->send(400, "application/json", jsonError("Layout file and name are required"));
          return;
        }
        if (request->contentLength() == 0) {
          request->send(400, "application/json", jsonError("Layout body must not be empty"));
        }
      },
      nullptr,
      [this](AsyncWebServerRequest* request, std::uint8_t* data,
             std::size_t len, std::size_t index, std::size_t total) {
        handleLayoutBody(request, data, len, index, total, true);
      });

  server_.on(
      "/api/dashboard/source", HTTP_PUT,
      [this](AsyncWebServerRequest* request) {
        if (!authenticate(request)) return;
        if (request->contentLength() == 0) {
          request->send(400, "application/json", jsonError("Dashboard source body must not be empty"));
        }
      },
      nullptr,
      [this](AsyncWebServerRequest* request, std::uint8_t* data,
             std::size_t len, std::size_t index, std::size_t total) {
        handleDashboardMutationBody(request, data, len, index, total, false);
      });

  server_.on(
      "/api/dashboard/upgrade", HTTP_POST,
      [this](AsyncWebServerRequest* request) {
        if (!authenticate(request)) return;
        if (request->contentLength() == 0) {
          request->send(400, "application/json", jsonError("Upgrade body must not be empty"));
        }
      },
      nullptr,
      [this](AsyncWebServerRequest* request, std::uint8_t* data,
             std::size_t len, std::size_t index, std::size_t total) {
        handleDashboardMutationBody(request, data, len, index, total, true);
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
    const String path = normalizePath(request->getParam("path")->value());
    if (!config::ConfigStore::safePath(path) || !LittleFS.exists(path)) {
      request->send(404, "text/plain", "Not found");
      return;
    }
    File file = LittleFS.open(path, "r");
    const bool regularFile = file && !file.isDirectory();
    file.close();
    if (!regularFile) {
      request->send(400, "text/plain", "Path is not a file");
      return;
    }
    const bool download = queryFlag(request, "download");
    request->send(LittleFS, path, contentTypeForPath(path), download);
  });

  server_.on("/api/file", HTTP_DELETE, [this](AsyncWebServerRequest* request) {
    if (!authenticate(request)) return;
    if (!request->hasParam("path")) {
      request->send(400, "text/plain", "Missing path");
      return;
    }
    const String path = normalizePath(request->getParam("path")->value());
    const bool ok = configStore_->removeFile(path);
    request->send(ok ? 200 : 400, "text/plain", ok ? "Deleted" : "Delete refused");
  });

  server_.on(
      "/api/file/text", HTTP_POST,
      [this](AsyncWebServerRequest* request) {
        if (!authenticate(request)) return;
        // ESPAsyncWebServer does not invoke the body callback for an empty
        // request body. Handle that case here so an existing text file can
        // intentionally be cleared and a new empty text file can be created.
        if (request->contentLength() != 0) return;
        if (!request->hasParam("path")) {
          request->send(400, "text/plain", "Missing path");
          return;
        }

        const String path = normalizePath(request->getParam("path")->value());
        if (!config::ConfigStore::editableTextPath(path)) {
          request->send(400, "text/plain", "File type is not editable");
          return;
        }

        String error;
        const bool ok = configStore_->saveTextFileAtomically(path, "", 0, error);
        request->send(
            ok ? 200 : 400,
            "text/plain",
            ok ? "File saved" : error);
      },
      nullptr,
      [this](AsyncWebServerRequest* request, std::uint8_t* data,
             std::size_t len, std::size_t index, std::size_t total) {
        if (!authenticate(request)) return;
        const bool bodyRangeInvalid = index > total || len > total - index;
        if (!request->hasParam("path") || total > kMaxEditableTextBytes ||
            bodyRangeInvalid) {
          if (index == 0) {
            request->send(
                total > kMaxEditableTextBytes ? 413 : 400,
                "text/plain",
                total > kMaxEditableTextBytes
                    ? "Editable text files are limited to 128 KiB"
                    : "Missing path or invalid request body");
          }
          return;
        }

        const String path = normalizePath(request->getParam("path")->value());
        if (!config::ConfigStore::editableTextPath(path)) {
          if (index == 0) request->send(400, "text/plain", "File type is not editable");
          return;
        }

        if (index == 0) {
          auto* body = static_cast<char*>(allocateRequestBuffer(total + 1u));
          if (!body) {
            request->send(507, "text/plain", "Not enough memory to buffer text file");
            return;
          }
          body[total] = '\0';
          request->_tempObject = body;
        }
        auto* body = static_cast<char*>(request->_tempObject);
        if (!body) return;
        std::memcpy(body + index, data, len);

        if (index + len == total) {
          String error;
          const bool ok = configStore_->saveTextFileAtomically(path, body, total, error);
          free(body);
          request->_tempObject = nullptr;

          bool hostnameChanged = false;
          if (ok && path == "/config.json" && wifi_) {
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
            request->send(200, "text/plain", "File saved; hostname changed; rebooting");
            requestRestart(1200);
          } else {
            request->send(200, "text/plain", "File saved");
          }
        }
      });

  server_.on("/api/screen/capture", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!authenticate(request)) return;
    if (!displayCapture_) {
      request->send(503, "text/plain", "Display capture unavailable");
      return;
    }

    std::uint32_t requestId = 0;
    const auto result = displayCapture_->requestSnapshot(requestId);
    if (result == core::DisplayCaptureResult::Busy && requestId != 0) {
      String body = String("{\"id\":") + requestId + "}";
      request->send(202, "application/json", body);
      return;
    }
    if (result != core::DisplayCaptureResult::Ok || requestId == 0) {
      request->send(503, "text/plain", "Display capture unavailable");
      return;
    }

    String body = String("{\"id\":") + requestId + "}";
    request->send(202, "application/json", body);
  });

  server_.on("/api/screen.bmp", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!authenticate(request)) return;
    if (!displayCapture_) {
      request->send(503, "text/plain", "Display capture unavailable");
      return;
    }

    std::uint32_t requestId = 0;
    if (request->hasParam("id")) {
      const String value = request->getParam("id")->value();
      char* end = nullptr;
      const unsigned long parsed = strtoul(value.c_str(), &end, 10);
      requestId = static_cast<std::uint32_t>(parsed);
      if (requestId == 0 || !end || *end != '\0') {
        request->send(400, "text/plain", "Invalid screenshot request id");
        return;
      }
    }

    core::DisplaySnapshot snapshot;
    const auto result = displayCapture_->readSnapshot(requestId, snapshot);
    if (result == core::DisplayCaptureResult::Pending) {
      request->send(202, "text/plain", "Capture pending");
      return;
    }
    if (result == core::DisplayCaptureResult::NotFound) {
      request->send(404, "text/plain", "Screenshot not found");
      return;
    }
    if (result == core::DisplayCaptureResult::Unavailable) {
      request->send(503, "text/plain", "Display capture unavailable");
      return;
    }
    if (result != core::DisplayCaptureResult::Ok || !snapshot.data || !snapshot.size) {
      request->send(500, "text/plain", "Display capture failed");
      return;
    }

    const core::DisplaySnapshot responseSnapshot = snapshot;
    auto* response = request->beginResponse(
        responseSnapshot.mimeType,
        responseSnapshot.size,
        [responseSnapshot](std::uint8_t* buffer, std::size_t maxLen,
                           std::size_t index) -> std::size_t {
          if (!responseSnapshot.data || index >= responseSnapshot.size) return 0;
          const std::size_t count = std::min(
              maxLen, responseSnapshot.size - index);
          std::memcpy(buffer, responseSnapshot.data.get() + index, count);
          return count;
        });
    response->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Content-Disposition", "inline; filename=homepoint-screen.bmp");
    request->send(response);
  });

  server_.on(
      "/api/upload", HTTP_POST,
      [this](AsyncWebServerRequest* request) {
        if (!authenticate(request)) return;
        auto* status = static_cast<UploadStatus*>(request->_tempObject);
        if (!status) {
          request->send(400, "text/plain", "No file was uploaded");
          return;
        }
        const int code = status->completed ? status->statusCode : 500;
        const String message = status->completed
            ? String(status->message)
            : String("Upload did not complete");
        free(status);
        request->_tempObject = nullptr;
        request->send(code, "text/plain", message);
      },
      [this](AsyncWebServerRequest* request, const String& filename,
             std::size_t index, std::uint8_t* data, std::size_t len, bool final) {
        if (!bootstrap_ || !request->authenticate(
                bootstrap_->webUsername.c_str(), bootstrap_->webPassword.c_str())) {
          return;
        }

        UploadStatus* status = static_cast<UploadStatus*>(request->_tempObject);
        if (index == 0) {
          if (status) {
            setUploadStatus(status, 400, "Only one file may be uploaded per request");
            return;
          }
          status = static_cast<UploadStatus*>(calloc(1, sizeof(UploadStatus)));
          if (!status) {
            request->send(507, "text/plain", "Not enough memory to track upload state");
            return;
          }
          status->statusCode = 500;
          snprintf(status->message, sizeof(status->message), "%s", "Upload did not complete");
          request->_tempObject = status;

          if (!configStore_->mounted()) {
            setUploadStatus(status, 503, "LittleFS unavailable");
            return;
          }

          const String targetPath = request->hasParam("path")
              ? normalizePath(request->getParam("path")->value())
              : normalizeUploadPath(filename);
          if (!config::ConfigStore::safePath(targetPath) || targetPath == "/") {
            setUploadStatus(status, 400, "Invalid upload destination");
            return;
          }
          if (!config::ConfigStore::uploadablePath(targetPath)) {
            setUploadStatus(
                status,
                400,
                targetPath == "/config.json"
                    ? "Upload to config.json is disabled; use the JSON editor so the configuration is validated"
                    : "Destination is a managed read-only file");
            return;
          }

          const String stagedPath = uploadStagePath(request);
          LittleFS.remove(stagedPath);
          request->onDisconnect([stagedPath]() {
            LittleFS.remove(stagedPath);
          });
          request->_tempFile = LittleFS.open(stagedPath, "w");
          if (!request->_tempFile) {
            setUploadStatus(status, 507, "Could not create upload staging file");
            return;
          }
        }

        status = static_cast<UploadStatus*>(request->_tempObject);
        if (!status || status->completed) return;

        const String targetPath = request->hasParam("path")
            ? normalizePath(request->getParam("path")->value())
            : normalizeUploadPath(filename);
        const String stagedPath = uploadStagePath(request);

        if (len && (!request->_tempFile || request->_tempFile.write(data, len) != len)) {
          if (request->_tempFile) request->_tempFile.close();
          LittleFS.remove(stagedPath);
          setUploadStatus(status, 507, "Upload write failed or filesystem is full");
          return;
        }

        if (final) {
          if (!request->_tempFile) {
            LittleFS.remove(stagedPath);
            setUploadStatus(status, 500, "Upload staging file is unavailable");
            return;
          }
          request->_tempFile.flush();
          request->_tempFile.close();

          String error;
          if (!configStore_->installUploadedFileAtomically(stagedPath, targetPath, error)) {
            LittleFS.remove(stagedPath);
            setUploadStatus(status, 400, error);
            return;
          }
          setUploadStatus(status, 200, String("Uploaded to ") + targetPath);
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

void WebPortal::handleDashboardBody(
    AsyncWebServerRequest* request,
    std::uint8_t* data,
    std::size_t len,
    std::size_t index,
    std::size_t total,
    bool persist) {
  if (!authenticate(request)) return;

  const bool bodyRangeInvalid = index > total || len > total - index;
  if (total == 0 || total > kMaxEditableTextBytes || bodyRangeInvalid) {
    if (index == 0) {
      request->send(
          total > kMaxEditableTextBytes ? 413 : 400,
          "application/json",
          jsonError(
              total > kMaxEditableTextBytes
                  ? "Dashboard is limited to 128 KiB"
                  : "Invalid dashboard request body"));
    }
    return;
  }

  if (index == 0) {
    auto* body = static_cast<char*>(allocateRequestBuffer(total + 1u));
    if (!body) {
      request->send(
          507, "application/json",
          jsonError("Not enough memory to buffer dashboard"));
      return;
    }
    body[total] = '\0';
    request->_tempObject = body;
  }

  auto* body = static_cast<char*>(request->_tempObject);
  if (!body) return;
  std::memcpy(body + index, data, len);

  if (index + len != total) return;

  String normalized;
  String error;
  const auto result = persist
      ? configStore_->saveDashboardAtomically(body, total, normalized, error)
      : configStore_->validateDashboard(body, total, normalized, error);
  free(body);
  request->_tempObject = nullptr;

  if (result != config::DashboardResult::Ok) {
    request->send(
        dashboardStatusCode(result), "application/json", jsonError(error));
    return;
  }

  if (persist && reloadCallback_) reloadCallback_();
  request->send(200, "application/json", normalized);
}


void WebPortal::handleLayoutBody(
    AsyncWebServerRequest* request,
    std::uint8_t* data,
    std::size_t len,
    std::size_t index,
    std::size_t total,
    bool persist) {
  if (!authenticate(request)) return;
  if (!request->hasParam("file") || !request->hasParam("name")) return;

  const bool bodyRangeInvalid = index > total || len > total - index;
  if (total == 0 || total > kMaxEditableTextBytes || bodyRangeInvalid) {
    if (index == 0) {
      request->send(
          total > kMaxEditableTextBytes ? 413 : 400,
          "application/json",
          jsonError(
              total > kMaxEditableTextBytes
                  ? "Layout is limited to 128 KiB"
                  : "Invalid layout request body"));
    }
    return;
  }

  if (index == 0) {
    auto* body = static_cast<char*>(allocateRequestBuffer(total + 1u));
    if (!body) {
      request->send(
          507, "application/json",
          jsonError("Not enough memory to buffer layout"));
      return;
    }
    body[total] = '\0';
    request->_tempObject = body;
  }

  auto* body = static_cast<char*>(request->_tempObject);
  if (!body) return;
  std::memcpy(body + index, data, len);
  if (index + len != total) return;

  const String filename = request->getParam("file")->value();
  const String name = request->getParam("name")->value();
  String normalized;
  String error;
  bool activeLayout = false;
  const auto result = persist
      ? configStore_->saveLayoutAtomically(
            filename, name, body, total, normalized, activeLayout, error)
      : configStore_->validateLayout(
            filename, name, body, total, normalized, error);
  free(body);
  request->_tempObject = nullptr;

  if (result != config::DashboardResult::Ok) {
    request->send(
        dashboardStatusCode(result), "application/json", jsonError(error));
    return;
  }

  if (persist && activeLayout && reloadCallback_) reloadCallback_();
  request->send(200, "application/json", normalized);
}

void WebPortal::handleDashboardMutationBody(
    AsyncWebServerRequest* request,
    std::uint8_t* data,
    std::size_t len,
    std::size_t index,
    std::size_t total,
    bool upgrade) {
  if (!authenticate(request)) return;

  const bool bodyRangeInvalid = index > total || len > total - index;
  if (total == 0 || total > kMaxEditableTextBytes || bodyRangeInvalid) {
    if (index == 0) {
      request->send(
          total > kMaxEditableTextBytes ? 413 : 400,
          "application/json",
          jsonError(
              total > kMaxEditableTextBytes
                  ? "Request is limited to 128 KiB"
                  : "Invalid request body"));
    }
    return;
  }

  if (index == 0) {
    auto* body = static_cast<char*>(allocateRequestBuffer(total + 1u));
    if (!body) {
      request->send(
          507, "application/json",
          jsonError("Not enough memory to buffer request"));
      return;
    }
    body[total] = '\0';
    request->_tempObject = body;
  }

  auto* body = static_cast<char*>(request->_tempObject);
  if (!body) return;
  std::memcpy(body + index, data, len);
  if (index + len != total) return;

  String response;
  String error;
  const auto result = upgrade
      ? configStore_->upgradeDashboardSchema(body, total, response, error)
      : configStore_->setDashboardSource(body, total, response, error);
  free(body);
  request->_tempObject = nullptr;

  if (result != config::DashboardResult::Ok) {
    request->send(
        dashboardStatusCode(result), "application/json", jsonError(error));
    return;
  }

  if (reloadCallback_) reloadCallback_();
  request->send(200, "application/json", response);
}

String WebPortal::normalizeUploadPath(const String& filename) {
  String clean = filename;
  clean.replace("\\", "/");
  const int slash = clean.lastIndexOf('/');
  if (slash >= 0) clean = clean.substring(slash + 1);
  if (!clean.startsWith("/")) clean = "/" + clean;
  return clean;
}

String WebPortal::normalizePath(const String& path) {
  String clean = path;
  clean.replace("\\", "/");
  if (!clean.startsWith("/")) clean = "/" + clean;
  return clean;
}

String WebPortal::contentTypeForPath(const String& path) {
  String lower = path;
  lower.toLowerCase();
  if (lower.endsWith(".jpg") || lower.endsWith(".jpeg")) return "image/jpeg";
  if (lower.endsWith(".png")) return "image/png";
  if (lower.endsWith(".gif")) return "image/gif";
  if (lower.endsWith(".bmp")) return "image/bmp";
  if (lower.endsWith(".svg")) return "image/svg+xml";
  if (lower.endsWith(".json")) return "application/json; charset=utf-8";
  if (lower.endsWith(".css")) return "text/css; charset=utf-8";
  if (lower.endsWith(".js")) return "application/javascript; charset=utf-8";
  if (lower.endsWith(".html") || lower.endsWith(".htm")) return "text/html; charset=utf-8";
  if (lower.endsWith(".txt") || lower.endsWith(".md") ||
      lower.endsWith(".csv") || lower.endsWith(".log")) {
    return "text/plain; charset=utf-8";
  }
  if (lower.endsWith(".vlw")) return "application/octet-stream";
  return "application/octet-stream";
}

bool WebPortal::queryFlag(AsyncWebServerRequest* request, const char* name) {
  if (!request || !request->hasParam(name)) return false;
  String value = request->getParam(name)->value();
  value.toLowerCase();
  return value.isEmpty() || value == "1" || value == "true" ||
         value == "yes" || value == "on";
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
