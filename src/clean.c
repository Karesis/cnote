

#include <clean.h>

#include <core/msg/asrt.h>
#include <std/io/file.h>
#include <std/string/str_slice.h>
#include <std/string/string.h>

#include <stdio.h>
#include <stdlib.h>

typedef enum {
  STATE_CODE,
  STATE_LINE_COMMENT,
  STATE_BLOCK_COMMENT,
  STATE_STRING,
  STATE_CHAR,
} clean_state_t;

bool cnote_clean_file(allocer_t *alc, const char *filename) {

  str_slice_t content;
  if (!read_file_to_slice(alc, filename, &content)) {
    fprintf(stderr, "Error: Failed to read file '%s'.\n", filename);
    return false;
  }

  string_t builder;
  string_init(&builder, alc, content.len);

  clean_state_t state = STATE_CODE;
  const char *p = content.ptr;
  const char *end = content.ptr + content.len;

  while (p < end) {
    char c = *p;
    char next = (p + 1 < end) ? *(p + 1) : '\0';

    switch (state) {

    case STATE_CODE:
      if (c == '/' && next == '/') {
        state = STATE_LINE_COMMENT;
        p++;
      } else if (c == '/' && next == '*') {
        state = STATE_BLOCK_COMMENT;
        string_push(&builder, c);
        string_push(&builder, next);
        p++;
      } else if (c == '"') {
        state = STATE_STRING;
        string_push(&builder, c);
      } else if (c == '\'') {
        state = STATE_CHAR;
        string_push(&builder, c);
      } else {
        string_push(&builder, c);
      }
      break;

    case STATE_LINE_COMMENT:
      if (c == '\n') {
        state = STATE_CODE;
        string_push(&builder, c);
      }
      break;

    case STATE_BLOCK_COMMENT:
      string_push(&builder, c);
      if (c == '*' && next == '/') {
        string_push(&builder, next);
        state = STATE_CODE;
        p++;
      }
      break;

    case STATE_STRING:
      string_push(&builder, c);
      if (c == '\\') {
        if (next != '\0') {
          string_push(&builder, next);
          p++;
        }
      } else if (c == '"') {
        state = STATE_CODE;
      }
      break;

    case STATE_CHAR:
      string_push(&builder, c);
      if (c == '\\') {
        if (next != '\0') {
          string_push(&builder, next);
          p++;
        }
      } else if (c == '\'') {
        state = STATE_CODE;
      }
      break;
    }

    p++;
  }

  str_slice_t result_slice = string_as_slice(&builder);

  if (!write_file_bytes(filename, (const void *)result_slice.ptr,
                        result_slice.len)) {
    fprintf(stderr, "Error: Failed to write file '%s'.\n", filename);
    string_destroy(&builder);
    return false;
  }

  string_destroy(&builder);

  string_t cmd;
  string_init(&cmd, alc, 64);

  string_append_cstr(&cmd, "clang-format -i ");
  string_append_cstr(&cmd, filename);

  printf("  Running: %s\n", string_as_cstr(&cmd));

  int ret = system(string_as_cstr(&cmd));

  if (ret != 0) {
    fprintf(stderr,
            "Warning: clang-format command failed (is it installed?)\n");
  }

  string_destroy(&cmd);

  return true;
}