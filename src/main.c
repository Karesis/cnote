// src/main.c

// --- fluf 核心 ---
#include <core/mem/allocer.h>
#include <std/allocer/bump/bump.h>
#include <std/allocer/bump/glue.h>
#include <core/msg/asrt.h>

// --- fluf 模块 ---
#include <std/env/args.h>
#include <std/string/str_slice.h>

// --- Cnote 模块 ---
#include <clean.h> // <-- 我们的 clean 模块
#include <doc.h>   // <-- [新] 我们的 doc 模块

// --- C 标准库 ---
#include <stdio.h>
#include <string.h>

/**
 * @brief 打印帮助信息
 */
static void print_usage(void)
{
  fprintf(stderr, "Usage: cnote <command> [options] [files...]\n");
  fprintf(stderr, "\nCommands:\n");
  fprintf(stderr, "  clean <file...>    Removes '//' comments and runs clang-format.\n");
  fprintf(stderr, "  doc <src_dir> <out_path>  Generates markdown documentation.\n"); // [修改]
  fprintf(stderr, "\nOptions:\n");
  fprintf(stderr, "  -h, --help         Show this help message.\n");
}

/**
 * @brief 'clean' 命令的实现
 */
static bool cmd_clean(allocer_t *alc, args_parser_t *p)
{
  str_slice_t file_slice;
  int files_processed = 0;
  bool all_ok = true;

  while (args_parser_peek(p, &file_slice) == ARG_TYPE_POSITIONAL)
  {
    args_parser_consume(p, &file_slice);
    const char *filename = file_slice.ptr;

    printf("Processing: %s\n", filename);
    if (!cnote_clean_file(alc, filename))
    {
      fprintf(stderr, "Error cleaning file: %s\n", filename);
      all_ok = false;
    }
    files_processed++;
  }

  if (files_processed == 0)
  {
    fprintf(stderr, "Error: 'clean' command requires at least one file.\n");
    return false;
  }

  return all_ok;
}

/**
 * @brief [新] 'doc' 命令的实现
 */
static bool cmd_doc(allocer_t *alc, args_parser_t *p)
{
  str_slice_t src_dir_slice, out_path_slice;

  // 1. 消耗第一个参数 <src_dir>
  // (fluf/args.h 里的 args_parser_consume_value 似乎更合适，
  // 但我们用 consume + peek 来检查)
  arg_type_t type = args_parser_consume(p, &src_dir_slice);
  if (type != ARG_TYPE_POSITIONAL)
  {
    fprintf(stderr, "Error: 'doc' command expected <src_dir> argument.\n");
    return false;
  }

  // 2. 消耗第二个参数 <out_path>
  type = args_parser_consume(p, &out_path_slice);
  if (type != ARG_TYPE_POSITIONAL)
  {
    fprintf(stderr, "Error: 'doc' command expected <out_path> argument.\n");
    return false;
  }

  // 3. 检查是否还有多余的参数
  str_slice_t dummy;
  if (args_parser_peek(p, &dummy) == ARG_TYPE_POSITIONAL)
  {
    fprintf(stderr, "Error: 'doc' command got too many arguments. Expected only 2.\n");
    return false;
  }

  // src_dir.ptr 和 out_path.ptr 是 \0 结尾的 C 字符串
  return cnote_doc_run(alc, src_dir_slice.ptr, out_path_slice.ptr);
}

int main(int argc, const char **argv)
{
  bump_t arena;
  bump_init(&arena);
  allocer_t alc = bump_to_allocer(&arena);

  args_parser_t p;
  args_parser_init(&p, argc, argv);

  str_slice_t arg;
  arg_type_t type = args_parser_consume(&p, &arg);

  if (type == ARG_TYPE_END)
  {
    fprintf(stderr, "Error: No command provided.\n");
    print_usage();
    bump_destroy(&arena);
    return 1;
  }

  if (type == ARG_TYPE_FLAG)
  {
    if (slice_equals_cstr(arg, "-h") || slice_equals_cstr(arg, "--help"))
    {
      print_usage();
      bump_destroy(&arena);
      return 0;
    }
  }

  if (type != ARG_TYPE_POSITIONAL)
  {
    fprintf(stderr, "Error: Expected command (e.g., 'clean') but got flag.\n");
    print_usage();
    bump_destroy(&arena);
    return 1;
  }

  bool success = false;
  if (slice_equals_cstr(arg, "clean"))
  {
    printf("--- cnote: Cleaning ---\n");
    success = cmd_clean(&alc, &p);
    printf("-------------------------\n");
  }
  else if (slice_equals_cstr(arg, "doc"))
  {
    // --- [修改] ---
    printf("--- cnote: Generating Docs ---\n");
    success = cmd_doc(&alc, &p); // 调用新函数
    printf("------------------------------\n");
  }
  else
  {
    fprintf(stderr, "Error: Unknown command '%.*s'\n", (int)arg.len, arg.ptr);
    print_usage();
  }

  bump_destroy(&arena);

  return success ? 0 : 1;
}