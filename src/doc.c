// src/doc.c

#include <doc.h> // 对应的头文件

// --- fluf 模块 ---
#include <std/string/string.h>
#include <std/string/str_slice.h>
#include <std/vec.h>
#include <std/io/file.h>
#include <core/msg/asrt.h>
#include <core/mem/layout.h> // <-- 用于 layout_of()

// --- POSIX C API (用于目录) ---
#include <dirent.h>   // opendir, readdir, closedir
#include <sys/stat.h> // stat, S_ISDIR
#include <string.h>   // strcmp, strrchr
#include <stdio.h>    // printf
#include <unistd.h>   // stat

// =============================================================================
// 1. 数据结构
// =============================================================================

/**
 * @brief 存储一个解析出的文档条目
 *
 * (这个结构体的实例将由 Arena 分配)
 */
typedef struct
{
  str_slice_t filepath;  // 指向文件路径 (由 Arena 复制)
  str_slice_t comment;   // 指向文档注释内容 (零拷贝，指向文件 slice)
  str_slice_t signature; // 指向 API 签名 (零拷贝，指向文件 slice)
} doc_entry_t;

// =============================================================================
// 2. 状态机和解析器
// =============================================================================

// 解析器状态
typedef enum
{
  DOC_STATE_CODE,      // 正在查找 "/**"
  DOC_STATE_COMMENT,   // 在 "/**" 内部, 正在查找 "*/"
  DOC_STATE_SIGNATURE, // 在 "*/" 之后, 正在查找签名
} doc_parse_state_t;

/**
 * @brief 辅助函数：跳过空白字符
 */
static const char *skip_whitespace(const char *p, const char *end)
{
  while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
  {
    p++;
  }
  return p;
}

/**
 * @brief (核心) 解析单个文件，并将找到的条目添加到 vec 中
 */
static void parse_file_for_docs(allocer_t *alc, vec_t *entries, str_slice_t file_content, const char *filepath)
{

  doc_parse_state_t state = DOC_STATE_CODE;
  const char *p = file_content.ptr;
  const char *end = file_content.ptr + file_content.len;

  const char *comment_start = NULL;
  const char *signature_start = NULL;

  while (p < end)
  {
    char c = *p;
    char next = (p + 1 < end) ? *(p + 1) : '\0';
    char next2 = (p + 2 < end) ? *(p + 2) : '\0';

    switch (state)
    {
    case DOC_STATE_CODE:
      // 规范 1：查找 "/**" (但不是 "/*" 或 "/***" 等)
      if (c == '/' && next == '*' && next2 == '*')
      {
        // 我们找到了文档注释！
        state = DOC_STATE_COMMENT;
        comment_start = p + 3; // 跳过 "/**"
        p += 2;                // 多消耗两个字符
      }
      break;

    case DOC_STATE_COMMENT:
      if (c == '*' && next == '/')
      {
        // 文档注释结束
        state = DOC_STATE_SIGNATURE;

        // 规范 2：查找签名
        // 跳过 "*/" 和紧随其后的任何空白
        signature_start = skip_whitespace(p + 2, end);

        p += 1; // 多消耗一个 '/'
      }
      break;

    case DOC_STATE_SIGNATURE:
      // 规范 2：签名在第一个 '{' 或 ';' 处结束
      if (c == '{' || c == ';')
      {
        // 签名结束，我们找到了一个完整的条目！

        // 1. 分配一个 entry
        doc_entry_t *entry = allocer_alloc(alc, layout_of(doc_entry_t));
        if (!entry)
          return; // OOM

        // 2. 复制文件路径 (因为 filepath 字符串可能在栈上)
        size_t path_len = strlen(filepath);
        char *path_copy = allocer_alloc(alc, layout_of_array(char, path_len + 1));
        if (!path_copy)
          return; // OOM
        memcpy(path_copy, filepath, path_len);
        path_copy[path_len] = '\0';
        entry->filepath = (str_slice_t){.ptr = path_copy, .len = path_len};

        // 3. 设置零拷贝切片
        entry->comment = (str_slice_t){.ptr = comment_start, .len = (size_t)(p - comment_start - 2)};       // (p-2) 指向 "*/" 之前
        entry->signature = (str_slice_t){.ptr = signature_start, .len = (size_t)(p - signature_start + 1)}; // +1 包含 '{' 或 ';'

        // 4. 推入 Vec
        if (!vec_push(entries, (void *)entry))
        {
          // OOM (arena 不太可能发生，但最好检查)
          return;
        }

        // 5. 重置状态机
        state = DOC_STATE_CODE;
        comment_start = NULL;
        signature_start = NULL;
      }
      // 如果在找到签名之前遇到了另一个文档注释，
      // 可能是个错误，但我们暂时忽略它，让状态机在下次循环时重置。
      if (c == '/' && next == '*' && next2 == '*')
      {
        state = DOC_STATE_CODE;
        p--; // 回退一步，让外层循环的 DOC_STATE_CODE 重新处理 "/**"
      }
      break;
    }
    p++;
  }
}

