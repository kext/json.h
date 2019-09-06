// json.h by Lukas Joeressen.
// Licensed under CC0-1.0.
// https://creativecommons.org/publicdomain/zero/1.0/legalcode

#ifndef JSON_HEADER
#define JSON_HEADER

#include <stddef.h>
#include <stdint.h>

// Number type.
typedef int32_t JsonNumber;

// A JsonValue is an annotated pointer to a 4-byte aligned struct.
// That leaves the two least significant bits as tag.
// Also null pointers have special meaning.
// xxx..xx00 Number/Undefined
// xxx..xx01 String/True
// xxx..xx10 Object/False
// xxx..xx11 Array/Null
typedef uintptr_t JsonValue;

// True if JsonNumber is a floating point type.
#define JSON_NUMBER_IS_FLOATING_POINT_TYPE ((JsonNumber) 0.1 > 0)
// True if JsonNumber is a signed number type.
#define JSON_NUMBER_IS_SIGNED_TYPE ((JsonNumber) -1 < 0)

// Arena allocator for JSON objects.
typedef struct JsonArena {
  uint8_t *buffer;
  size_t size;
  size_t heap;
  size_t stack;
} JsonArena;

// Initialise an arena allocator for use.
int json_arena_init(JsonArena *arena, uint8_t *buffer, size_t length);

// Allocate size bytes from the arena allocator.
void *json_arena_alloc(JsonArena *arena, size_t size);

// Parse a zero terminated JSON string into a JsonValue buffer.
// Returns the parsed JsonValue on success and stores the used size of the buffer back into length.
// If an error occurs, 0 is returned and length contains the position of the error.
JsonValue json_parse(const char *json, uint8_t *buffer, size_t *length);

// Convert a JSON value to a string.
// Returns the number of bytes used excluding the zero terminator or 0 on error.
size_t json_stringify(JsonValue v, char *buffer, size_t length);

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

// Create a new number.
JsonValue json_number_new(JsonArena *arena, JsonNumber number);

// Create a new object.
JsonValue json_object_new(JsonArena *arena);

// Set a key to a value.
int json_object_set(JsonArena *arena, JsonValue object, const char *key, JsonValue value);

// Append a value to an object.
// This is faster than set, but may only be used if the object does not contain the key.
int json_object_append(JsonArena *arena, JsonValue object, const char *key, JsonValue value);

// Get the value of a key.
JsonValue json_object_get(JsonValue object, const char *key);

// Create a new array.
JsonValue json_array_new(JsonArena *arena);

// Append a value to an array.
int json_array_push(JsonArena *arena, JsonValue array, JsonValue value);

// Get the element at the given index.
JsonValue json_array_get(JsonValue array, int index);

// Opaque iterator over an object.
typedef struct JsonObjectEntry *JsonObjectIterator;
// Get an iterator for an object.
JsonObjectIterator json_object_iterator(JsonValue v);
// Advance an iterator. Returns 1 if a key and value were produced, 0 at the end.
// key or value may be null if unneeded.
int json_object_next(JsonObjectIterator *i, const char **key, JsonValue *value);

// Opaque iterator over an array.
typedef struct JsonArrayElement *JsonArrayIterator;
// Get an iterator for an array.
JsonArrayIterator json_array_iterator(JsonValue v);
// Advance an iterator. Returns 1 if a value was produced, 0 at the end.
// value may be null if unneeded.
int json_array_next(JsonArrayIterator *i, JsonValue *value);

#endif

#ifdef JSON_IMPLEMENTATION
#undef JSON_IMPLEMENTATION

#include <string.h>

// We use the builtin overflow checks.
// If your compiler does not have them, you must supply them yourself.
#define json__builtin_add_overflow __builtin_add_overflow
#define json__builtin_sub_overflow __builtin_sub_overflow
#define json__builtin_mul_overflow __builtin_mul_overflow

#define JSON__ALIGNMENT (sizeof(void *) < 4 ? 4 : sizeof(void *))

// Json Objects.
struct JsonObject {
  struct JsonObjectEntry *first;
  struct JsonObjectEntry *last;
};
struct JsonObjectEntry {
  struct JsonObjectEntry *next;
  const char *key;
  JsonValue value;
};

// Json Arrays.
struct JsonArray {
  struct JsonArrayElement *first;
  struct JsonArrayElement *last;
};
struct JsonArrayElement {
  struct JsonArrayElement *next;
  JsonValue value;
};

