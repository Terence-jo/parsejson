/*
 * A simple JSON parser written for my own enjoyment. It utilises recursive
 * descent to assemble an internal representation of the JSON. This
 * representation will be a nested union struct with potentially unused fields
 * for its value in different types and a type field denoting which is in use.
 * It will have next and prev pointer fields pointing to the next element in
 * either an object or array (dependent on type) and a child field that will be
 * used for objects or arrays to point to the first item in the chain of items
 * in the array/object. This is essentially the same model as cJSON.
 *
 *
 * This will only support UTF-8 to start. Otherwise it follows RFC7159.
 */

#include "parsejson.h"
#include <cassert>
#include <cfloat>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace parsejson {

void skip_whitespace(ParseBuffer &input_buffer) {
  while (std::isspace(input_buffer.raw_json[input_buffer.pos])) {
    input_buffer.pos++;
  }
}

bool can_read(ParseBuffer &input_buffer, size_t bytes) {
  return (!input_buffer.raw_json.empty() &&
          (input_buffer.pos + bytes <= input_buffer.raw_json.size()));
}

// the parsing functions will return values and throw exceptions.

double parse_number(ParseBuffer &input_buffer) {
  // currently allows leading zeros, which does not technically satisfy RFC7159
  char *endptr;
  const char *startptr = &input_buffer.raw_json.data()[input_buffer.pos];
  double double_val = strtod(startptr, &endptr);
  if (endptr == &input_buffer.raw_json[input_buffer.pos]) {
    char msg[100];
    std::snprintf(msg, 100, "bad double at pos: %zu", input_buffer.pos);
    throw ParseError((const char *)msg);
  }
  input_buffer.pos += endptr - startptr;
  return double_val;
}

std::string parse_string(ParseBuffer &input_buffer) {
  std::string out_str;
  while (input_buffer.raw_json[input_buffer.pos] != '\"' &&
         input_buffer.pos < input_buffer.raw_json.size()) {
    if (input_buffer.raw_json[input_buffer.pos] != '\\') {
      out_str.append(1, input_buffer.raw_json[input_buffer.pos]);
      input_buffer.pos++;
      continue;
    }
    if (input_buffer.raw_json.size() - input_buffer.pos < 2) {
      char msg[100];
      std::snprintf(msg, 100,
                    "prematurely terminated escape sequence at pos: %zu",
                    input_buffer.pos);
      throw ParseError((const char *)msg);
    }
    switch (input_buffer.raw_json[input_buffer.pos + 1]) {
    case 'b':
      out_str.append(1, '\b');
      break;
    case 'f':
      out_str.append(1, '\f');
      break;
    case 'n':
      out_str.append(1, '\n');
      break;
    case 'r':
      out_str.append(1, '\r');
      break;
    case 't':
      out_str.append(1, '\t');
      break;
    case '\"':
    case '\\':
    case '/':
      out_str.append(1, input_buffer.raw_json[input_buffer.pos + 1]);
    default:
      char msg[100];
      std::snprintf(msg, 100, "unknown escape sequence at pos: %zu",
                    input_buffer.pos);
      throw ParseError((const char *)msg);
    }
    input_buffer.pos += 2;
  }
  input_buffer.pos++; // consume closing '"'
  return out_str;
}

JSONItem *parse_json(ParseBuffer &input_buffer);

// TODO: handle premature termination of the buffer
JSONItem *parse_array(ParseBuffer &input_buffer) {
  input_buffer.depth++;
  if (input_buffer.depth > PARSER_NESTING_LIMIT) {
    char msg[100];
    std::snprintf(msg, 100,
                  "max nesting limit of %d exceeded in array at pos: %zu",
                  PARSER_NESTING_LIMIT, input_buffer.pos);
    throw ParseError((const char *)msg);
  }
  skip_whitespace(input_buffer);
  if (input_buffer.raw_json[input_buffer.pos] == ']') {
    // empty array. parent will be array but have no children.
    return NULL;
  }
  JSONItem *head = NULL;
  JSONItem *current = NULL;
  while ((input_buffer.raw_json[input_buffer.pos] != ']') &&
         input_buffer.pos < input_buffer.raw_json.size()) {
    skip_whitespace(input_buffer);
    if (!head) {
      current = head = parse_json(input_buffer);
    } else {
      JSONItem *new_item = parse_json(input_buffer);
      current->next = new_item;
      new_item->prev = current;
      current = new_item;
    }
    skip_whitespace(input_buffer);
    if (input_buffer.raw_json[input_buffer.pos] == ']') {
      break;
    }
    if (input_buffer.raw_json[input_buffer.pos] != ',') {
      char msg[100];
      std::snprintf(msg, 100, "invalid array continuation at pos: %zu",
                    input_buffer.pos);
      throw ParseError((const char *)msg);
    }
    input_buffer.pos++;
  }
  if (input_buffer.raw_json[input_buffer.pos] != ']') {
    const char *msg = "unexpected EOF";
    throw ParseError(msg);
  }
  input_buffer.pos++;
  input_buffer.depth--;
  return head;
}

