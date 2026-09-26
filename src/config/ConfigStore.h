#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "PersistentSettings.h"
#include "../model/Model.h"

namespace homepoint::config {

class ConfigStore {
 public:
  bool begin(bool allowInitialFormat);
  bool mounted() const { return mounted_; }
  String statusText() const;

  bool ensureDefaultConfig();
  bool load(model::AppConfig& config, String& error);
  bool saveConfigAtomically(const String& json, String& error);

  String readConfigText() const;
  bool importLegacyBootstrap(BootstrapSettings& settings) const;

  bool fileExists(const String& path) const;
  String listFilesJson() const;
  bool removeFile(const String& path);
  bool format();

  static bool safePath(const String& path);

 private:
  static constexpr const char* kConfigPath = "/config.json";
  static constexpr const char* kLastGoodPath = "/config.lastgood.json";
  static constexpr const char* kNewPath = "/config.new.json";

  bool mounted_ = false;
  String lastError_ = "not started";

  bool mount();
  bool formatPartition();
  void logUsage() const;

  bool loadFromPath(const char* path, model::AppConfig& config, String& error) const;
  bool parseDocument(JsonDocument& document, model::AppConfig& config, String& error) const;
  bool validateAndParse(const String& json, model::AppConfig& config, String& error) const;

  static String readFile(const char* path);
  static bool writeFile(const char* path, const String& content);
  static bool copyFile(const char* from, const char* to);
};

}  // namespace homepoint::config
