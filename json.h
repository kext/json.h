// json.h by Lukas Joeressen.
// Licensed under CC0-1.0.
// https://creativecommons.org/publicdomain/zero/1.0/legalcode

#ifndef JSON_HEADER
#define JSON_HEADER

#include <stddef.h>
#include <stdint.h>

// A JsonValue is an annotated pointer to a 4-byte aligned struct.
// That leaves the two least significant bits as tag.
// Also null pointers have special meaning.
// xxx..xx00 Number/Undefined
// xxx..xx01 String/True
// xxx..xx10 Object/False
// xxx..xx11 Array/Null
typedef uintptr_t JsonValue;

// Number type.
typedef int32_t JsonNumber;

// Arena allocator for JSON objects.
typedef struct JsonArena {
  uint8_t *buffer;
  size_t size;
  size_t heap;
  size_t stack;
} JsonArena;

// Initialise an arena allocator for use.
int json_arena_init(JsonArena *a, uint8_t *buffer, size_t length);

// Allocate size bytes from the arena allocator.
void *json_arena_alloc(JsonArena *a, size_t size);

// Parse a zero terminated JSON string into a JsonValue buffer.
// Returns the parsed JsonValue on success and stores the used size of the buffer back into length.
// If an error occurs, 0 is returned and length contains the position of the error.
JsonValue json_parse(const char *json, uint8_t *buffer, size_t *length);

// Convert a JSON value to a string.
// Returns zero on success or an error code.
// Length contains the length of the resulting string if the buffer was large enough.
// If there is space left in the buffer, the string will be zero terminated.
int json_stringify(JsonValue v, char *buffer, size_t *length);

// Check if a JSON value is null.
int json_is_null(JsonValue v);

// Check if a JSON value is either true or false.
int json_is_boolean(JsonValue v);
// Check if a JSON value is true.
int json_is_true(JsonValue v);
// Check if a JSON value is false.
int json_is_false(JsonValue v);

#define JSON_UNDEFINED ((JsonValue) 0)
#define JSON_TRUE ((JsonValue) 1)
#define JSON_FALSE ((JsonValue) 2)
#define JSON_NULL ((JsonValue) 3)

// Check if a JSON value is an number.
int json_is_number(JsonValue v);

// Get the value of a number or 0 if not a number.
JsonNumber json_number_get(JsonValue v);

// Get the data of a string or a null pointer if not a string.
const char *json_string_get(JsonValue v);

// Create a JsonValue from a string.
JsonValue json_string(const char *s);

// Check if a JSON value is an string.
int json_is_string(JsonValue v);

// Check if a JSON value is an object.
int json_is_object(JsonValue v);

// Check if a JSON value is an array.
int json_is_array(JsonValue v);

// Create a new Object.
JsonValue json_object_new(JsonArena *a);

// Set a key to a value.
int json_object_set(JsonArena *a, JsonValue object, const char *key, JsonValue value);

// Get the value of a key.
JsonValue json_object_get(JsonValue object, const char *key);

typedef struct JsonObjectEntry *JsonObjectIterator;
JsonObjectIterator json_object_iterator(JsonValue v);
int json_object_next(JsonObjectIterator *i, const char **key, JsonValue *value);

typedef struct JsonArrayElement *JsonArrayIterator;
JsonArrayIterator json_array_iterator(JsonValue v);
int json_array_next(JsonArrayIterator *i, JsonValue *value);

#endif

#ifdef JSON_IMPLEMENTATION
#undef JSON_IMPLEMENTATION

#include <string.h>

#define JSON__ALIGNMENT (sizeof(void *) < 4 ? 4 : sizeof(void *))

struct JsonObject {
  struct JsonObjectEntry *first;
  struct JsonObjectEntry *last;
};
struct JsonObjectEntry {
  struct JsonObjectEntry *next;
  const char *key;
  JsonValue value;
};

struct JsonArray {
  struct JsonArrayElement *first;
  struct JsonArrayElement *last;
};
struct JsonArrayElement {
  struct JsonArrayElement *next;
  JsonValue value;
};

int json_arena_init(JsonArena *a, uint8_t *buffer, size_t length)
{
  while (((uintptr_t) buffer & (JSON__ALIGNMENT - 1)) && length > 0) {
    buffer += 1;
    length -= 1;
  }
  if (length < JSON__ALIGNMENT) {
    a->buffer = 0;
    a->size = 0;
    a->heap = 0;
    a->stack = 0;
    return -1;
  }
  while (length & (JSON__ALIGNMENT - 1)) {
    length -= 1;
  }
  a->buffer = buffer;
  a->size = length;
  a->heap = 0;
  a->stack = length;
  return 0;
}

