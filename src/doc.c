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
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

// =============================================================================
// 1. 数据结构
// =============================================================================

typedef struct
{
  str_slice_t filepath;
  str_slice_t comment;
  str_slice_t signature;
} doc_entry_t;

// =============================================================================
// 2. 状态机和解析器
// =============================================================================

typedef enum
{
  DOC_STATE_CODE,
  DOC_STATE_COMMENT,
  DOC_STATE_SIGNATURE,
} doc_parse_state_t;

static const char *skip_whitespace(const char *p, const char *end)
{
  while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
  {
    p++;
  }
  return p;
}

static void parse_file_for_docs(allocer_t *alc, vec_t *entries, str_slice_t file_content, const char *filepath)
{

  doc_parse_state_t state = DOC_STATE_CODE;
  const char *p = file_content.ptr;
  const char *end = file_content.ptr + file_content.len;

  const char *comment_start = NULL;
  const char *comment_end = NULL; // <-- [FIX #2]
  const char *signature_start = NULL;

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
        comment_start = p + 3; // 跳过 "/**"
        p += 2;
      }
      break;

    case DOC_STATE_COMMENT:
      if (c == '*' && next == '/')
      {
        state = DOC_STATE_SIGNATURE;

        comment_end = p; // <-- [FIX #2] 记录 'p' (它指向 '*')

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

        size_t path_len = strlen(filepath);
        char *path_copy = allocer_alloc(alc, layout_of_array(char, path_len + 1));
        if (!path_copy)
          return;
        memcpy(path_copy, filepath, path_len);
        path_copy[path_len] = '\0';
        entry->filepath = (str_slice_t){.ptr = path_copy, .len = path_len};

        // [FIX #2] 使用 comment_end 来计算长度
        entry->comment = (str_slice_t){.ptr = comment_start, .len = (size_t)(comment_end - comment_start)};
        entry->signature = (str_slice_t){.ptr = signature_start, .len = (size_t)(p - signature_start + 1)};

        if (!vec_push(entries, (void *)entry))
        {
          return;
        }

        state = DOC_STATE_CODE;
        comment_start = NULL;
        comment_end = NULL; // <-- [FIX #2]
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
// 3. 目录遍历
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
 * @brief (核心) 递归遍历目录，解析文件
 *
 * [FIX #1] 'path_builder' 仅用于构建新路径,
 * 'current_path' 是一个稳定的、复制过的 C 字符串
 */
static void traverse_directory(allocer_t *alc, vec_t *entries,
                               const char *current_path, string_t *path_builder)
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
    {
      continue;
    }

    // [FIX #1] 使用 string_builder 来构建路径,
    // 但不要依赖它在循环中保持 'current_path'

    // 1. 清空 builder
    string_clear(path_builder);
    // 2. 添加稳定的父路径
    string_append_cstr(path_builder, current_path);
    // 3. 添加斜杠和文件名
    string_push(path_builder, '/');
    string_append_cstr(path_builder, name);

    const char *full_path = string_as_cstr(path_builder);

    struct stat statbuf;
    if (stat(full_path, &statbuf) != 0)
    {
      fprintf(stderr, "Warning: Could not stat file '%s'\n", full_path);
      continue; // full_path 指针在这里还是有效的
    }

    if (S_ISDIR(statbuf.st_mode))
    {
      // 是目录：递归。
      // full_path 指向的内存可能会在递归调用中失效，
      // 但 full_path C 字符串本身的内容 (e.g., "src/foo")
      // 是我们需要的，所以我们在这里调用 traverse_directory
      traverse_directory(alc, entries, full_path, path_builder);
    }
    else if (has_doc_extension(full_path))
    {
      str_slice_t content;
      if (read_file_to_slice(alc, full_path, &content))
      {
        parse_file_for_docs(alc, entries, content, full_path);
      }
      else
      {
        fprintf(stderr, "Warning: Could not read file '%s'\n", full_path);
      }
    }

    // 路径恢复? 不需要了!
    // 我们在循环开始时就用 string_clear() 重置了 path_builder。
  }

  closedir(dir);
}

// =============================================================================
// 4. Markdown 生成
// =============================================================================

static bool generate_markdown(allocer_t *alc, vec_t *entries, const char *out_path)
{
  string_t md;
  string_init(&md, alc, 4096);

  string_append_cstr(&md, "# API Documentation\n\n");
  string_append_cstr(&md, "Generated by `cnote`.\n\n");

  for (size_t i = 0; i < vec_count(entries); i++)
  {
    doc_entry_t *entry = (void *)vec_get(entries, i);

    string_append_cstr(&md, "## `");
    string_append_slice(&md, entry->signature);
    string_append_cstr(&md, "`\n\n");

    string_append_cstr(&md, "*Source: ");
    string_append_slice(&md, entry->filepath);
    string_append_cstr(&md, "*\n\n");

    // [FIX #2] entry->comment 现在是正确的 slice
    string_append_slice(&md, entry->comment);
    string_append_cstr(&md, "\n\n---\n\n");
  }

  str_slice_t md_slice = string_as_slice(&md);
  bool ok = write_file_bytes(out_path, (const void *)md_slice.ptr, md_slice.len);

  string_destroy(&md);
  return ok;
}

// =============================================================================
// 5. 主入口
// =============================================================================

bool cnote_doc_run(allocer_t *alc, const char *src_dir, const char *out_path)
{

  vec_t entries;
  if (!vec_init(&entries, alc, 0))
  {
    fprintf(stderr, "Error: Failed to initialize vector (OOM?)\n");
    return false;
  }

  // 路径构建器
  string_t path_builder;
  if (!string_init(&path_builder, alc, 256))
  {
    vec_destroy(&entries);
    return false;
  }
  // (我们不再预填充 path_builder)

  printf("  Scanning `%s`...\n", src_dir);

  // [FIX #1]
  // 我们将 `src_dir` (一个稳定的 const char*) 作为
  // 初始路径传递。
  // `path_builder` (一个临时的 builder) 也被传入。
  traverse_directory(alc, &entries, src_dir, &path_builder);

  printf("  Found %zu documentation entries.\n", vec_count(&entries));

  if (vec_count(&entries) > 0)
  {
    printf("  Generating markdown to `%s`...\n", out_path);
    if (!generate_markdown(alc, &entries, out_path))
    {
      fprintf(stderr, "Error: Failed to write markdown file.\n");
      string_destroy(&path_builder);
      vec_destroy(&entries);
      return false;
    }
  }
  else
  {
    printf("  No entries found, skipping markdown generation.\n");
  }

  string_destroy(&path_builder);
  vec_destroy(&entries);

  return true;
}