#pragma once
#include "clapp.hpp"
#include <cstdlib>
#include <type_traits>

// Implementations of some common types
template <class T>
  requires(std::is_same_v<T, long> || std::is_same_v<T, int> ||
           std::is_same_v<T, short> || std::is_same_v<T, char>)
struct ArgParse<T> {
  static ArgParseReturnT<T> Parse(auto &begin, auto const end) {
    if (begin == end) {
      // We need a string to read
      return ParseError{};
    }
    // Try to parse the integer from the value(s)
    auto ptr = *begin++;
    char *end_ptr;
    auto v = std::strtol(ptr, &end_ptr, 0);
    if (errno == 0 && *end_ptr == '\0') {
      return static_cast<T>(v);
    }
    return ParseError{};
  }
};