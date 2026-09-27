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
  bool saveConfigAtomically(
      const char* json, std::size_t length, String& error);
  bool saveTextFileAtomically(const String& path, const String& text, String& error);
  bool saveTextFileAtomically(
      const String& path, const char* text, std::size_t length, String& error);
  bool installUploadedFileAtomically(
      const String& stagedPath, const String& path, String& error);
  bool setHostname(const String& hostname, String& error);

  String readConfigText() const;
  bool importLegacyBootstrap(BootstrapSettings& settings) const;

  bool fileExists(const String& path) const;
  String listFilesJson() const;
  bool removeFile(const String& path);
  bool format();

  static bool safePath(const String& path);
  static bool editableTextPath(const String& path);
  static bool deletablePath(const String& path);
  static bool uploadablePath(const String& path);

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
  bool validateAndParse(
      const char* json, std::size_t length, model::AppConfig& config, String& error) const;
  bool replaceStagedFileAtomically(
      const String& stagedPath, const String& path, String& error);
  void appendFiles(JsonArray files, const char* dirname, std::uint8_t depth) const;

  static bool internalPath(const String& path);
  static std::uint32_t pathHash(const String& path);
  static String transactionPath(const String& path, const char* suffix);
  static String readFile(const char* path);
  static bool writeFile(const char* path, const String& content);
  static bool writeFile(
      const char* path, const std::uint8_t* content, std::size_t length);
  static bool copyFile(const char* from, const char* to);
};

}  // namespace homepoint::config