// =============================================================================
// 3. 目录遍历
// =============================================================================

/**
 * @brief 检查文件是否是我们关心的扩展名 (.c, .h)
 */
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
 * @param alc       Arena
 * @param entries   用于存储结果的 Vec
 * @param path_builder (fluf) string_t, 用于构建当前路径 (避免栈溢出)
 */
static void traverse_directory(allocer_t *alc, vec_t *entries, string_t *path_builder)
{
  const char *current_path = string_as_cstr(path_builder);
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

    // 跳过 "." 和 ".."
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
    {
      continue;
    }

    // 记录当前路径长度，以便稍后恢复
    size_t path_len_before = string_count(path_builder);

    // 构建新路径
    string_push(path_builder, '/');
    string_append_cstr(path_builder, name);
    const char *full_path = string_as_cstr(path_builder);

    struct stat statbuf;
    if (stat(full_path, &statbuf) != 0)
    {
      fprintf(stderr, "Warning: Could not stat file '%s'\n", full_path);
      continue;
    }

    if (S_ISDIR(statbuf.st_mode))
    {
      // 是目录：递归
      traverse_directory(alc, entries, path_builder);
    }
    else if (has_doc_extension(full_path))
    {
      // 是 .c 或 .h 文件：解析它
      // printf("Parsing: %s\n", full_path);

      // (fluf) 读文件到 Arena
      str_slice_t content;
      if (read_file_to_slice(alc, full_path, &content))
      {
        parse_file_for_docs(alc, entries, content, full_path);
        // 注意：read_file_to_slice 分配的内存
        // (content.ptr) 在整个程序生命周期内
        // (直到 arena destroy) 都是有效的。
        // 这使得我们的零拷贝切片 (entry->comment) 是安全的。
      }
      else
      {
        fprintf(stderr, "Warning: Could not read file '%s'\n", full_path);
      }
    }

    // 恢复路径 (e.g., 从 "src/foo/bar.c" 恢复到 "src/foo")
    string_clear(path_builder);
    string_append_cstr(path_builder, current_path);
  }

  closedir(dir);
}

// =============================================================================
// 4. Markdown 生成
// =============================================================================

/**
 * @brief (核心) 将所有 entry 写入 Markdown 文件
 */
static bool generate_markdown(allocer_t *alc, vec_t *entries, const char *out_path)
{
  string_t md;
  string_init(&md, alc, 4096); // 4k 初始容量

  string_append_cstr(&md, "# API Documentation\n\n");
  string_append_cstr(&md, "Generated by `cnote`.\n\n");

  for (size_t i = 0; i < vec_count(entries); i++)
  {
    doc_entry_t *entry = (doc_entry_t *)vec_get(entries, i);

    // --- 1. 签名 ---
    string_append_cstr(&md, "## `");
    string_append_slice(&md, entry->signature);
    string_append_cstr(&md, "`\n\n");

    // --- 2. 文件路径 ---
    string_append_cstr(&md, "*Source: ");
    string_append_slice(&md, entry->filepath);
    string_append_cstr(&md, "*\n\n");

    // --- 3. 注释 (TODO: 清理开头的 '*') ---
    // (fluf 没有 split/trim，我们暂时只追加原始注释)
    string_append_slice(&md, entry->comment);
    string_append_cstr(&md, "\n\n---\n\n");
  }

  // (fluf) 写入文件
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

  // 1. 初始化 Vec<doc_entry_t*>
  vec_t entries;
  if (!vec_init(&entries, alc, 0))
  { // 0 = 默认容量
    fprintf(stderr, "Error: Failed to initialize vector (OOM?)\n");
    return false;
  }

  // 2. 初始化路径构建器
  string_t path_builder;
  if (!string_init(&path_builder, alc, 256))
  { // 256B 路径缓冲区
    vec_destroy(&entries);
    return false;
  }
  string_append_cstr(&path_builder, src_dir);
  // (TODO: 处理 src_dir 结尾的 '/' )

  // 3. 递归遍历和解析
  printf("  Scanning `%s`...\n", src_dir);
  traverse_directory(alc, &entries, &path_builder);
  printf("  Found %zu documentation entries.\n", vec_count(&entries));

  // 4. 生成 Markdown
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

  // 5. 清理 (fluf 哲学：栈对象需要 destroy)
  string_destroy(&path_builder);
  vec_destroy(&entries);

  // 内存由 main() 中的 arena 自动释放
  return true;
}