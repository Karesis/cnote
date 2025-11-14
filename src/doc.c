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



#include <doc.h>

#include <core/mem/layout.h>
#include <core/msg/asrt.h>
#include <std/io/file.h>
#include <std/string/str_slice.h>
#include <std/string/string.h>
#include <std/vec.h>

#include <dirent.h>
#include <stdio.h>
#include <string.h>
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

typedef struct {
  str_slice_t filepath;
  str_slice_t comment;
  str_slice_t signature;
} doc_entry_t;

typedef enum {
  DOC_STATE_CODE,
  DOC_STATE_COMMENT,
  DOC_STATE_SIGNATURE,
} doc_parse_state_t;

static const char *skip_whitespace(const char *p, const char *end) {
  while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
    p++;
  }
  return p;
}

static void parse_file_for_docs(allocer_t *alc, vec_t *entries,
                                str_slice_t file_content,
                                const char *filepath) {

  doc_parse_state_t state = DOC_STATE_CODE;
  const char *p = file_content.ptr;
  const char *end = file_content.ptr + file_content.len;

  const char *comment_start = NULL;
  const char *comment_end = NULL;
  const char *signature_start = NULL;

  while (p < end) {
    char c = *p;
    char next = (p + 1 < end) ? *(p + 1) : '\0';
    char next2 = (p + 2 < end) ? *(p + 2) : '\0';

    switch (state) {
    case DOC_STATE_CODE:
      if (c == '/' && next == '*' && next2 == '*') {
        state = DOC_STATE_COMMENT;
        comment_start = p + 3;
        p += 2;
      }
      break;
    case DOC_STATE_COMMENT:
      if (c == '*' && next == '/') {
        state = DOC_STATE_SIGNATURE;
        comment_end = p;
        signature_start = skip_whitespace(p + 2, end);
        p += 1;
      }
      break;
    case DOC_STATE_SIGNATURE:
      if (c == '{' || c == ';') {
        doc_entry_t *entry = allocer_alloc(alc, layout_of(doc_entry_t));
        if (!entry)
          return;

        entry->filepath = slice_from_cstr(filepath);

        entry->comment = (str_slice_t){
            .ptr = comment_start, .len = (size_t)(comment_end - comment_start)};
        entry->signature = (str_slice_t){
            .ptr = signature_start, .len = (size_t)(p - signature_start + 1)};

        if (!vec_push(entries, (void *)entry))
          return;

        state = DOC_STATE_CODE;
        comment_start = NULL;
        comment_end = NULL;
        signature_start = NULL;
      }
      if (c == '/' && next == '*' && next2 == '*') {
        state = DOC_STATE_CODE;
        p--;
      }
      break;
    }
    p++;
  }
}

static bool has_doc_extension(const char *filename) {
  const char *dot = strrchr(filename, '.');
  if (!dot)
    return false;
  if (strcmp(dot, ".c") == 0)
    return true;
  if (strcmp(dot, ".h") == 0)
    return true;
  return false;
}

static void traverse_directory(allocer_t *alc, vec_t *entries,
                               const char *current_path,
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

    size_t len = strlen(current_path);
    if (len > 0 && current_path[len - 1] != '/') {
      string_push(path_builder, '/');
    }

    string_append_cstr(path_builder, name);

    const char *full_path = string_as_cstr(path_builder);

    struct stat statbuf;
    if (stat(full_path, &statbuf) != 0) {
      fprintf(stderr, "Warning: Could not stat file '%s'\n", full_path);
      continue;
    }

    if (S_ISDIR(statbuf.st_mode)) {

      char *stable_path = allocer_strdup(alc, full_path);
      if (stable_path) {
        traverse_directory(alc, entries, stable_path, path_builder);
      }
    } else if (has_doc_extension(full_path)) {
      str_slice_t content;
      if (read_file_to_slice(alc, full_path, &content)) {

        char *stable_path = allocer_strdup(alc, full_path);
        if (stable_path) {
          parse_file_for_docs(alc, entries, content, stable_path);
        }
      } else {
        fprintf(stderr, "Warning: Could not read file '%s'\n", full_path);
      }
    }
  }
  closedir(dir);
}

static str_slice_t slice_trim_whitespace_left(str_slice_t s) {
  const char *p = s.ptr;
  const char *end = s.ptr + s.len;
  while (p < end && (*p == ' ' || *p == '\t')) {
    p++;
  }
  return (str_slice_t){.ptr = p, .len = (size_t)(end - p)};
}

static bool slice_starts_with_lit(str_slice_t s, const char *lit) {
  size_t lit_len = strlen(lit);
  if (s.len < lit_len)
    return false;
  return strncmp(s.ptr, lit, lit_len) == 0;
}

static void string_append_compact_slice(string_t *md, str_slice_t slice) {
  const char *p = slice.ptr;
  const char *end = slice.ptr + slice.len;
  bool last_was_space = false;
  p = skip_whitespace(p, end);
  while (p < end) {
    char c = *p++;
    if (c == '\n' || c == '\t' || c == '\r') {
      if (!last_was_space) {
        string_push(md, ' ');
        last_was_space = true;
      }
    } else if (c == ' ') {
      if (!last_was_space) {
        string_push(md, ' ');
        last_was_space = true;
      }
    } else {
      string_push(md, c);
      last_was_space = false;
    }
  }
}

