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
  std::size_t size() const { return value_.size(); }
  bool isEmpty() const { return value_.empty(); }

  bool reserve(std::size_t size) {
    try {
      value_.reserve(size);
      return true;
    } catch (...) {
      return false;
    }
  }

  bool concat(const char* data, std::size_t size) {
    if (!data) return true;
    try {
      value_.append(data, size);
      return true;
    } catch (...) {
      return false;
    }
  }

  bool concat(const char* data) {
    if (!data) return true;
    try {
      value_ += data;
      return true;
    } catch (...) {
      return false;
    }
  }

  bool concat(char value) {
    try {
      value_.push_back(value);
      return true;
    } catch (...) {
      return false;
    }
  }

  String& operator+=(char value) {
    value_.push_back(value);
    return *this;
  }

  String& operator+=(const char* value) {
    if (value) value_ += value;
    return *this;
  }

  bool operator==(const String& other) const { return value_ == other.value_; }
  bool operator!=(const String& other) const { return !(*this == other); }
  bool operator==(const char* other) const {
    return value_ == (other ? other : "");
  }

 private:
  std::string value_;
};
