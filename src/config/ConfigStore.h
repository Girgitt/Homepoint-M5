#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "PersistentSettings.h"
#include "DashboardCodec.h"
#include "LayoutCodec.h"
#include "../model/Model.h"

namespace homepoint::config {

enum class DashboardResult {
  Ok,
  InvalidRequest,
  StorageUnavailable,
  Conflict,
  InsufficientMemory,
  IoError,
};

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
  DashboardResult getDashboardJson(String& json, String& error) const;
  DashboardResult validateDashboard(
      const char* json, std::size_t length,
      String& normalizedJson, String& error) const;
  DashboardResult saveDashboardAtomically(
      const char* json, std::size_t length,
      String& normalizedJson, String& error);
  DashboardResult getLayoutsJson(String& json, String& error) const;
  DashboardResult getLayoutJson(
      const String& filename, String& json, String& error) const;
  DashboardResult validateLayout(
      const String& filename, const String& name,
      const char* json, std::size_t length,
      String& normalizedJson, String& error) const;
  DashboardResult saveLayoutAtomically(
      const String& filename, const String& name,
      const char* json, std::size_t length,
      String& normalizedJson, bool& activeLayout, String& error);
  DashboardResult setDashboardSource(
      const char* json, std::size_t length, String& responseJson, String& error);
  DashboardResult upgradeDashboardSchema(
      const char* json, std::size_t length, String& responseJson, String& error);
  bool isLayoutActive(const String& filename) const;
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
  static bool validLayoutFilename(const String& filename);

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
  bool loadEditableDocument(
      JsonDocument& document, model::AppConfig& config,
      bool& recoveredFromLastGood, String& error) const;
  bool loadLayoutDocument(
      const String& filename, LayoutDocument& layout,
      bool& recoveredFromLastGood, String& error) const;
  bool loadLayoutFromPath(
      const String& path, LayoutDocument& layout, String& error) const;
  bool saveLayoutDocumentAtomically(
      const String& filename, const LayoutDocument& layout, String& error);
  bool writeConfigDocumentAtomically(JsonDocument& document, String& error);
  static bool looksLikeMemoryError(const String& error);
  bool replaceStagedFileAtomically(
      const String& stagedPath, const String& path, String& error);
  void appendFiles(JsonArray files, const char* dirname, std::uint8_t depth) const;

  static bool internalPath(const String& path);
  static std::uint32_t pathHash(const String& path);
  static String transactionPath(const String& path, const char* suffix);
  static String layoutPath(const String& filename);
  static String layoutLastGoodPath(const String& filename);
  static String layoutNewPath(const String& filename);
  static String readFile(const char* path);
  static bool writeFile(const char* path, const String& content);
  static bool writeFile(
      const char* path, const std::uint8_t* content, std::size_t length);
  static bool copyFile(const char* from, const char* to);
};

}  // namespace homepoint::config
