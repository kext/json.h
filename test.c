#define JSON_IMPLEMENTATION
#include "json.h"

#include <stdio.h>

int main()
{
  uint8_t memory[8192];
  size_t l = sizeof(memory);
  JsonArena a[1];
  json_arena_init(a, memory, l);
  //JsonValue v = json_parse("{\"key\":[\"value\",1,true,false,null]}", memory, &l);
  JsonValue v = json_object_new(a);
  json_object_set(a, v, "true", JSON_TRUE);
  json_object_set(a, v, "false", JSON_FALSE);
  json_object_set(a, v, "null", JSON_NULL);
  char s[1024];
  size_t slen = sizeof(s);
  json_stringify(json_object_get(v, "true"), s, &slen);

  printf("%s\n", s);
}
