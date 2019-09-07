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
// xxx..xx00 String/Undefined
// xxx..xx01 Number/True
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

// Get the current position of the arena.
// If the position is later passed to restore, all objects allocated after the call to
// json_arena_save are freed.
size_t json_arena_save(JsonArena *arena);

// Restore the arena to a previous state.
void json_arena_restore(JsonArena *arena, size_t position);

// Parse a JSON string into a JsonValue.
// Returns the position after the parsed value.
// If an error occurs, 0 is returned.
const char *json_parse(const char *json, JsonArena *arena, JsonValue *result);

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

// Create a number owned by the calling code.
JsonValue json_number_extern(const JsonNumber *number);

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

// Iterator over an object.
typedef struct JsonObjectEntry *JsonObjectIterator;
// Get an iterator for an object.
JsonObjectIterator json_object_iterator(JsonValue v);
// Advance an iterator. Returns 1 if a key and value were produced, 0 at the end.
// key or value may be null if unneeded.
int json_object_next(JsonObjectIterator *i, const char **key, JsonValue *value);

// Iterator over an array.
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

size_t json_arena_save(JsonArena *arena)
{
  return arena->heap;
}

void json_arena_restore(JsonArena *arena, size_t position)
{
  arena->heap = position;
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
  return (v & 3) == JSON_TRUE && v > 3;
}

int json_is_string(JsonValue v)
{
  return (v & 3) == JSON_UNDEFINED && v > 3;
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
    return *((const JsonNumber *) json__pointer(v));
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
  return (JsonValue) s | JSON_UNDEFINED;
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
  return (JsonValue) n | JSON_TRUE;
}

