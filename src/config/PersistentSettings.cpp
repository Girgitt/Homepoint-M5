#include "PersistentSettings.h"

#include <EEPROM.h>
#include <cstdio>

#include "Crc16.h"

namespace homepoint::config {

bool PersistentSettingsStore::begin() {
  return EEPROM.begin(kEepromSize);
}

PersistentLoadResult PersistentSettingsStore::load(BootstrapSettings& settings) {
  RecordHeader header{};
  EEPROM.get(0, header);

  if (header.magic == 0xFFFFFFFFu || header.magic == 0u) {
    return PersistentLoadResult::Uninitialized;
  }
  if (header.magic != kMagic) {
    // Erased EEPROM is handled above. A different non-erased magic means
    // there is data here, but it is not a valid Homepoint-M5 record.
    return PersistentLoadResult::Corrupt;
  }

  switch (header.schemaVersion) {
    case 1:
      return loadV1(settings) ? PersistentLoadResult::Ok : PersistentLoadResult::Corrupt;
    case 2:
      return loadV2(settings) ? PersistentLoadResult::Ok : PersistentLoadResult::Corrupt;
    default:
      return PersistentLoadResult::UnsupportedVersion;
  }
}

bool PersistentSettingsStore::loadV1(BootstrapSettings& settings) {
  RecordV1 record{};
  EEPROM.get(0, record);
  if (!validateV1(record)) return false;

  settings.configured = record.payload.configured == 1u;
  settings.wifiSsid = record.payload.wifiSsid;
  settings.wifiPassword = record.payload.wifiPassword;
  settings.hostname = record.payload.hostname;
  settings.webUsername = record.payload.webUsername;
  settings.webPassword = record.payload.webPassword;
  // V1 predates selectable UI modes. Existing installations migrate to the
  // end-user renderer by default; the web UI can switch back to Debug mode.
  settings.debugUi = false;
  return true;
}

bool PersistentSettingsStore::loadV2(BootstrapSettings& settings) {
  RecordV2 record{};
  EEPROM.get(0, record);
  if (!validateV2(record)) return false;

  settings.configured = record.payload.configured == 1u;
  settings.wifiSsid = record.payload.wifiSsid;
  settings.wifiPassword = record.payload.wifiPassword;
  settings.hostname = record.payload.hostname;
  settings.webUsername = record.payload.webUsername;
  settings.webPassword = record.payload.webPassword;
  settings.debugUi = record.payload.debugUi == 1u;
  return true;
}

bool PersistentSettingsStore::validateV1(const RecordV1& record) const {
  if (record.header.magic != kMagic) return false;
  if (record.header.schemaVersion != 1u) return false;
  if (record.header.payloadSize != sizeof(PayloadV1)) return false;

  const auto calculated = homepoint::core::crc16Ibm(
      reinterpret_cast<const std::uint8_t*>(&record),
      offsetof(RecordV1, crc16));
  return calculated == record.crc16;
}

bool PersistentSettingsStore::validateV2(const RecordV2& record) const {
  if (record.header.magic != kMagic) return false;
  if (record.header.schemaVersion != 2u) return false;
  if (record.header.payloadSize != sizeof(PayloadV2)) return false;
  if (record.payload.debugUi > 1u) return false;

  const auto calculated = homepoint::core::crc16Ibm(
      reinterpret_cast<const std::uint8_t*>(&record),
      offsetof(RecordV2, crc16));
  return calculated == record.crc16;
}

void PersistentSettingsStore::copyString(
    char* destination, std::size_t destinationSize, const String& source) {
  if (!destination || destinationSize == 0) return;
  std::snprintf(destination, destinationSize, "%s", source.c_str());
  destination[destinationSize - 1] = '\0';
}

bool PersistentSettingsStore::save(const BootstrapSettings& settings) {
  RecordV2 record{};
  record.header.magic = kMagic;
  record.header.schemaVersion = kCurrentSchemaVersion;
  record.header.payloadSize = sizeof(PayloadV2);
  record.payload.configured = settings.configured ? 1u : 0u;

  copyString(record.payload.wifiSsid, sizeof(record.payload.wifiSsid), settings.wifiSsid);
  copyString(record.payload.wifiPassword, sizeof(record.payload.wifiPassword), settings.wifiPassword);
  copyString(record.payload.hostname, sizeof(record.payload.hostname), settings.hostname);
  copyString(record.payload.webUsername, sizeof(record.payload.webUsername), settings.webUsername);
  copyString(record.payload.webPassword, sizeof(record.payload.webPassword), settings.webPassword);
  record.payload.debugUi = settings.debugUi ? 1u : 0u;

  record.crc16 = homepoint::core::crc16Ibm(
      reinterpret_cast<const std::uint8_t*>(&record),
      offsetof(RecordV2, crc16));

  EEPROM.put(0, record);
  return EEPROM.commit();
}

bool PersistentSettingsStore::clear() {
  for (std::size_t i = 0; i < kEepromSize; ++i) {
    EEPROM.write(i, 0xFF);
  }
  return EEPROM.commit();
}

}  // namespace homepoint::config