void *json_arena_alloc(JsonArena *a, size_t size)
{
  void *r;
  while (size & (JSON__ALIGNMENT - 1)) {
    if (__builtin_add_overflow(size, 1, &size)) return 0;
  }
  if (size > a->stack - a->heap) return 0;
  r = a->buffer + a->heap;
  a->heap += size;
  return r;
}

int json_is_null(JsonValue v)
{
  return v == JSON_NULL;
}

int json_is_boolean(JsonValue v)
{
  return v == JSON_TRUE || v == JSON_FALSE;
}

int json_is_true(JsonValue v)
{
  return v == JSON_TRUE;
}

int json_is_false(JsonValue v)
{
  return v == JSON_FALSE;
}

int json_is_number(JsonValue v)
{
  return (v & 3) == 0 && v > 3;
}

int json_is_string(JsonValue v)
{
  return (v & 3) == 1 && v > 3;
}

int json_is_object(JsonValue v)
{
  return (v & 3) == 2 && v > 3;
}

int json_is_array(JsonValue v)
{
  return (v & 3) == 3 && v > 3;
}

static void *json__pointer(JsonValue v)
{
  return (void *) (v & ~3);
}

JsonNumber json_number_get(JsonValue v)
{
  if (json_is_number(v)) {
    return *((JsonNumber *) json__pointer(v));
  } else {
    return 0;
  }
}

const char *json_string_get(JsonValue v)
{
  if (json_is_string(v)) {
    return json__pointer(v);
  } else {
    return 0;
  }
}

JsonValue json_string(const char *s)
{
  if (!s || ((JsonValue) s) & 3) return JSON_UNDEFINED;
  return (JsonValue) s | 1;
}

JsonObjectIterator json_object_iterator(JsonValue object)
{
  if (!json_is_object(object)) return 0;
  struct JsonObject *o = json__pointer(object);
  return o->first;
}

int json_object_next(JsonObjectIterator *i, const char **key, JsonValue *value)
{
  if (!i || !*i) return 0;
  if (key) *key = (*i)->key;
  if (value) *value = (*i)->value;
  *i = (*i)->next;
  return 1;
}

JsonValue json_object_new(JsonArena *a)
{
  struct JsonObject *o = json_arena_alloc(a, sizeof(*o));
  if (!o) return JSON_UNDEFINED;
  o->first = 0;
  o->last = 0;
  return (JsonValue) o | 2;
}

int json_object_set(JsonArena *a, JsonValue object, const char *key, JsonValue value)
{
  if (!json_is_object(object) || !key) return -1;
  struct JsonObjectEntry *e = json_arena_alloc(a, sizeof(*e));
  if (!e) return -1;
  struct JsonObject *o = json__pointer(object);
  e->key = key;
  e->value = value;
  e->next = o->first;
  o->first = e;
  if (!o->last) o->last = e;
  return 0;
}

JsonValue json_object_get(JsonValue object, const char *key)
{
  if (!json_is_object(object) || !key) return JSON_UNDEFINED;
  JsonObjectIterator i = json_object_iterator(object);
  const char *k;
  JsonValue v;
  while (json_object_next(&i, &k, &v)) {
    if (strcmp(k, key) == 0) return v;
  }
  return JSON_UNDEFINED;
}

static int json__write(const char *text, size_t text_len, char *buffer, size_t *length)
{
  if (text_len >= *length) {
    if (*length > 0) buffer[0] = 0;
    return -1;
  }
  memcpy(buffer, text, text_len);
  buffer[text_len] = 0;
  *length = text_len;
  return 0;
}

#define json__write_literal(text, buffer, length) json__write(text, strlen(text), buffer, length)

int json_stringify(JsonValue v, char *buffer, size_t *length)
{
  if (v == JSON_UNDEFINED || v == JSON_NULL) {
    return json__write_literal("null", buffer, length);
  } else if (v == JSON_TRUE) {
    return json__write_literal("true", buffer, length);
  } else if (v == JSON_FALSE) {
    return json__write_literal("false", buffer, length);
  } else {
    if (*length > 0) buffer[0] = 0;
    return -1;
  }
}

#endif
