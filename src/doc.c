// src/doc.c

#include <doc.h>

// --- fluf 模块 ---
#include <std/string/string.h>
#include <std/string/str_slice.h>
#include <std/vec.h>
#include <std/io/file.h>
#include <core/msg/asrt.h>
#include <core/mem/layout.h>

// --- POSIX C API (用于目录) ---
#include <dirent.h>
#include <sys/stat.h> // mkdir
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h> // 确保 errno 被包含

// =============================================================================
// 辅助函数
// =============================================================================

static inline char *
allocer_strdup(allocer_t *alc, const char *s)
{
  size_t len = strlen(s);
  layout_t layout = layout_of_array(char, len + 1);
  char *new_s = allocer_alloc(alc, layout);
  if (new_s)
  {
    memcpy(new_s, s, len + 1);
  }
  return new_s;
}

static bool ensure_directory(const char *path)
{
  if (mkdir(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == 0)
  {
    return true;
  }
  if (errno == EEXIST)
  {
    return true;
  }
  perror("mkdir");
  fprintf(stderr, "Failed to create directory: %s\n", path);
  return false;
}

static void sanitize_path_to_filename(str_slice_t path, string_t *out_builder)
{
  for (size_t i = 0; i < path.len; i++)
  {
    char c = path.ptr[i];
    if (c == '/' || c == '.')
    {
      string_push(out_builder, '_');
    }
    else
    {
      string_push(out_builder, c);
    }
  }
  string_append_cstr(out_builder, ".md");
}

// =============================================================================
// 解析器 (已修改)
// =============================================================================

/**
 * @brief [修改] doc_entry_t 不再需要存储 filepath
 */
typedef struct
{
  str_slice_t comment;
  str_slice_t signature;
} doc_entry_t;

typedef enum
{
  DOC_STATE_CODE,
  DOC_STATE_COMMENT,
  DOC_STATE_SIGNATURE
} doc_parse_state_t;

static const char *skip_whitespace(const char *p, const char *end)
{
  while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
  {
    p++;
  }
  return p;
}

/**
 * @brief [修改] 不再需要 filepath, 因为它只解析
 */
static void parse_file_for_docs(allocer_t *alc, vec_t *entries_vec, str_slice_t file_content)
{
  doc_parse_state_t state = DOC_STATE_CODE;
  const char *p = file_content.ptr;
  const char *end = file_content.ptr + file_content.len;

  const char *comment_start = NULL, *comment_end = NULL, *signature_start = NULL;

  while (p < end)
  {
    char c = *p;
    char next = (p + 1 < end) ? *(p + 1) : '\0';
    char next2 = (p + 2 < end) ? *(p + 2) : '\0';
    switch (state)
    {
    case DOC_STATE_CODE:
      if (c == '/' && next == '*' && next2 == '*')
      {
        state = DOC_STATE_COMMENT;
        comment_start = p + 3;
        p += 2;
      }
      break;
    case DOC_STATE_COMMENT:
      if (c == '*' && next == '/')
      {
        state = DOC_STATE_SIGNATURE;
        comment_end = p;
        signature_start = skip_whitespace(p + 2, end);
        p += 1;
      }
      break;
    case DOC_STATE_SIGNATURE:
      if (c == '{' || c == ';')
      {
        doc_entry_t *entry = allocer_alloc(alc, layout_of(doc_entry_t));
        if (!entry)
          return;
        entry->comment = (str_slice_t){.ptr = comment_start, .len = (size_t)(comment_end - comment_start)};
        entry->signature = (str_slice_t){.ptr = signature_start, .len = (size_t)(p - signature_start + 1)};
        if (!vec_push(entries_vec, (void *)entry))
          return;
        state = DOC_STATE_CODE;
        comment_start = NULL;
        comment_end = NULL;
        signature_start = NULL;
      }
      if (c == '/' && next == '*' && next2 == '*')
      {
        state = DOC_STATE_CODE;
        p--;
      }
      break;
    }
    p++;
  }
}

// =============================================================================
// Markdown 生成 (已修改)
// =============================================================================

static str_slice_t slice_trim_whitespace_left(str_slice_t s)
{
  const char *p = s.ptr;
  const char *end = s.ptr + s.len;
  while (p < end && (*p == ' ' || *p == '\t'))
  {
    p++;
  }
  return (str_slice_t){.ptr = p, .len = (size_t)(end - p)};
}

static bool slice_starts_with_lit(str_slice_t s, const char *lit)
{
  size_t lit_len = strlen(lit);
  if (s.len < lit_len)
    return false;
  return strncmp(s.ptr, lit, lit_len) == 0;
}

static void string_append_compact_slice(string_t *md, str_slice_t slice)
{
  const char *p = slice.ptr;
  const char *end = slice.ptr + slice.len;
  bool last_was_space = false;
  p = skip_whitespace(p, end);
  while (p < end)
  {
    char c = *p++;
    if (c == '\n' || c == '\t' || c == '\r')
    {
      if (!last_was_space)
      {
        string_push(md, ' ');
        last_was_space = true;
      }
    }
    else if (c == ' ')
    {
      if (!last_was_space)
      {
        string_push(md, ' ');
        last_was_space = true;
      }
    }
    else
    {
      string_push(md, c);
      last_was_space = false;
    }
  }
}

typedef enum
{
  TAG_STATE_NONE,
  TAG_STATE_LIST,
  TAG_STATE_EXAMPLE
} comment_parse_state_t;

static void format_comment(string_t *md, str_slice_t comment)
{
  const char *p = comment.ptr;
  const char *end = comment.ptr + comment.len;
  comment_parse_state_t state = TAG_STATE_NONE;
  while (p < end)
  {
    const char *line_end = p;
    while (line_end < end && *line_end != '\n')
    {
      line_end++;
    }
    str_slice_t line = {.ptr = p, .len = (size_t)(line_end - p)};
    str_slice_t stripped_line = slice_trim_whitespace_left(line);
    if (stripped_line.len > 0 && stripped_line.ptr[0] == '*')
    {
      stripped_line.ptr++;
      stripped_line.len--;
      if (stripped_line.len > 0 && stripped_line.ptr[0] == ' ')
      {
        stripped_line.ptr++;
        stripped_line.len--;
      }
    }
    bool is_new_tag = (stripped_line.len > 0 && stripped_line.ptr[0] == '@');
    if (state == TAG_STATE_EXAMPLE)
    {
      if (is_new_tag)
      {
        string_append_cstr(md, "```\n\n");
        state = TAG_STATE_NONE;
      }
      else
      {
        string_append_slice(md, line);
        string_push(md, '\n');
        p = line_end;
        if (p < end)
          p++;
        continue;
      }
    }
    str_slice_t tag_body = slice_trim_whitespace_left(stripped_line);
    if (slice_starts_with_lit(tag_body, "@brief"))
    {
      state = TAG_STATE_NONE;
      tag_body.ptr += 6;
      tag_body.len -= 6;
      tag_body = slice_trim_whitespace_left(tag_body);
      string_append_slice(md, tag_body);
      string_push(md, '\n');
    }
    else if (slice_starts_with_lit(tag_body, "@param"))
    {
      if (state != TAG_STATE_LIST)
      {
        string_push(md, '\n');
      }
      state = TAG_STATE_LIST;
      tag_body.ptr += 6;
      tag_body.len -= 6;
      tag_body = slice_trim_whitespace_left(tag_body);
      const char *name_end = tag_body.ptr;
      while (name_end < tag_body.ptr + tag_body.len && *name_end != ' ' && *name_end != '\t')
      {
        name_end++;
      }
      str_slice_t name_slice = {.ptr = tag_body.ptr, .len = (size_t)(name_end - tag_body.ptr)};
      str_slice_t desc_slice = {.ptr = name_end, .len = (size_t)((tag_body.ptr + tag_body.len) - name_end)};
      desc_slice = slice_trim_whitespace_left(desc_slice);
      string_append_cstr(md, "- **`");
      string_append_slice(md, name_slice);
      string_append_cstr(md, "`**: ");
      string_append_slice(md, desc_slice);
      string_push(md, '\n');
    }
    else if (slice_starts_with_lit(tag_body, "@return"))
    {
      if (state != TAG_STATE_LIST)
      {
        string_push(md, '\n');
      }
      state = TAG_STATE_LIST;
      tag_body.ptr += 7;
      tag_body.len -= 7;
      tag_body = slice_trim_whitespace_left(tag_body);
      string_append_cstr(md, "- **Returns**: ");
      string_append_slice(md, tag_body);
      string_push(md, '\n');
    }
    else if (slice_starts_with_lit(tag_body, "@note"))
    {
      state = TAG_STATE_NONE;
      tag_body.ptr += 5;
      tag_body.len -= 5;
      tag_body = slice_trim_whitespace_left(tag_body);
      string_append_cstr(md, "\n> **Note:** ");
      string_append_slice(md, tag_body);
      string_push(md, '\n');
    }
    else if (slice_starts_with_lit(tag_body, "@example"))
    {
      state = TAG_STATE_EXAMPLE;
      string_append_cstr(md, "\n**Example:**\n\n```c\n");
    }
    else if (tag_body.len > 0)
    {
      state = TAG_STATE_NONE;
      string_append_slice(md, tag_body);
      string_push(md, '\n');
    }
    else
    {
      state = TAG_STATE_NONE;
      string_push(md, '\n');
    }
    p = line_end;
    if (p < end)
      p++;
  }
  if (state == TAG_STATE_EXAMPLE)
  {
    string_append_cstr(md, "```\n");
  }
}

/**
 * @brief [修改] 移除了 *Source: ...*
 */
static bool generate_markdown_for_file(
    allocer_t *alc,
    vec_t *entries,
    str_slice_t relative_path, // (这个 slice 必须是稳定的)
    const char *md_file_path)
{
  string_t md;
  string_init(&md, alc, 4096);

  string_append_cstr(&md, "# ");
  string_append_slice(&md, relative_path);
  string_append_cstr(&md, "\n\n");

  for (size_t i = 0; i < vec_count(entries); i++)
  {
    doc_entry_t *entry = (void *)vec_get(entries, i);

    string_append_cstr(&md, "## `");
    string_append_compact_slice(&md, entry->signature);
    string_append_cstr(&md, "`\n\n");

    // [修改] 移除 Source

    format_comment(&md, entry->comment);
    string_append_cstr(&md, "\n---\n\n");
  }

  str_slice_t md_slice = string_as_slice(&md);
  bool ok = write_file_bytes(md_file_path, (const void *)md_slice.ptr, md_slice.len);

  string_destroy(&md);
  return ok;
}

// =============================================================================
// [新] 目录遍历 (已修复生命周期 Bug)
// =============================================================================

static bool has_doc_extension(const char *filename)
{
  const char *dot = strrchr(filename, '.');
  if (!dot)
    return false;
  if (strcmp(dot, ".c") == 0)
    return true;
  if (strcmp(dot, ".h") == 0)
    return true;
  return false;
}

/**
 * @brief [重构] 遍历并立即处理文件
 */
static void traverse_and_process(
    allocer_t *alc,
    const char *current_path,  // "src/core"
    const char *base_path,     // "src" (用于计算相对路径)
    const char *api_out_dir,   // "docs/api"
    string_t *summary_builder, // (用于 SUMMARY.md)
    string_t *path_builder)    // (用于构建路径)
{
  DIR *dir = opendir(current_path);
  if (!dir)
  {
    fprintf(stderr, "Warning: Could not open directory '%s'\n", current_path);
    return;
  }

  struct dirent *dp;
  while ((dp = readdir(dir)) != NULL)
  {
    const char *name = dp->d_name;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
      continue;

    string_clear(path_builder);
    string_append_cstr(path_builder, current_path);
    if (current_path[strlen(current_path) - 1] != '/')
      string_push(path_builder, '/');
    string_append_cstr(path_builder, name);

    // full_path 仍然是一个指向 path_builder 内部的 *临时* 指针
    const char *full_path = string_as_cstr(path_builder);

    struct stat statbuf;
    if (stat(full_path, &statbuf) != 0)
    {
      fprintf(stderr, "Warning: Could not stat file '%s'\n", full_path);
      continue;
    }

    if (S_ISDIR(statbuf.st_mode))
    {
      char *stable_path = allocer_strdup(alc, full_path);
      if (stable_path)
      {
        // base_path 保持不变 (e.g., "include")
        traverse_and_process(alc, stable_path, base_path, api_out_dir, summary_builder, path_builder);
      }
    }
    else if (has_doc_extension(full_path))
    {

      // --- [BUG 修复] ---
      // 1. 计算相对路径 (仍然是临时的)
      const char *relative_path_ptr = full_path + strlen(base_path);
      if (relative_path_ptr[0] == '/')
        relative_path_ptr++;

      // 2. 将其复制到 Arena, 使其变为 *稳定*
      char *stable_relative_path = allocer_strdup(alc, relative_path_ptr);
      if (!stable_relative_path)
        continue; // OOM

      // 3. 创建一个指向稳定内存的 *安全* 切片
      str_slice_t relative_path_slice = slice_from_cstr(stable_relative_path);
      // --- [修复结束] ---

      str_slice_t content;
      if (!read_file_to_slice(alc, full_path, &content))
      {
        fprintf(stderr, "Warning: Could not read file '%s'\n", full_path);
        continue;
      }

      vec_t entries;
      if (!vec_init(&entries, alc, 0))
        continue;
      // [修改] 移除 filepath 参数
      parse_file_for_docs(alc, &entries, content);

      if (vec_count(&entries) > 0)
      {
        // builder 们现在可以安全地使用
        // (path_builder, sanitized_name, summary_builder)

        string_t sanitized_name;
        string_init(&sanitized_name, alc, relative_path_slice.len + 4);
        // [修改] 传入安全的 slice
        sanitize_path_to_filename(relative_path_slice, &sanitized_name);

        string_clear(path_builder); // (重用 path_builder)
        string_append_cstr(path_builder, api_out_dir);
        if (api_out_dir[strlen(api_out_dir) - 1] != '/')
          string_push(path_builder, '/');
        string_append_slice(path_builder, string_as_slice(&sanitized_name));
        const char *md_file_path = string_as_cstr(path_builder);

        // B. 生成 MD
        // [修改] 传入安全的 slice
        generate_markdown_for_file(alc, &entries, relative_path_slice, md_file_path);

        // C. 添加到 SUMMARY.md
        // [修改] 传入安全的 slice
        string_append_cstr(summary_builder, "  - [");
        string_append_slice(summary_builder, relative_path_slice);
        string_append_cstr(summary_builder, "](api/");
        string_append_slice(summary_builder, string_as_slice(&sanitized_name));
        string_append_cstr(summary_builder, ")\n");

        string_destroy(&sanitized_name);
      }

      vec_destroy(&entries);
    }
  }
  closedir(dir);
}

// =============================================================================
// 主入口
// =============================================================================

bool cnote_doc_run(allocer_t *alc, const char *src_dir, const char *out_dir)
{

  string_t path_builder;
  string_t summary_builder;
  string_t api_dir_builder;

  if (!string_init(&path_builder, alc, 256) ||
      !string_init(&summary_builder, alc, 1024) ||
      !string_init(&api_dir_builder, alc, 64))
  {
    return false;
  }

  if (!ensure_directory(out_dir))
    return false;

  string_append_cstr(&api_dir_builder, out_dir);
  if (out_dir[strlen(out_dir) - 1] != '/')
    string_push(&api_dir_builder, '/');
  string_append_cstr(&api_dir_builder, "api");
  const char *api_out_dir = string_as_cstr(&api_dir_builder);

  if (!ensure_directory(api_out_dir))
    return false;

  string_append_cstr(&summary_builder, "# API Reference\n\n");

  printf("  Scanning `%s`...\n", src_dir);
  char *stable_src_dir = allocer_strdup(alc, src_dir);
  if (!stable_src_dir)
    return false;

  traverse_and_process(
      alc,
      stable_src_dir,
      stable_src_dir,
      api_out_dir,
      &summary_builder,
      &path_builder);

  printf("  Writing SUMMARY.md to `%s`...\n", out_dir);
  string_clear(&path_builder);
  string_append_cstr(&path_builder, out_dir);
  if (out_dir[strlen(out_dir) - 1] != '/')
    string_push(&path_builder, '/');
  string_append_cstr(&path_builder, "SUMMARY.md");

  str_slice_t summary_slice = string_as_slice(&summary_builder);
  write_file_bytes(string_as_cstr(&path_builder), (const void *)summary_slice.ptr, summary_slice.len);

  string_destroy(&path_builder);
  string_destroy(&summary_builder);
  string_destroy(&api_dir_builder);

  return true;
}