JSONItem *parse_object(ParseBuffer &input_buffer) {
  input_buffer.depth++;
  if (input_buffer.depth > PARSER_NESTING_LIMIT) {
    char msg[100];
    std::snprintf(msg, 100,
                  "max nesting limit of %d exceeded in object at pos: %zu",
                  PARSER_NESTING_LIMIT, input_buffer.pos);
    throw ParseError((const char *)msg);
  }
  skip_whitespace(input_buffer);
  if (input_buffer.raw_json[input_buffer.pos] == '}') {
    // empty object. parent will be object but have no children.
    return NULL;
  }
  JSONItem *head = NULL;
  JSONItem *current = NULL;
  while ((input_buffer.raw_json[input_buffer.pos] != '}') &&
         input_buffer.pos < input_buffer.raw_json.size()) {
    // consume name
    skip_whitespace(input_buffer);
    if (!can_read(input_buffer, 1) ||
        input_buffer.raw_json[input_buffer.pos] != '\"') {
      char msg[100];
      std::snprintf(msg, 100, "bad object member name at pos: %zu",
                    input_buffer.pos);
      throw ParseError((const char *)msg);
    }
    input_buffer.pos++; // consume opening '"'
    std::string name = parse_string(input_buffer);
    skip_whitespace(input_buffer);
    if (!can_read(input_buffer, 1) ||
        input_buffer.raw_json[input_buffer.pos] != ':') {
      char msg[100];
      std::snprintf(msg, 100, "bad object name-value separation at pos: %zu",
                    input_buffer.pos);
      throw ParseError((const char *)msg);
    }
    input_buffer.pos++;
    skip_whitespace(input_buffer);

    if (!head) {
      current = head = parse_json(input_buffer);
    } else {
      JSONItem *new_item = parse_json(input_buffer);
      current->next = new_item;
      new_item->prev = current;
      current = new_item;
    }
    current->name.swap(name);
    skip_whitespace(input_buffer);
    if (input_buffer.raw_json[input_buffer.pos] == '}') {
      break;
    }
    if (!can_read(input_buffer, 1) ||
        input_buffer.raw_json[input_buffer.pos] != ',') {
      char msg[100];
      std::snprintf(msg, 100, "invalid object continuation at pos: %zu",
                    input_buffer.pos);
      throw ParseError((const char *)msg);
    }
    input_buffer.pos++;
  }
  if (input_buffer.raw_json[input_buffer.pos] != '}') {
    char *msg = (char *)"unexpected EOF";
    throw ParseError(msg);
  }
  input_buffer.pos++;
  input_buffer.depth--;
  return head;
}

void destroy_json(JSONItem *item) {
  // recurse into children, but iterate to the end of the JSON to avoid stack
  // overflow. depth is limited, length is not
  JSONItem *next = NULL;
  while (item) {
    next = item->next;
    if (item->child) {
      destroy_json(item->child);
    }
    delete item;
    item = next;
  }
}

JSONItem *parse_json(ParseBuffer &input_buffer) {
  JSONItem *item = new JSONItem();
  skip_whitespace(input_buffer);

  try {
    if (can_read(input_buffer, 1) &&
        input_buffer.raw_json[input_buffer.pos] == '\"') {
      item->type = JSONType::j_string;
      input_buffer.pos++;
      item->string_val = parse_string(input_buffer);
    } else if (can_read(input_buffer, 1) &&
               ((input_buffer.raw_json[input_buffer.pos] == '-') ||
                (input_buffer.raw_json[input_buffer.pos] == '+') ||
                std::isdigit(input_buffer.raw_json[input_buffer.pos]))) {
      item->type = JSONType::j_number;
      item->double_val = parse_number(input_buffer);
    } else if (can_read(input_buffer, 1) &&
               input_buffer.raw_json[input_buffer.pos] == '[') {
      item->type = JSONType::j_array;
      input_buffer.pos++;
      item->child = parse_array(input_buffer);
    } else if (can_read(input_buffer, 1) &&
               input_buffer.raw_json[input_buffer.pos] == '{') {
      item->type = JSONType::j_object;
      input_buffer.pos++;
      item->child = parse_object(input_buffer);
    } else if (can_read(input_buffer, 4) &&
               input_buffer.raw_json.compare(input_buffer.pos, 4, "null") ==
                   0) {
      item->type = JSONType::j_null;
      input_buffer.pos += 4;
    } else if (can_read(input_buffer, 1) &&
               input_buffer.raw_json.compare(input_buffer.pos, 4, "true") ==
                   0) {
      item->type = JSONType::j_bool;
      item->bool_val = true;
      input_buffer.pos += 4;
    } else if (can_read(input_buffer, 1) &&
               input_buffer.raw_json.compare(input_buffer.pos, 5, "false") ==
                   0) {
      item->type = JSONType::j_bool;
      item->bool_val = false;
      input_buffer.pos += 5;
    } else {
      throw ParseError("invalid json");
    }
  } catch (ParseError &pe) {
    destroy_json(item);
    const char *msg = pe.what();
    throw ParseError(msg);
  }
  // the potentially recursive process above should process all available
  // valid JSON. this may be followed by an arbitrary amount of whitespace,
  // at which point we should be at the end of the buffer.
  skip_whitespace(input_buffer);
  if ((input_buffer.depth == 0) &&
      (input_buffer.pos != input_buffer.raw_json.size())) {
    const char *msg = "trailing junk";
    throw ParseError(msg);
  }
  return item;
}

} // namespace parsejson
