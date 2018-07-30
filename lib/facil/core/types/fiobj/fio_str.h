#ifndef H_FIO_STRING_H
/*
Copyright: Boaz Segev, 2018
License: MIT
*/

/**
 * A Dynamic String for ease of use and binary strings.
 *
 * The string is a simple byte string which is compatible with binary data (NUL
is a valid byte).
 *
 * Use:

fio_str_s str = FIO_STR_INIT;    // a container can be placed on the stack.
fio_str_write(&str, "hello", 5); // add / remove / read data...
fio_str_free(&str)               // free any resources, not the container.

 */
#define H_FIO_STRING_H

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef FIO_FUNC
#define FIO_FUNC static __attribute__((unused))
#endif

#ifndef FIO_ASSERT_ALLOC
#define FIO_ASSERT_ALLOC(ptr)                                                  \
  if (!(ptr)) {                                                                \
    perror("FATAL ERROR: no memory (for string allocation)");                  \
    exit(errno);                                                               \
  }
#endif

#ifdef __cplusplus
#define register
#endif

/* *****************************************************************************
String API - Initialization and Destruction
***************************************************************************** */

typedef struct {
  uint8_t small;
  uint8_t frozen;
  uint8_t reserved[sizeof(size_t) - (sizeof(uint8_t) * 2)];
  size_t capa;
  size_t len;
  char *data;
} fio_str_s;

/**
 * This value should be used for initialization. For example:
 *
 *      // on the stack
 *      fio_str_s str = FIO_STR_INIT;
 *
 *      // or on the heap
 *      fio_str_s *str = malloc(sizeof(*str);
 *      *str = FIO_STR_INIT;
 *
 * Remember to cleanup:
 *
 *      // on the stack
 *      fio_str_free(&str);
 *
 *      // or on the heap
 *      fio_str_free(str);
 *      free(str);
 */
#define FIO_STR_INIT ((fio_str_s){.data = NULL, .small = 1})

/**
 * This macro allows the container to be initialized with existing data, as long
 * as it's memory was allocated using `malloc`.
 *
 * The `capacity` value should exclude the NUL character (if exists).
 */
#define FIO_STR_INIT_EXISTING(buffer, length, capacity)                        \
  ((fio_str_s){.data = (buffer), .len = (length), .capa = (capacity)})

/**
 * Frees the String's resources and _reinitializes the container_.
 *
 * Note: if the container isn't allocated on the stack, it should be freed
 * separately using `free(s)`.
 */
inline FIO_FUNC void fio_str_free(fio_str_s *s);

/* *****************************************************************************
String API - String state (data pointers, length, capacity, etc')
***************************************************************************** */

typedef struct {
  size_t capa;
  size_t len;
  char *data;
} fio_str_state_s;

/** Returns the String's state (capacity, length and pointer). */
inline FIO_FUNC fio_str_state_s fio_str_state(const fio_str_s *s);

/** Returns the String's length in bytes. */
inline FIO_FUNC size_t fio_str_len(fio_str_s *s);

/** Returns a pointer (`char *`) to the String's content. */
inline FIO_FUNC char *fio_str_data(fio_str_s *s);

/** Returns a byte pointer (`uint8_t *`) to the String's unsigned content. */
inline FIO_FUNC uint8_t *fio_str_bytes(fio_str_s *s);

/** Returns the String's existing capacity (allocated memory). */
inline FIO_FUNC size_t fio_str_capa(fio_str_s *s);

/* *****************************************************************************
String API - Memory management and resizing
***************************************************************************** */

/** Performs a best attempt at minimizing memory consumption. */
inline FIO_FUNC void fio_str_compact(fio_str_s *s);

/**
 * Requires the String to have at least `needed` capacity. Returns the current
 * state of the String.
 */
FIO_FUNC fio_str_state_s fio_str_capa_assert(fio_str_s *s, size_t needed);

/**
 * Sets the new String size without reallocating any memory (limited by
 * existing capacity).
 *
 * Returns the updated state of the String.
 *
 * Note: When shrinking, any existing data beyond the new size may be corrupted.
 */