int json_arena_init(JsonArena *arena, uint8_t *buffer, size_t length)
{
  while (((uintptr_t) buffer & (JSON__ALIGNMENT - 1)) && length > 0) {
    buffer += 1;
    length -= 1;
  }
  if (length < JSON__ALIGNMENT) {
    arena->buffer = 0;
    arena->size = 0;
    arena->heap = 0;
    arena->stack = 0;
    return -1;
  }
  while (length & (JSON__ALIGNMENT - 1)) {
    length -= 1;
  }
  arena->buffer = buffer;
  arena->size = length;
  arena->heap = 0;
  arena->stack = length;
  return 0;
}

void *json_arena_alloc(JsonArena *arena, size_t size)
{
  void *r;
  while (size & (JSON__ALIGNMENT - 1)) {
    if (json__builtin_add_overflow(size, 1, &size)) return 0;
  }
  if (size > arena->stack - arena->heap) return 0;
  r = arena->buffer + arena->heap;
  arena->heap += size;
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
  return (v & 3) == JSON_UNDEFINED && v > 3;
}

int json_is_string(JsonValue v)
{
  return (v & 3) == JSON_TRUE && v > 3;
}

int json_is_object(JsonValue v)
{
  return (v & 3) == JSON_FALSE && v > 3;
}

