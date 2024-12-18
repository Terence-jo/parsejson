#include "parsejson.cpp"
#include <cassert>
#include <fstream>
#include <sstream>
#include <string>

using namespace parsejson;

int main() {
  bool exception_thrown = false;
  ParseBuffer input;

  std::string json = "5.9";
  input.raw_json = json;
  JSONItem *parsed = parse_json(input);
  assert(parsed->type == JSONType::j_number);
  assert(parsed->name.empty());
  assert(parsed->double_val == 5.9);
  destroy_json(parsed);
  input.raw_json = "1five";
  input.pos = 0;
  try {
    parse_json(input);
  } catch (ParseError &pe) {
    exception_thrown = true;
  }
  if (!exception_thrown) {
    destroy_json(parsed);
  }
  assert(exception_thrown);
  exception_thrown = false;

  json = "{\"test\": \"harry\", \"next\": {\"inner\": 6.2, \"again\": null}, "
         "\"arr\": [1.0, 2.0]}";
  input.raw_json = json;
  input.pos = 0;
  input.depth = 0;
  // overwrite JSONItem with no children or linked items
  parsed = parse_json(input);
  assert(parsed->type == JSONType::j_object);
  assert(parsed->name.empty());
  // this assertion failed, child not being assigned. no parse error though
  assert(parsed->child);
  // child access is giving a segfault, bad allocation?
  assert(parsed->child->type == JSONType::j_string);
  assert(parsed->child->name == "test");
  assert(parsed->child->string_val == "harry");
  assert(parsed->child->next);
  destroy_json(parsed);

  input.raw_json = "{\"bad_obj\" \"bad_val\"}";
  input.pos = 0;
  input.depth = 0;
  try {
    parsed = parse_json(input);
  } catch (ParseError &) {
    exception_thrown = true;
  }
  if (!exception_thrown) {
    destroy_json(parsed);
  }
  assert(exception_thrown);

  json = "[\"test\", \"harry\", \"next\", 6.2, \"again\", null]";
  input.raw_json = json;
  input.pos = 0;
  input.depth = 0;
  // overwrite JSONItem with no children or linked items
  parsed = parse_json(input);
  assert(parsed->type == JSONType::j_array);
  assert(parsed->name.empty());
  // this assertion failed, child not being assigned. no parse error though
  assert(parsed->child);
  // child access is giving a segfault, bad allocation?
  assert(parsed->child->type == JSONType::j_string);
  assert(parsed->child->name.empty());
  assert(parsed->child->string_val == "test");
  assert(parsed->child->next);
  destroy_json(parsed);

  input.raw_json = "[\"bad_arr\" \"bad_val\"]";
  input.pos = 0;
  input.depth = 0;
  try {
    parsed = parse_json(input);
  } catch (ParseError &) {
    exception_thrown = true;
  }
  if (!exception_thrown) {
    destroy_json(parsed);
  }
  assert(exception_thrown);

  // try parsing a variety of jsonl
  std::ifstream jsonl_file(
      "~/Downloads/bq-results-20241213-034916-1734061788935.json");
  std::string json_string;
  while (jsonl_file) {
    std::getline(jsonl_file, input.raw_json);
    input.depth = 0;
    input.pos = 0;
    parsed = parse_json(input);
    destroy_json(parsed);
  }
  // try json with huge array
  // std::ifstream json_file("~/misc-data/geojsonCells.geojson");
  // std::stringstream buffer;
  // buffer << jsonl_file.rdbuf();
  // json_string = buffer.str();
  // input.raw_json.swap(json_string);
  // input.depth = 0;
  // input.pos = 0;
  // parsed = parse_json(input);
  // destroy_json(parsed);
}
