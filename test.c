#define JSON_IMPLEMENTATION
#include "json.h"

#include <stdio.h>

int main()
{
  uint8_t memory[8192];
  size_t l = sizeof(memory);
  JsonArena arena[1];
  json_arena_init(arena, memory, l);
  //JsonValue object = json_parse("{\"key\":[\"value\",1,true,false,null]}", memory, &l);
  JsonValue array = json_array_new(arena);
  json_array_push(arena, array, json_string("value"));
  json_array_push(arena, array, json_number_new(arena, 1));
  json_array_push(arena, array, JSON_TRUE);
  json_array_push(arena, array, JSON_FALSE);
  json_array_push(arena, array, JSON_NULL);
  JsonValue object = json_object_new(arena);
  json_object_append(arena, object, "key", array);
  char s[1024];
  json_stringify(object, s, sizeof(s));

  printf("%s\n", s);
}
