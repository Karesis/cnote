// src/main.c

// --- fluf 核心 ---
#include <core/mem/allocer.h>
#include <std/allocer/bump/bump.h>
#include <std/allocer/bump/glue.h>
#include <core/msg/asrt.h>

// --- fluf 模块 ---
#include <std/env/args.h>
#include <std/string/str_slice.h>
#include <std/vec.h>

// --- Cnote 模块 ---
#include <clean.h>
#include <doc.h>
#include <license.h>

// --- C 标准库 ---
#include <stdio.h>
#include <string.h>

/**
 * @brief 打印帮助信息
 */
static void print_usage(void)
{
  fprintf(stderr, "Usage: cnote <command> [options] [targets...]\n");
  fprintf(stderr, "\nCommands:\n");
  fprintf(stderr, "  clean [opts] <paths...>    Removes '//' comments and runs clang-format.\n");
  // [修改]
  fprintf(stderr, "  doc <src_dir> <out_dir>     Generates markdown documentation (mdBook compatible).\n");
  fprintf(stderr, "  license [opts] <paths...>   Applies or maintains a license header.\n");

  fprintf(stderr, "\nGeneral Options:\n");
  fprintf(stderr, "  -h, --help                 Show this help message.\n");

  fprintf(stderr, "\n'clean' Options:\n");
  fprintf(stderr, "  -e, --exclude <path>       Exclude a file/directory.\n");
  fprintf(stderr, "  -s, --style <file>         Path to .clang-format file to use.\n");

  fprintf(stderr, "\n'license' Options:\n");
  fprintf(stderr, "  -e, --exclude <path>       Exclude a file/directory.\n");
  fprintf(stderr, "  -f, --file <license_file>  (Required) Path to the license text file.\n");
}

/**
 * @brief 'clean' 命令的实现
 */
static bool cmd_clean(allocer_t *alc, args_parser_t *p)
{
  vec_t targets;
  vec_t exclusions;
  const char *style_file = NULL;

  if (!vec_init(&targets, alc, 0) || !vec_init(&exclusions, alc, 0))
    return false;

  str_slice_t arg, value;
  arg_type_t type;

  while ((type = args_parser_peek(p, &arg)) != ARG_TYPE_END)
  {
    if (type == ARG_TYPE_FLAG)
    {
      args_parser_consume(p, &arg);
      if (slice_equals_cstr(arg, "-e") || slice_equals_cstr(arg, "--exclude"))
      {
        if (!args_parser_consume_value(p, arg.ptr, &value))
          return false;
        if (!vec_push(&exclusions, (void *)value.ptr))
          return false;
      }
      else if (slice_equals_cstr(arg, "-s") || slice_equals_cstr(arg, "--style"))
      {
        if (!args_parser_consume_value(p, arg.ptr, &value))
          return false;
        style_file = value.ptr;
      }
      else
      {
        fprintf(stderr, "Error: Unknown flag '%.*s' for 'clean' command\n", (int)arg.len, arg.ptr);
        return false;
      }
    }
    else if (type == ARG_TYPE_POSITIONAL)
    {
      args_parser_consume(p, &arg);
      if (!vec_push(&targets, (void *)arg.ptr))
        return false;
    }
  }

  if (vec_count(&targets) == 0)
  {
    fprintf(stderr, "Error: 'clean' command requires at least one target path.\n");
    return false;
  }

  bool ok = cnote_clean_run(alc, &targets, &exclusions, style_file);
  vec_destroy(&exclusions);
  vec_destroy(&targets);
  return ok;
}

/**
 * @brief 'doc' 命令的实现
 */
static bool cmd_doc(allocer_t *alc, args_parser_t *p)
{
  // [修改]
  str_slice_t src_dir_slice, out_dir_slice;

  arg_type_t type = args_parser_consume(p, &src_dir_slice);
  if (type != ARG_TYPE_POSITIONAL)
  {
    fprintf(stderr, "Error: 'doc' command expected <src_dir> argument.\n");
    return false;
  }
  // [修改]
  type = args_parser_consume(p, &out_dir_slice);
  if (type != ARG_TYPE_POSITIONAL)
  {
    fprintf(stderr, "Error: 'doc' command expected <out_dir> argument.\n");
    return false;
  }
  str_slice_t dummy;
  if (args_parser_peek(p, &dummy) == ARG_TYPE_POSITIONAL)
  {
    fprintf(stderr, "Error: 'doc' command got too many arguments. Expected only 2.\n");
    return false;
  }
  // [修改]
  return cnote_doc_run(alc, src_dir_slice.ptr, out_dir_slice.ptr);
}

/**
 * @brief 'license' 命令的实现
 */
static bool cmd_license(allocer_t *alc, args_parser_t *p)
{
  vec_t targets;
  vec_t exclusions;
  const char *license_file = NULL;

  if (!vec_init(&targets, alc, 0) || !vec_init(&exclusions, alc, 0))
    return false;

  str_slice_t arg, value;
  arg_type_t type;

  while ((type = args_parser_peek(p, &arg)) != ARG_TYPE_END)
  {
    if (type == ARG_TYPE_FLAG)
    {
      args_parser_consume(p, &arg);
      if (slice_equals_cstr(arg, "-e") || slice_equals_cstr(arg, "--exclude"))
      {
        if (!args_parser_consume_value(p, arg.ptr, &value))
          return false;
        if (!vec_push(&exclusions, (void *)value.ptr))
          return false;
      }
      else if (slice_equals_cstr(arg, "-f") || slice_equals_cstr(arg, "--file"))
      {
        if (!args_parser_consume_value(p, arg.ptr, &value))
          return false;
        license_file = value.ptr;
      }
      else
      {
        fprintf(stderr, "Error: Unknown flag '%.*s' for 'license' command\n", (int)arg.len, arg.ptr);
        return false;
      }
    }
    else if (type == ARG_TYPE_POSITIONAL)
    {
      args_parser_consume(p, &arg);
      if (!vec_push(&targets, (void *)arg.ptr))
        return false;
    }
  }

  if (license_file == NULL)
  {
    fprintf(stderr, "Error: 'license' command requires a --file <license_file> argument.\n");
    return false;
  }
  if (vec_count(&targets) == 0)
  {
    fprintf(stderr, "Error: 'license' command requires at least one target path.\n");
    return false;
  }

  bool ok = cnote_license_run(alc, &targets, &exclusions, license_file);
  vec_destroy(&exclusions);
  vec_destroy(&targets);
  return ok;
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
    printf("--- cnote: Generating Docs ---\n");
    success = cmd_doc(&alc, &p);
    printf("------------------------------\n");
  }
  else if (slice_equals_cstr(arg, "license"))
  {
    printf("--- cnote: Applying License ---\n");
    success = cmd_license(&alc, &p);
    printf("-------------------------------\n");
  }
  else
  {
    fprintf(stderr, "Error: Unknown command '%.*s'\n", (int)arg.len, arg.ptr);
    print_usage();
  }

  bump_destroy(&arena);
  return success ? 0 : 1;
}