int json_is_array(JsonValue v)
{
  return (v & 3) == JSON_NULL && v > 3;
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
  return (JsonValue) s | JSON_TRUE;
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

JsonArrayIterator json_array_iterator(JsonValue array)
{
  if (!json_is_array(array)) return 0;
  struct JsonArray *a = json__pointer(array);
  return a->first;
}

int json_array_next(JsonArrayIterator *i, JsonValue *value)
{
  if (!i || !*i) return 0;
  if (value) *value = (*i)->value;
  *i = (*i)->next;
  return 1;
}

JsonValue json_number_new(JsonArena *arena, JsonNumber number)
{
  JsonNumber *n = json_arena_alloc(arena, sizeof(*n));
  if (!n) return JSON_UNDEFINED;
  *n = number;
  return (JsonValue) n | JSON_UNDEFINED;
}

JsonValue json_object_new(JsonArena *arena)
{
  struct JsonObject *o = json_arena_alloc(arena, sizeof(*o));
  if (!o) return JSON_UNDEFINED;
  o->first = 0;
  o->last = 0;
  return (JsonValue) o | JSON_FALSE;
}

int json_object_append(JsonArena *arena, JsonValue object, const char *key, JsonValue value)
{
  if (!json_is_object(object) || !key) return -1;
  struct JsonObjectEntry *e = json_arena_alloc(arena, sizeof(*e));
  if (!e) return -1;
  struct JsonObject *o = json__pointer(object);
  e->key = key;
  e->value = value;
  e->next = 0;
  if (o->last) {
    o->last->next = e;
  } else {
    o->first = e;
  }
  o->last = e;
  return 0;
}

int json_object_set(JsonArena *arena, JsonValue object, const char *key, JsonValue value)
{
  if (!json_is_object(object) || !key) return -1;
  JsonObjectIterator i = json_object_iterator(object), j = i;
  const char *k;
  while (json_object_next(&i, &k, 0)) {
    if (strcmp(k, key) == 0) {
      j->value = value;
      return 0;
    }
    j = i;
  }
  return json_object_append(arena, object, key, value);
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

JsonValue json_array_new(JsonArena *arena)
{
  struct JsonArray *a = json_arena_alloc(arena, sizeof(*a));
  if (!a) return JSON_UNDEFINED;
  a->first = 0;
  a->last = 0;
  return (JsonValue) a | JSON_NULL;
}

int json_array_push(JsonArena *arena, JsonValue array, JsonValue value)
{
  if (!json_is_array(array)) return -1;
  struct JsonArrayElement *e = json_arena_alloc(arena, sizeof(*e));
  if (!e) return -1;
  struct JsonArray *a = json__pointer(array);
  e->value = value;
  e->next = 0;
  if (a->last) {
    a->last->next = e;
  } else {
    a->first = e;
  }
  a->last = e;
  return 0;
}

JsonValue json_array_get(JsonValue array, int index)
{
  if (!json_is_array(array) || index < 0) return JSON_UNDEFINED;
  int n = 0;
  JsonValue v;
  JsonArrayIterator i = json_array_iterator(array);
  while (json_array_next(&i, &v)) {
    if (n == index) return v;
    n += 1;
  }
  return JSON_UNDEFINED;
}

static size_t json__write(const char *text, size_t text_len, char *buffer, size_t length)
{
  if (text_len >= length) {
    if (length > 0) buffer[0] = 0;
    return 0;
  }
  memcpy(buffer, text, text_len);
  buffer[text_len] = 0;
  return text_len;
}

#define json__write_literal(text, buffer, length) json__write(text, strlen(text), buffer, length)

static const char json__hex[16] = "0123456789abcdef";

static size_t json__stringify_string(const char *string, char *buffer, size_t length)
{
  size_t n = 0;
  if (length < 3) return 0;
  buffer[n++] = '"';
  while (*string) {
    if (*string == '"') {
      if (length < n + 4) return 0;
      buffer[n++] = '\\';
      buffer[n++] = '"';
    } else if (*string == '\\') {
      if (length < n + 4) return 0;
      buffer[n++] = '\\';
      buffer[n++] = '\\';
    } else if (*string == '\r') {
      if (length < n + 4) return 0;
      buffer[n++] = '\\';
      buffer[n++] = 'r';
    } else if (*string == '\n') {
      if (length < n + 4) return 0;
      buffer[n++] = '\\';
      buffer[n++] = 'n';
    } else if (*string == '\t') {
      if (length < n + 4) return 0;
      buffer[n++] = '\\';
      buffer[n++] = 't';
    } else if ((*string & 255) < 32) {
      // Escape sequence.
      if (length < n + 8) return 0;
      buffer[n++] = '\\';
      buffer[n++] = 'u';
      buffer[n++] = '0';
      buffer[n++] = '0';
      buffer[n++] = json__hex[(*string >> 4) & 15];
      buffer[n++] = json__hex[*string & 15];
    } else {
      if (length < n + 4) return 0;
      buffer[n++] = *string;
    }
    ++string;
  }
  buffer[n++] = '"';
  buffer[n] = 0;
  return n;
}

static size_t json__stringify_object(JsonValue object, char *buffer, size_t length)
{
  size_t l, n = 1;
  if (length < 3) return 0;
  buffer[0] = '{';
  const char *k;
  JsonValue v;
  JsonObjectIterator i = json_object_iterator(object);
  while (json_object_next(&i, &k, &v)) {
    if (n > 1) {
      if (length < n + 6) return 0;
      buffer[n++] = ',';
    }
    if ((l = json__stringify_string(k, buffer + n, length - n)) == 0) return 0;
    n += l;
    if (length < n + 4) return 0;
    buffer[n++] = ':';
    if ((l = json_stringify(v, buffer + n, length - n)) == 0) return 0;
    n += l;
  }
  if (length < n + 2) return 0;
  buffer[n++] = '}';
  buffer[n] = 0;
  return n;
}

static size_t json__stringify_array(JsonValue array, char *buffer, size_t length)
{
  size_t l, n = 1;
  if (length < 3) return 0;
  buffer[0] = '[';
  JsonValue v;
  JsonArrayIterator i = json_array_iterator(array);
  while (json_array_next(&i, &v)) {
    if (n > 1) {
      if (length < n + 4) return 0;
      buffer[n++] = ',';
    }
    if ((l = json_stringify(v, buffer + n, length - n)) == 0) return 0;
    n += l;
  }
  if (length < n + 2) return 0;
  buffer[n++] = ']';
  buffer[n] = 0;
  return n;
}

static void json__reverse(char *buffer, size_t length)
{
  size_t i;
  char c;
  for (i = 0; i < length / 2; ++i) {
    c = buffer[i];
    buffer[i] = buffer[length - i - 1];
    buffer[length - i - 1] = c;
  }
}

static size_t json__stringify_number(JsonNumber number, char *buffer, size_t length)
{
  size_t n = 0;
  int negative = 0;
  if (number < 0) {
    if (length < 2 || json__builtin_sub_overflow(0, number, &number)) return 0;
    buffer[0] = '-';
    ++buffer;
    --length;
    negative = 1;
  }
  do {
    if (length < n + 2) return 0;
    buffer[n++] = '0' + number % 10;
    number /= 10;
  } while (number > 0);
  json__reverse(buffer, n);
  buffer[n] = 0;
  return n + negative;
}

size_t json_stringify(JsonValue v, char *buffer, size_t length)
{
  size_t r = 0;
  if (v == JSON_NULL) {
    r = json__write_literal("null", buffer, length);
  } else if (v == JSON_TRUE) {
    r = json__write_literal("true", buffer, length);
  } else if (v == JSON_FALSE) {
    r = json__write_literal("false", buffer, length);
  } else if (json_is_number(v)) {
    r = json__stringify_number(*((JsonNumber *) json__pointer(v)), buffer, length);
  } else if (json_is_string(v)) {
    r = json__stringify_string(json__pointer(v), buffer, length);
  } else if (json_is_object(v)) {
    r = json__stringify_object(v, buffer, length);
  } else if (json_is_array(v)) {
    r = json__stringify_array(v, buffer, length);
  }
  if (r == 0) {
    if (length > 0) buffer[0] = 0;
  }
  return r;
}

#endif