inline FIO_FUNC fio_str_state_s fio_str_resize(fio_str_s *s, size_t size);

/**
 * Clears the string (retaining the existing capacity).
 */
#define fio_str_clear(s) fio_str_resize((s), 0)

/* *****************************************************************************
String API - Content Manipulation
***************************************************************************** */

/**
 * Writes data at the end of the String (similar to `fio_str_insert` with the
 * argument `pos == -1`).
 */
inline FIO_FUNC fio_str_state_s fio_str_write(fio_str_s *s, void *src,
                                              size_t src_len);

/**
 * Appens the `src` String to the end of the `dest` String.
 */
inline FIO_FUNC fio_str_state_s fio_str_concat(fio_str_s *dest,
                                               fio_str_s const *src);

/**
 * Pastes data to the specified location in the string, overwriting existing
 * data.
 *
 * Negative `pos` values are calculated backwards, `-1` == end of String.
 */
inline FIO_FUNC fio_str_state_s fio_str_overwrite(fio_str_s *s, void *src,
                                                  size_t src_len, intptr_t pos);

/**
 * Pastes data to the specified location in the string, moving existing data.
 *
 * Negative `pos` values are calculated backwards, `-1` == end of String.
 */
inline FIO_FUNC fio_str_state_s fio_str_insert(fio_str_s *s, void *src,
                                               size_t src_len, intptr_t pos);

/**
 * Prevents further manipulations to the String's content.
 */
inline FIO_FUNC void fio_str_freeze(fio_str_s *s);

/* *****************************************************************************


                              IMPLEMENTATION


***************************************************************************** */

/* *****************************************************************************
Implementation - String state (data pointers, length, capacity, etc')
***************************************************************************** */

/* the capacity when the string is stored in the container itself */
#define FIO_STR_SMALL_CAPA                                                     \
  (sizeof(fio_str_s) - (size_t)(&((fio_str_s *)0)->reserved))

typedef struct {
  uint8_t small;
  uint8_t frozen;
  char data[1];
} fio_str__small_s;

/** Returns the String's state (capacity, length and pointer). */
inline FIO_FUNC fio_str_state_s fio_str_state(const fio_str_s *s) {
  if (!s)
    return (fio_str_state_s){.capa = 0};
  return (s->small || !s->data)
             ? (fio_str_state_s){.capa = (FIO_STR_SMALL_CAPA - 1),
                                 .len = (size_t)(s->small >> 1),
                                 .data = ((fio_str__small_s *)s)->data}
             : (fio_str_state_s){
                   .capa = s->capa, .len = s->len, .data = s->data};
}

/**
 * Frees the String's resources and reinitializes the container.
 *
 * Note: if the container isn't allocated on the stack, it should be freed
 * separately using `free(s)`.
 */
inline FIO_FUNC void fio_str_free(fio_str_s *s) {
  if (!s->small)
    free(s->data);
  *s = FIO_STR_INIT;
}

/** Returns the String's length in bytes. */
inline FIO_FUNC size_t fio_str_len(fio_str_s *s) {
  return (s->small || !s->data) ? (s->small >> 1) : s->len;
}

/** Returns a pointer (`char *`) to the String's content. */
inline FIO_FUNC char *fio_str_data(fio_str_s *s) {
  return (s->small || !s->data) ? (((fio_str__small_s *)s)->data) : s->data;
}

/** Returns a byte pointer (`uint8_t *`) to the String's unsigned content. */
inline FIO_FUNC uint8_t *fio_str_bytes(fio_str_s *s) {
  return (uint8_t *)fio_str_data(s);
}

/** Returns the String's existing capacity (allocated memory). */
inline FIO_FUNC size_t fio_str_capa(fio_str_s *s) {
  return (s->small || !s->data) ? (FIO_STR_SMALL_CAPA - 1) : s->capa;
}

/* *****************************************************************************
Implementation - Memory management and resizing
***************************************************************************** */

