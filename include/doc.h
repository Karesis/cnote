// include/doc.h
#pragma once

#include <core/mem/allocer.h>
#include <stdbool.h>

/**
 * @brief 运行文档生成器
 *
 * 扫描 `src_dir` (递归)，为 .c 和 .h 文件生成文档，
 * 并将所有内容合并到一个 .md 文件中，写入 `out_path`。
 *
 * @param alc      用于所有操作的 Arena 分配器
 * @param src_dir  要扫描的源目录
 * @param out_path 最终输出的 Markdown 文件路径 (例如 "docs/api.md")
 * @return true 成功, false 失败
 */
bool cnote_doc_run(allocer_t *alc, const char *src_dir, const char *out_path);