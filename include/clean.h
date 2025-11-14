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
#include <std/vec.h>
#include <stdbool.h>

/**
 * @brief 运行 'clean' 命令
 *
 * 遍历所有 'targets' (文件或目录)，清理 .c 和 .h 文件，
 * 同时跳过 'exclusions' 中的任何路径。
 *
 * @param alc      用于所有临时分配的 Arena
 * @param targets  (vec_t*) 指向 Vec<const char*> 的指针, 包含要处理的文件/目录
 * @param exclusions (vec_t*) 指向 Vec<const char*> 的指针,
 * 包含要跳过的路径(子字符串匹配)
 * @param style_file (可选) 指向 .clang-format 文件的路径, 如果为 NULL
 * 则使用默认
 * @return bool     true 成功, false 失败
 */
bool cnote_clean_run(allocer_t *alc, vec_t *targets, vec_t *exclusions,
                     const char *style_file);