/** Performs a best attempt at minimizing memory consumption. */
inline FIO_FUNC void fio_str_compact(fio_str_s *s) {
  if (!s || (s->small || !s->data))
    return;
  if (s->len < FIO_STR_SMALL_CAPA)
    goto shrink2small;
  s->data = realloc(s->data, s->len + 1);
  FIO_ASSERT_ALLOC(s->data);
  s->capa = s->len;
  return;

shrink2small:
  (void)s;
  char *tmp = s->data;
  size_t len = s->len;
  *s = (fio_str_s){.small = (uint8_t)(((len << 1) | 1) & 0xFF),
                   .frozen = s->frozen};
  if (len) {
    memcpy(((fio_str__small_s *)s)->data, tmp, len + 1);
  }
  free(tmp);
}

/* *****************************************************************************
String data
***************************************************************************** */

/**
 * Requires the String to have at least `needed` capacity. Returns the current
 * state of the String.
 */
FIO_FUNC fio_str_state_s fio_str_capa_assert(fio_str_s *s, size_t needed) {
  if (!s)
    return (fio_str_state_s){.capa = 0};
  if (s->small || !s->data) {
    goto is_small;
  }
  if (needed > s->capa) {
    s->capa = needed;
    s->data = (char *)realloc(s->data, s->capa + 1);
    FIO_ASSERT_ALLOC(s->data);
    s->data[s->capa] = 0;
  }
  return (fio_str_state_s){.capa = s->capa, .len = s->len, .data = s->data};

is_small:
  if (needed < FIO_STR_SMALL_CAPA) {
    return (fio_str_state_s){.capa = (FIO_STR_SMALL_CAPA - 1),
                             .len = (size_t)(s->small >> 1),
                             .data = ((fio_str__small_s *)s)->data};
  }
  char *tmp = (char *)malloc(needed + 1);
  FIO_ASSERT_ALLOC(tmp);
  if ((size_t)(s->small >> 1)) {
    memcpy(tmp, ((fio_str__small_s *)s)->data, (size_t)(s->small >> 1) + 1);
  } else {
    tmp[0] = 0;
  }
  *s = (fio_str_s){
      .small = 0,
      .capa = needed,
      .len = (size_t)(s->small >> 1),
      .data = tmp,
  };
  return (fio_str_state_s){.capa = needed, .len = s->len, .data = s->data};
}

/**
 * Sets the new String size without reallocating any memory (limited by
 * existing capacity).
 *
 * Returns the updated state of the String.
 *
 * Note: When shrinking, any existing data beyond the new size may be corrupted.
 */
inline FIO_FUNC fio_str_state_s fio_str_resize(fio_str_s *s, size_t size) {
  if (!s || s->frozen) {
    return fio_str_state(s);
  }
  fio_str_capa_assert(s, size);
  if (s->small || !s->data) {
    s->small = (uint8_t)(((size << 1) | 1) & 0xFF);
    ((fio_str__small_s *)s)->data[size] = 0;
    return (fio_str_state_s){.capa = (FIO_STR_SMALL_CAPA - 1),
                             .len = size,
                             .data = ((fio_str__small_s *)s)->data};
  }
  s->len = size;
  s->data[size] = 0;
  return (fio_str_state_s){.capa = s->capa, .len = size, .data = s->data};
}

/* *****************************************************************************
Implementation - Content Manipulation
***************************************************************************** */

/**
 * Writes data at the end of the String (similar to `fio_str_insert` with the
 * argument `pos == -1`).
 */
inline FIO_FUNC fio_str_state_s fio_str_write(fio_str_s *s, void *src,
                                              size_t src_len) {
  if (!s || !src_len || !src || s->frozen)
    return fio_str_state(s);
  fio_str_state_s state = fio_str_resize(s, src_len + fio_str_len(s));
  memcpy(state.data + (state.len - src_len), src, src_len);
  return state;
}

/**
 * Appens the `src` String to the end of the `dest` String.
 */
inline FIO_FUNC fio_str_state_s fio_str_concat(fio_str_s *dest,
                                               fio_str_s const *src) {
  if (!dest || !src || dest->frozen)
    return fio_str_state(dest);
  fio_str_state_s src_state = fio_str_state(src);
  if (!src_state.len)
    return fio_str_state(dest);
  fio_str_state_s state =
      fio_str_resize(dest, src_state.len + fio_str_len(dest));
  memcpy(state.data + state.len - src_state.len, src_state.data, src_state.len);
  return state;
}
/**
 * Pastes data to the specified location in the string, overwriting existing
 * data.
 */