JsonValue json_number_extern(const JsonNumber *number)
{
  if (!number) return JSON_UNDEFINED;
  return (JsonValue) number | JSON_TRUE;
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
  JsonNumber num;
  if (number < 0) {
    if (length < 2) return 0;
    buffer[0] = '-';
    ++buffer;
    --length;
    negative = 1;
    if (json__builtin_sub_overflow(0, number, &num)) {
      // If this overflows, the last digit is greater than 0.
      number += 1;
      buffer[n++] = '1' + (-number) % 10;
      number = (-number) / 10;
    } else {
      number = num;
    }
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

static const char *json__skip_whitespace(const char *json)
{
  if (!json) return 0;
  while (*json == ' ' || *json == '\t' || *json == '\r' || *json == '\n') {
    ++json;
  }
  return json;
}

static const char *json__parse_simple(const char *json, JsonValue *result)
{
  if (json[0] == 't' && json[1] == 'r' && json[2] == 'u' && json[3] == 'e') {
    if (result) *result = JSON_TRUE;
    return json + 4;
  } else if (json[0] == 'f' && json[1] == 'a' && json[2] == 'l' && json[3] == 's' && json[4] == 'e') {
    if (result) *result = JSON_FALSE;
    return json + 5;
  } else if (json[0] == 'n' && json[1] == 'u' && json[2] == 'l' && json[3] == 'l') {
    if (result) *result = JSON_NULL;
    return json + 4;
  } else {
    return 0;
  }
}

static const char *json__parse_string(const char *json, JsonArena *arena, const char **result)
{
  size_t l = 0;
  char c;
  if (arena->heap + 1 > arena->stack) return 0;
  // Opening quote.
  ++json;
  while ((c = *json) != '"') {
    if ((c & 255) < 32) return 0;
    if (arena->heap + l + 2 > arena->stack) return 0;
    if (c == '\\') {
      ++json;
      switch (*json) {
      case '"':
        c = '"';
        break;
      case '\\':
        c = '\\';
        break;
      case '/':
        c = '/';
        break;
      case 't':
        c = '\t';
        break;
      case 'r':
        c = '\r';
        break;
      case 'n':
        c = '\n';
        break;
      case 'b':
        c = '\b';
        break;
      case 'f':
        c = '\f';
        break;
      default:
        return 0;
      }
    }
    arena->buffer[arena->heap + l++] = c;
    ++json;
  }
  ++json;
  // Zero terminate and update arena.
  arena->buffer[arena->heap + l] = 0;
  l += 1;
  while (l & (JSON__ALIGNMENT - 1)) {
    l += 1;
  }
  if (arena->heap + l > arena->stack) return 0;
  if (result) *result = (const char *) (arena->buffer + arena->heap);
  arena->heap += l;
  return json;
}

static int json__is_digit(char c)
{
  return '0' <= c && c <= '9';
}

static const char *json__parse_number(const char *json, JsonArena *arena, JsonValue *result)
{
  JsonNumber n = 0;
  int negative = 0;
  if (*json == '+') {
    ++json;
  } else if (*json == '-') {
    negative = 1;
    ++json;
  }
  if (!json__is_digit(*json)) return 0;
  while (json__is_digit(*json)) {
    if (json__builtin_mul_overflow(n, 10, &n)) return 0;
    if (negative) {
      if (json__builtin_sub_overflow(n, *json - '0', &n)) return 0;
    } else {
      if (json__builtin_add_overflow(n, *json - '0', &n)) return 0;
    }
    ++json;
  }
  if (result) {
    *result = json_number_new(arena, n);
    if (*result == JSON_UNDEFINED) return 0;
  }
  return json;
}

static const char *json__parse_object(const char *json, JsonArena *arena, JsonValue *result)
{
  size_t position = json_arena_save(arena);
  JsonValue object = json_object_new(arena);
  const char *k;
  JsonValue v;
  int start = 1;
  // Skip opening brace and whitespace.
  json = json__skip_whitespace(json + 1);
  while (1) {
    if (*json == '}') {
      if (result) *result = object;
      return json + 1;
    }
    if (!start) {
      if (*json != ',') goto fail;
      json = json__skip_whitespace(json + 1);
    }
    json = json__parse_string(json, arena, &k);
    if (!json) goto fail;
    json = json__skip_whitespace(json);
    if (*json != ':') goto fail;
    json = json__skip_whitespace(json + 1);
    json = json_parse(json, arena, &v);
    if (!json) goto fail;
    if (json_object_append(arena, object, k, v)) goto fail;
    json = json__skip_whitespace(json);
    start = 0;
  }
fail:
  json_arena_restore(arena, position);
  return 0;
}

static const char *json__parse_array(const char *json, JsonArena *arena, JsonValue *result)
{
  size_t position = json_arena_save(arena);
  JsonValue array = json_array_new(arena);
  JsonValue v;
  int start = 1;
  // Skip opening bracket and whitespace.
  json = json__skip_whitespace(json + 1);
  while (1) {
    if (*json == ']') {
      if (result) *result = array;
      return json + 1;
    }
    if (!start) {
      if (*json != ',') goto fail;
      json = json__skip_whitespace(json + 1);
    }
    json = json_parse(json, arena, &v);
    if (!json) goto fail;
    if (json_array_push(arena, array, v)) goto fail;
    json = json__skip_whitespace(json);
    start = 0;
  }
fail:
  json_arena_restore(arena, position);
  return 0;
}

const char *json__parse_string_value(const char *json, JsonArena *arena, JsonValue *result)
{
  const char *s, *r = json__parse_string(json, arena, &s);
  if (!r) return 0;
  if (result) *result = json_string(s);
  return r;
}

const char *json_parse(const char *json, JsonArena *arena, JsonValue *result)
{
  switch (*json) {
  case '{':
    return json__parse_object(json, arena, result);
  case '[':
    return json__parse_array(json, arena, result);
  case '"':
    return json__parse_string_value(json, arena, result);
  case 't':
  case 'f':
  case 'n':
    return json__parse_simple(json, result);
  case '+':
  case '-':
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    return json__parse_number(json, arena, result);
  default:
    return 0;
  }
}

#endif
