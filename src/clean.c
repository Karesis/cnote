/*
 *    Copyright 2025 Karesis
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include <clean.h>

#include <core/mem/layout.h>
#include <core/msg/asrt.h>
#include <std/io/file.h>
#include <std/string/str_slice.h>
#include <std/string/string.h>
#include <std/vec.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

typedef enum {
  STATE_CODE,
  STATE_LINE_COMMENT,
  STATE_BLOCK_COMMENT,
  STATE_STRING,
  STATE_CHAR,
} clean_state_t;

/**
 * @brief (辅助) 复制一个 C 字符串到 Arena
 */
static inline char *allocer_strdup(allocer_t *alc, const char *s) {
  size_t len = strlen(s);
  layout_t layout = layout_of_array(char, len + 1);
  char *new_s = allocer_alloc(alc, layout);
  if (new_s) {
    memcpy(new_s, s, len + 1);
  }
  return new_s;
}

static bool clean_single_file(allocer_t *alc, const char *filename,
                              const char *style_file) {

  printf("  Cleaning: %s\n", filename);

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
        state = STATE_CODE;
        string_push(&builder, next);
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
  string_init(&cmd, alc, 256);
  string_append_cstr(&cmd, "clang-format -i ");
  if (style_file) {
    string_append_cstr(&cmd, "-style=file:");
    string_append_cstr(&cmd, style_file);
    string_push(&cmd, ' ');
  }
  string_append_cstr(&cmd, filename);
  int ret = system(string_as_cstr(&cmd));
  if (ret != 0) {
    fprintf(stderr,
            "Warning: clang-format command failed (is it installed?)\n");
  }
  string_destroy(&cmd);
  return true;
}

static bool is_excluded(const char *path, vec_t *exclusions) {
  for (size_t i = 0; i < vec_count(exclusions); i++) {
    const char *pattern = (const char *)vec_get(exclusions, i);
    if (strstr(path, pattern) != NULL) {
      printf("  Excluding: %s (matches '%s')\n", path, pattern);
      return true;
    }
  }
  return false;
}

static bool is_cleanable_file(const char *filename) {
  const char *dot = strrchr(filename, '.');
  if (!dot)
    return false;
  if (strcmp(dot, ".c") == 0)
    return true;
  if (strcmp(dot, ".h") == 0)
    return true;
  return false;
}

static void traverse_dir_for_clean(allocer_t *alc, const char *current_path,
                                   vec_t *exclusions, const char *style_file,
                                   string_t *path_builder) {
  DIR *dir = opendir(current_path);
  if (!dir) {
    fprintf(stderr, "Warning: Could not open directory '%s'\n", current_path);
    return;
  }

  struct dirent *dp;
  while ((dp = readdir(dir)) != NULL) {
    const char *name = dp->d_name;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
      continue;
    }

    string_clear(path_builder);
    string_append_cstr(path_builder, current_path);

    if (current_path[strlen(current_path) - 1] != '/') {
      string_push(path_builder, '/');
    }
    string_append_cstr(path_builder, name);

    const char *full_path = string_as_cstr(path_builder);

    if (is_excluded(full_path, exclusions)) {
      continue;
    }

    struct stat statbuf;
    if (stat(full_path, &statbuf) != 0) {
      fprintf(stderr, "Warning: Could not stat file '%s'\n", full_path);
      continue;
    }

    if (S_ISDIR(statbuf.st_mode)) {

      char *stable_path = allocer_strdup(alc, full_path);
      if (stable_path) {
        traverse_dir_for_clean(alc, stable_path, exclusions, style_file,
                               path_builder);
      }
    } else if (is_cleanable_file(full_path)) {
      clean_single_file(alc, full_path, style_file);
    }
  }
  closedir(dir);
}

bool cnote_clean_run(allocer_t *alc, vec_t *targets, vec_t *exclusions,
                     const char *style_file) {
  string_t path_builder;
  if (!string_init(&path_builder, alc, 256)) {
    return false;
  }

  for (size_t i = 0; i < vec_count(targets); i++) {
    const char *target_path = (const char *)vec_get(targets, i);

    if (is_excluded(target_path, exclusions)) {
      continue;
    }

    struct stat statbuf;
    if (stat(target_path, &statbuf) != 0) {
      fprintf(stderr, "Warning: Could not stat target '%s'\n", target_path);
      continue;
    }

    if (S_ISDIR(statbuf.st_mode)) {

      char *stable_path = allocer_strdup(alc, target_path);
      if (stable_path) {
        traverse_dir_for_clean(alc, stable_path, exclusions, style_file,
                               &path_builder);
      }
    } else {
      if (is_cleanable_file(target_path)) {
        clean_single_file(alc, target_path, style_file);
      }
    }
  }

  string_destroy(&path_builder);
  return true;
}