static void format_comment(string_t *md, str_slice_t comment) {
  const char *p = comment.ptr;
  const char *end = comment.ptr + comment.len;
  bool in_list = false;
  while (p < end) {
    const char *line_end = p;
    while (line_end < end && *line_end != '\n') {
      line_end++;
    }
    str_slice_t line = {.ptr = p, .len = (size_t)(line_end - p)};
    line = slice_trim_whitespace_left(line);
    if (line.len > 0 && line.ptr[0] == '*') {
      line.ptr++;
      line.len--;
      if (line.len > 0 && line.ptr[0] == ' ') {
        line.ptr++;
        line.len--;
      }
    }
    line = slice_trim_whitespace_left(line);

    if (slice_starts_with_lit(line, "@brief")) {
      in_list = false;
      line.ptr += 6;
      line.len -= 6;
      line = slice_trim_whitespace_left(line);
      string_append_slice(md, line);
      string_push(md, '\n');
    } else if (slice_starts_with_lit(line, "@param")) {
      if (!in_list) {
        string_push(md, '\n');
        in_list = true;
      }
      line.ptr += 6;
      line.len -= 6;
      line = slice_trim_whitespace_left(line);
      const char *name_end = line.ptr;
      while (name_end < line.ptr + line.len && *name_end != ' ' &&
             *name_end != '\t') {
        name_end++;
      }
      str_slice_t name_slice = {.ptr = line.ptr,
                                .len = (size_t)(name_end - line.ptr)};
      str_slice_t desc_slice = {
          .ptr = name_end, .len = (size_t)((line.ptr + line.len) - name_end)};
      desc_slice = slice_trim_whitespace_left(desc_slice);
      string_append_cstr(md, "- **`");
      string_append_slice(md, name_slice);
      string_append_cstr(md, "`**: ");
      string_append_slice(md, desc_slice);
      string_push(md, '\n');
    } else if (slice_starts_with_lit(line, "@return")) {
      if (!in_list) {
        string_push(md, '\n');
        in_list = true;
      }
      line.ptr += 7;
      line.len -= 7;
      line = slice_trim_whitespace_left(line);
      string_append_cstr(md, "- **Returns**: ");
      string_append_slice(md, line);
      string_push(md, '\n');
    } else if (line.len > 0) {
      in_list = false;
      string_append_slice(md, line);
      string_push(md, '\n');
    } else {
      in_list = false;
      string_push(md, '\n');
    }
    p = line_end;
    if (p < end && *p == '\n') {
      p++;
    }
  }
}

static bool generate_markdown(allocer_t *alc, vec_t *entries,
                              const char *out_path) {
  string_t md;
  string_init(&md, alc, 4096);
  string_append_cstr(&md, "# API Documentation\n\n");
  string_append_cstr(&md, "Generated by `cnote`.\n\n");
  for (size_t i = 0; i < vec_count(entries); i++) {
    doc_entry_t *entry = (void *)vec_get(entries, i);
    string_append_cstr(&md, "## `");
    string_append_compact_slice(&md, entry->signature);
    string_append_cstr(&md, "`\n\n");
    string_append_cstr(&md, "*Source: ");
    string_append_slice(&md, entry->filepath);
    string_append_cstr(&md, "*\n\n");
    format_comment(&md, entry->comment);
    string_append_cstr(&md, "\n---\n\n");
  }
  str_slice_t md_slice = string_as_slice(&md);
  bool ok =
      write_file_bytes(out_path, (const void *)md_slice.ptr, md_slice.len);
  string_destroy(&md);
  return ok;
}

bool cnote_doc_run(allocer_t *alc, const char *src_dir, const char *out_path) {

  vec_t entries;
  if (!vec_init(&entries, alc, 0)) {
    fprintf(stderr, "Error: Failed to initialize vector (OOM?)\n");
    return false;
  }
  string_t path_builder;
  if (!string_init(&path_builder, alc, 256)) {
    vec_destroy(&entries);
    return false;
  }
  printf("  Scanning `%s`...\n", src_dir);

  char *stable_src_dir = allocer_strdup(alc, src_dir);
  if (!stable_src_dir) {
    string_destroy(&path_builder);
    vec_destroy(&entries);
    return false;
  }

  traverse_directory(alc, &entries, stable_src_dir, &path_builder);

  printf("  Found %zu documentation entries.\n", vec_count(&entries));
  if (vec_count(&entries) > 0) {
    printf("  Generating markdown to `%s`...\n", out_path);
    if (!generate_markdown(alc, &entries, out_path)) {
      fprintf(stderr, "Error: Failed to write markdown file.\n");
      string_destroy(&path_builder);
      vec_destroy(&entries);
      return false;
    }
  } else {
    printf("  No entries found, skipping markdown generation.\n");
  }

  string_destroy(&path_builder);
  vec_destroy(&entries);
  return true;
}