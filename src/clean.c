// src/clean.c

#include <clean.h> // 对应的头文件

// --- fluf 模块 ---
#include <std/string/string.h>    // string_t, string_init, string_push, ...
#include <std/string/str_slice.h> // str_slice_t
#include <std/io/file.h>          // read_file_to_slice, write_file_bytes
#include <core/msg/asrt.h>        // asrt_msg (fluf 的断言)

// --- C 标准库 ---
#include <stdio.h>
#include <stdlib.h> // for system()

// 状态机
typedef enum
{
  STATE_CODE,          // 默认代码
  STATE_LINE_COMMENT,  // 在 // 之后 (我们要删除这个)
  STATE_BLOCK_COMMENT, // 在 /* ... */ 内部 (我们要保留这个)
  STATE_STRING,        // 在 "..." 内部
  STATE_CHAR,          // 在 '...' 内部
} clean_state_t;

bool cnote_clean_file(allocer_t *alc, const char *filename)
{

  // 1. (fluf) 使用 Arena 读取文件
  str_slice_t content;
  if (!read_file_to_slice(alc, filename, &content))
  {
    fprintf(stderr, "Error: Failed to read file '%s'.\n", filename);
    return false;
  }

  // 2. (fluf) 创建一个字符串构建器
  string_t builder;
  string_init(&builder, alc, content.len);

  clean_state_t state = STATE_CODE;
  const char *p = content.ptr;
  const char *end = content.ptr + content.len;

  // 3. 核心：状态机遍历
  while (p < end)
  {
    char c = *p;
    char next = (p + 1 < end) ? *(p + 1) : '\0';

    switch (state)
    {

    case STATE_CODE:
      if (c == '/' && next == '/')
      {
        state = STATE_LINE_COMMENT;
        p++;
      }
      else if (c == '/' && next == '*')
      {
        state = STATE_BLOCK_COMMENT;
        string_push(&builder, c);
        string_push(&builder, next);
        p++;
      }
      else if (c == '"')
      {
        state = STATE_STRING;
        string_push(&builder, c);
      }
      else if (c == '\'')
      {
        state = STATE_CHAR;
        string_push(&builder, c);
      }
      else
      {
        string_push(&builder, c);
      }
      break;

    case STATE_LINE_COMMENT:
      if (c == '\n')
      {
        state = STATE_CODE;
        string_push(&builder, c); // 保留换行符
      }
      break;

    case STATE_BLOCK_COMMENT:
      string_push(&builder, c);
      if (c == '*' && next == '/')
      {
        string_push(&builder, next);
        state = STATE_CODE;
        p++;
      }
      break;

    case STATE_STRING:
      string_push(&builder, c);
      if (c == '\\')
      {
        if (next != '\0')
        {
          string_push(&builder, next);
          p++;
        }
      }
      else if (c == '"')
      {
        state = STATE_CODE;
      }
      break;

    case STATE_CHAR:
      string_push(&builder, c);
      if (c == '\\')
      {
        if (next != '\0')
        {
          string_push(&builder, next);
          p++;
        }
      }
      else if (c == '\'')
      {
        state = STATE_CODE;
      }
      break;
    }

    p++; // 前进到下一个字符
  }

  // 4. (fluf) 写回文件 (原地修改)

  // (*** 已修正 ***)
  // 使用 string_as_slice() 获取 ptr 和 len
  str_slice_t result_slice = string_as_slice(&builder);

  if (!write_file_bytes(filename, (const void *)result_slice.ptr, result_slice.len))
  {
    fprintf(stderr, "Error: Failed to write file '%s'.\n", filename);
    string_destroy(&builder);
    return false;
  }

  string_destroy(&builder);

  // --- 5. 运行 clang-format ---

  string_t cmd;
  string_init(&cmd, alc, 64);

  // (*** 已修正 ***)
  // fluf 没有 push_fmt, 我们分两步 append
  string_append_cstr(&cmd, "clang-format -i ");
  string_append_cstr(&cmd, filename); // filename 是 \0 结尾的 cstr

  // (*** 已修正 ***)
  // 使用 string_as_cstr()
  printf("  Running: %s\n", string_as_cstr(&cmd));

  // (*** 已修正 ***)
  // 使用 string_as_cstr()
  int ret = system(string_as_cstr(&cmd));

  if (ret != 0)
  {
    fprintf(stderr, "Warning: clang-format command failed (is it installed?)\n");
  }

  string_destroy(&cmd);

  return true;
}