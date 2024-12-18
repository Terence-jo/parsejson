/*
 * A simple JSON parser written for my own enjoyment. It utilises recursive
 * descent to assemble an internal representation of the JSON. This
 * representation will be a nested "fat struct" with potentially unused fields
 * for its value in different types and a type field denoting which is in use.
 * It will have next and prev pointer fields pointing to the next element in
 * either an object or array (dependent on type) and a child field that will be
 * used for objects or arrays to point to the first item in the chain of items
 * in the array/object. This is essentially the same model as cJSON.
 *
 * This will only support UTF-8 to start.
 */

#pragma once

#include <cmath>
#include <cstddef>
#include <exception>
#include <string>

#ifndef PARSER_NESTING_LIMIT
#define PARSER_NESTING_LIMIT 1000
#endif

namespace parsejson {
// I'll start simple by storing the whole json in a std::string. This will
// probably want to be revisited.
struct ParseBuffer {
  std::string raw_json;
  uint32_t depth = 0;
  size_t pos = 0;
};

enum JSONType {
  j_object,
  j_array,
  j_string,
  j_number,
  j_bool,
  j_null,
};

struct JSONItem {
  JSONItem *next = NULL;
  JSONItem *prev = NULL;
  JSONItem *child = NULL;
  std::string name;
  JSONType type;
  double_t double_val;
  uint64_t uint_val;
  int64_t int_val;
  std::string string_val;
  bool bool_val;
};

class ParseError : public std::exception {
private:
  const char *message;

public:
  ParseError(const char *msg) : message(msg) {}
  const char *what() { return message; }
};

JSONItem *parse_json(ParseBuffer &input_buffer);
void destroy_json(JSONItem *item);

} // namespace parsejson