inline FIO_FUNC fio_str_state_s fio_str_overwrite(fio_str_s *s, void *src,
                                                  size_t src_len,
                                                  intptr_t pos) {
  if (!s || !src_len || !src || s->frozen)
    return fio_str_state(s);
  if (pos < 0) {
    /* backwards position indexing */
    pos += s->len + 1;
    if (pos < 0)
      pos = 0;
  }

  fio_str_state_s state = fio_str_state(s);

  if ((size_t)pos > state.len)
    pos = state.len; /* prevent overflow */

  if (pos + src_len > state.len) {
    state = fio_str_resize(s, (pos + src_len));
  }
  memcpy(state.data + pos, src, src_len);
  if (pos + src_len > s->len)
    s->len = pos + src_len;
  return state;
}

/**
 * Pastes data to the specified location in the string, moving existing data.
 *
 * Negative `pos` values allow for reverse positioning (-1 == end of String).
 */
inline FIO_FUNC fio_str_state_s fio_str_insert(fio_str_s *s, void *src,
                                               size_t src_len, intptr_t pos) {
  if (!s || !src_len || !src || s->frozen)
    return fio_str_state(s);
  if (pos < 0) {
    /* backwards position indexing */
    pos += s->len + 1;
    if (pos < 0)
      pos = 0;
  }

  fio_str_state_s state = fio_str_resize(s, src_len + fio_str_len(s));

  if ((size_t)pos > (state.len - src_len))
    pos = (state.len - src_len); /* prevent overflow */

  if ((size_t)pos != state.len) {
    memmove(state.data + pos + src_len, state.data + pos, src_len);
  }
  memcpy(state.data + pos, src, src_len);
  return state;
}

/**
 * Prevents further manipulations to the String's content.
 */
inline FIO_FUNC void fio_str_freeze(fio_str_s *s) {
  if (!s)
    return;
  s->frozen = 1;
}

/* *****************************************************************************
Testing
***************************************************************************** */

#if DEBUG
#include <stdio.h>
#define TEST_ASSERT(cond, ...)                                                 \
  if (!(cond)) {                                                               \
    fprintf(stderr, "* " __VA_ARGS__);                                         \
    fprintf(stderr, "\n !!! Testing failed !!!\n");                            \
    exit(-1);                                                                  \
  }
/**
 * Removes any FIO_ARY_TYPE_INVALID  *pointers* from an Array, keeping all other
 * data in the array.
 *
 * This action is O(n) where n in the length of the array.
 * It could get expensive.
 */
