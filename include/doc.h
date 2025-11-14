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


#pragma once

#include <core/mem/allocer.h>
#include <stdbool.h>

/**
 * @brief 运行文档生成器 (mdBook 模式)
 *
 * 扫描 `src_dir` (递归)，为 .c 和 .h 文件生成文档，
 * 并将所有内容按文件结构输出到 `out_dir` (例如 'docs/')。
 *
 * @param alc      用于所有操作的 Arena 分配器
 * @param src_dir  要扫描的源目录
 * @param out_dir  要写入 Markdown 文件的输出目录
 * @return true 成功, false 失败
 */
bool cnote_doc_run(allocer_t *alc, const char *src_dir, const char *out_dir);