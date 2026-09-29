#pragma once

#include <cstddef>
#include <cstdint>
#include <cctype>
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

  char operator[](std::size_t index) const { return value_[index]; }

  bool startsWith(const char* prefix) const {
    const std::string p = prefix ? prefix : "";
    return value_.rfind(p, 0) == 0;
  }

  bool endsWith(const char* suffix) const {
    const std::string s = suffix ? suffix : "";
    return s.size() <= value_.size() &&
           value_.compare(value_.size() - s.size(), s.size(), s) == 0;
  }

  int indexOf(char needle) const {
    const auto pos = value_.find(needle);
    return pos == std::string::npos ? -1 : static_cast<int>(pos);
  }

  int indexOf(const char* needle) const {
    const auto pos = value_.find(needle ? needle : "");
    return pos == std::string::npos ? -1 : static_cast<int>(pos);
  }

  void trim() {
    std::size_t first = 0;
    while (first < value_.size() &&
           std::isspace(static_cast<unsigned char>(value_[first]))) ++first;
    std::size_t last = value_.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(value_[last - 1]))) --last;
    value_ = value_.substr(first, last - first);
  }

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

  friend String operator+(const String& lhs, const String& rhs) {
    return String(lhs.value_ + rhs.value_);
  }
  friend String operator+(const String& lhs, const char* rhs) {
    return String(lhs.value_ + (rhs ? rhs : ""));
  }
  friend String operator+(const char* lhs, const String& rhs) {
    return String((lhs ? lhs : "") + rhs.value_);
  }

 private:
  std::string value_;
};
