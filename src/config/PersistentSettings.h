#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

namespace homepoint::config {

struct BootstrapSettings {
  bool configured = false;
  String wifiSsid;
  String wifiPassword;
  // Legacy migration/recovery fallback only. Runtime hostname is owned by config.json.
  String hostname;
  String webUsername = "admin";
  String webPassword;
  bool debugUi = false;
};

enum class PersistentLoadResult {
  Ok,
  Uninitialized,
  Corrupt,
  UnsupportedVersion,
  IoError,
};

class PersistentSettingsStore {
 public:
  bool begin();
  PersistentLoadResult load(BootstrapSettings& settings);
  bool save(const BootstrapSettings& settings);
  bool clear();

  static constexpr std::uint32_t kMagic = 0x48504D35u;  // "HPM5"
  static constexpr std::uint16_t kCurrentSchemaVersion = 2;

 private:
  static constexpr std::size_t kEepromSize = 1024;

#pragma pack(push, 1)
  struct RecordHeader {
    std::uint32_t magic;
    std::uint16_t schemaVersion;
    std::uint16_t payloadSize;
  };

  struct PayloadV1 {
    std::uint8_t configured;
    char wifiSsid[33];
    char wifiPassword[65];
    char hostname[33];
    char webUsername[33];
    char webPassword[65];
  };

  struct RecordV1 {
    RecordHeader header;
    PayloadV1 payload;
    std::uint16_t crc16;
  };

  struct PayloadV2 {
    std::uint8_t configured;
    char wifiSsid[33];
    char wifiPassword[65];
    char hostname[33];
    char webUsername[33];
    char webPassword[65];
    std::uint8_t debugUi;
  };

  struct RecordV2 {
    RecordHeader header;
    PayloadV2 payload;
    std::uint16_t crc16;
  };
#pragma pack(pop)

  bool loadV1(BootstrapSettings& settings);
  bool loadV2(BootstrapSettings& settings);
  bool validateV1(const RecordV1& record) const;
  bool validateV2(const RecordV2& record) const;
  static void copyString(char* destination, std::size_t destinationSize, const String& source);
};

}  // namespace homepoint::config
