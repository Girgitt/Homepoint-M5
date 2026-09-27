#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Minimal Arduino compatibility surface for native tests.
// Production firmware never includes this file: platformio.ini adds this
// directory only to the native test environment.
class String {
 public:
  String() = default;
  String(const char* value) : value_(value ? value : "") {}
  String(const std::string& value) : value_(value) {}

  String& operator=(const char* value) {
    value_ = value ? value : "";
    return *this;
  }

  const char* c_str() const { return value_.c_str(); }
  std::size_t length() const { return value_.length(); }
  bool isEmpty() const { return value_.empty(); }

  bool operator==(const String& other) const { return value_ == other.value_; }
  bool operator!=(const String& other) const { return !(*this == other); }
  bool operator==(const char* other) const {
    return value_ == (other ? other : "");
  }

 private:
  std::string value_;
};