FIO_FUNC inline void fio_str_test(void) {
  fprintf(stderr, "=== Testing Core String features (fio_str.h)\n");
  fprintf(stderr, "* String container size: %zu\n", sizeof(fio_str_s));
  fprintf(stderr,
          "* Self-Contained String Capacity (FIO_STR_SMALL_CAPA): %zu\n",
          FIO_STR_SMALL_CAPA);
  fio_str_s str = FIO_STR_INIT;
  TEST_ASSERT(fio_str_capa(&str) == FIO_STR_SMALL_CAPA - 1,
              "Small String capacity reporting error!");
  TEST_ASSERT(fio_str_len(&str) == 0, "Small String length reporting error!");
  TEST_ASSERT(fio_str_data(&str) ==
                  (char *)((uintptr_t)(&str + 1) - FIO_STR_SMALL_CAPA),
              "Small String pointer reporting error!");
  fio_str_write(&str, "World", 4);
  TEST_ASSERT(str.small,
              "Small String writing error - not small on small write!");
  TEST_ASSERT(fio_str_capa(&str) == FIO_STR_SMALL_CAPA - 1,
              "Small String capacity reporting error after write!");
  TEST_ASSERT(fio_str_len(&str) == 4,
              "Small String length reporting error after write!");
  TEST_ASSERT(fio_str_data(&str) ==
                  (char *)((uintptr_t)(&str + 1) - FIO_STR_SMALL_CAPA),
              "Small String pointer reporting error after write!");
  TEST_ASSERT(strlen(fio_str_data(&str)) == 4,
              "Small String NUL missing after write (%zu)!",
              strlen(fio_str_data(&str)));
  TEST_ASSERT(!strcmp(fio_str_data(&str), "Worl"),
              "Small String write error (%s)!", fio_str_data(&str));

  fio_str_capa_assert(&str, sizeof(fio_str_s));
  TEST_ASSERT(!str.small,
              "Long String reporting as small after capacity update!");
  TEST_ASSERT(fio_str_capa(&str) == sizeof(fio_str_s),
              "Long String capacity update error (%zu != %zu)!",
              fio_str_capa(&str), sizeof(fio_str_s));
  TEST_ASSERT(
      fio_str_len(&str) == 4,
      "Long String length changed during conversion from small string (%zu)!",
      fio_str_len(&str));
  TEST_ASSERT(fio_str_data(&str) == str.data,
              "Long String pointer reporting error after capacity update!");
  TEST_ASSERT(strlen(fio_str_data(&str)) == 4,
              "Long String NUL missing after capacity update (%zu)!",
              strlen(fio_str_data(&str)));
  TEST_ASSERT(!strcmp(fio_str_data(&str), "Worl"),
              "Long String value changed after capacity update (%s)!",
              fio_str_data(&str));

  fio_str_write(&str, "d!", 2);
  TEST_ASSERT(!strcmp(fio_str_data(&str), "World!"),
              "Long String `write` error (%s)!", fio_str_data(&str));

  fio_str_insert(&str, "Hello ", 6, 0);
  TEST_ASSERT(!strcmp(fio_str_data(&str), "Hello World!"),
              "Long String `insert` error (%s)!", fio_str_data(&str));

  str.capa = str.len;
  fio_str_overwrite(&str, "Big World!", 10, 6);
  TEST_ASSERT(!strcmp(fio_str_data(&str), "Hello Big World!"),
              "Long String `overwrite` error (%s)!", fio_str_data(&str));
  TEST_ASSERT(fio_str_capa(&str) == strlen("Hello Big World!"),
              "Long String `overwrite` capacity update error (%zu != %zu)!",
              fio_str_capa(&str), strlen("Hello Big World!"));
  if (str.len < FIO_STR_SMALL_CAPA) {
    fio_str_compact(&str);
    TEST_ASSERT(str.small, "Compacting didn't change String to small!");
    TEST_ASSERT(fio_str_len(&str) == strlen("Hello Big World!"),
                "Compacting altered String length! (%zu != %zu)!",
                fio_str_len(&str), strlen("Hello Big World!"));
    TEST_ASSERT(!strcmp(fio_str_data(&str), "Hello Big World!"),
                "Compact data error (%s)!", fio_str_data(&str));
    TEST_ASSERT(fio_str_capa(&str) == FIO_STR_SMALL_CAPA - 1,
                "Compacted String capacity reporting error!");
  } else {
    fprintf(stderr, "* skipped `compact` test!\n");
  }

  fio_str_freeze(&str);
  {
    fio_str_state_s old_state = fio_str_state(&str);
    fio_str_write(&str, "more data to be written here", 28);
    fio_str_insert(&str, "more data to be written here", 28, -1);
    fio_str_overwrite(&str, "more data to be written here", 28, -1);
    fio_str_state_s new_state = fio_str_state(&str);
    TEST_ASSERT(old_state.len == new_state.len,
                "Frozen String length changed!");
    TEST_ASSERT(old_state.data == new_state.data,
                "Frozen String pointer changed!");
    TEST_ASSERT(
        old_state.capa == new_state.capa,
        "Frozen String capacity changed (allowed, but shouldn't happen)!");
  }

  fio_str_free(&str);
  fprintf(stderr, "* passed.\n");
}
#undef TEST_ASSERT
#endif

/* *****************************************************************************
Done
***************************************************************************** */

#ifdef __cplusplus
#undef register
#endif

#undef FIO_FUNC
#undef FIO_ASSERT_ALLOC

#endif