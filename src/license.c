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



#include <license.h>

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

static bool slice_starts_with_slice(str_slice_t s, str_slice_t prefix) {
  if (prefix.len > s.len)
    return false;
  return memcmp(s.ptr, prefix.ptr, prefix.len) == 0;
}

static bool slice_starts_with_lit(str_slice_t s, const char *lit) {
  size_t lit_len = strlen(lit);
  if (lit_len > s.len)
    return false;
  return strncmp(s.ptr, lit, lit_len) == 0;
}

static ssize_t find_first_block_comment_end(str_slice_t s) {
  for (size_t i = 0; i + 1 < s.len; i++) {
    if (s.ptr[i] == '*' && s.ptr[i + 1] == '/') {
      return (ssize_t)i;
    }
  }
  return -1;
}

static const char *skip_whitespace(const char *p, const char *end) {
  while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
    p++;
  }
  return p;
}

static void format_license_as_comment(allocer_t *alc, str_slice_t raw_license,
                                      string_t *out_builder) {
  (void)alc;
  string_append_cstr(out_builder, "/*\n");
  const char *p = raw_license.ptr;
  const char *end = raw_license.ptr + raw_license.len;
  while (p < end) {
    string_append_cstr(out_builder, " * ");
    const char *line_end = p;
    while (line_end < end && *line_end != '\n') {
      line_end++;
    }
    str_slice_t line = {.ptr = p, .len = (size_t)(line_end - p)};
    string_append_slice(out_builder, line);
    string_push(out_builder, '\n');
    p = line_end;
    if (p < end && *p == '\n') {
      p++;
    }
  }
  string_append_cstr(out_builder, " */\n\n");
}

static bool is_licensable_file(const char *filename) {
  const char *dot = strrchr(filename, '.');
  if (!dot)
    return false;
  if (strcmp(dot, ".c") == 0)
    return true;
  if (strcmp(dot, ".h") == 0)
    return true;
  return false;
}

static bool apply_license_to_file(allocer_t *alc, const char *filepath,
                                  str_slice_t golden_header_slice) {

  str_slice_t file_content;
  if (!read_file_to_slice(alc, filepath, &file_content)) {
    fprintf(stderr, "Warning: Could not read file '%s'\n", filepath);
    return false;
  }
  str_slice_t rest_of_file;
  bool needs_write = false;
  if (slice_starts_with_slice(file_content, golden_header_slice)) {
    printf("  License OK: %s\n", filepath);
    return true;
  }
  if (slice_starts_with_lit(file_content, "/*")) {
    printf("  Updating license: %s\n", filepath);
    needs_write = true;
    ssize_t end_pos = find_first_block_comment_end(file_content);
    if (end_pos == -1) {
      fprintf(stderr,
              "Warning: Skipping '%s' (malformed block comment at start)\n",
              filepath);
      return false;
    }
    const char *after_comment = file_content.ptr + end_pos + 2;
    const char *content_start =
        skip_whitespace(after_comment, file_content.ptr + file_content.len);
    rest_of_file = (str_slice_t){
        .ptr = content_start,
        .len = (size_t)((file_content.ptr + file_content.len) - content_start)};
  } else {
    printf("  Adding license: %s\n", filepath);
    needs_write = true;
    rest_of_file = file_content;
  }
  if (needs_write) {
    string_t builder;
    string_init(&builder, alc, golden_header_slice.len + rest_of_file.len + 1);
    string_append_slice(&builder, golden_header_slice);
    string_append_slice(&builder, rest_of_file);
    str_slice_t new_content = string_as_slice(&builder);
    bool ok = write_file_bytes(filepath, (const void *)new_content.ptr,
                               new_content.len);
    string_destroy(&builder);
    return ok;
  }
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

/**
 * @brief (辅助) 递归遍历目录
 */
static void traverse_dir_for_license(allocer_t *alc, const char *current_path,
                                     vec_t *exclusions,
                                     str_slice_t golden_header_slice,
                                     string_t *path_builder) {
  DIR *dir = opendir(current_path);
  if (!dir) {
    fprintf(stderr, "Warning: Could not open directory '%s'\n", current_path);
    return;
  }

  struct dirent *dp;
  while ((dp = readdir(dir)) != NULL) {
    const char *name = dp->d_name;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
      continue;

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
        traverse_dir_for_license(alc, stable_path, exclusions,
                                 golden_header_slice, path_builder);
      }
    } else if (is_licensable_file(full_path)) {
      apply_license_to_file(alc, full_path, golden_header_slice);
    }
  }
  closedir(dir);
}

/**
 * @brief 'license' 命令的入口函数
 */
bool cnote_license_run(allocer_t *alc, vec_t *targets, vec_t *exclusions,
                       const char *license_file) {

  string_t golden_header;
  if (!string_init(&golden_header, alc, 1024))
    return false;

  str_slice_t raw_license;
  if (!read_file_to_slice(alc, license_file, &raw_license)) {
    fprintf(stderr, "Error: Failed to read license file '%s'\n", license_file);
    string_destroy(&golden_header);
    return false;
  }

  format_license_as_comment(alc, raw_license, &golden_header);
  str_slice_t golden_slice = string_as_slice(&golden_header);

  string_t path_builder;
  if (!string_init(&path_builder, alc, 256)) {
    string_destroy(&golden_header);
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
        traverse_dir_for_license(alc, stable_path, exclusions, golden_slice,
                                 &path_builder);
      }
    } else {
      if (is_licensable_file(target_path)) {
        apply_license_to_file(alc, target_path, golden_slice);
      }
    }
  }

  string_destroy(&path_builder);
  string_destroy(&golden_header);
  return true;
}