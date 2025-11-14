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
 * @brief 运行 'license' 命令
 *
 * 遍历 'targets' (文件或目录)，应用或维护 'license_file'
 * 中的许可证头, 同时跳过 'exclusions'。
 *
 * @param alc      用于所有临时分配的 Arena
 * @param targets  (vec_t*) 指向 Vec<const char*> 的指针, 包含要处理的文件/目录
 * @param exclusions (vec_t*) 指向 Vec<const char*> 的指针, 包含要跳过的路径
 * @param license_file (必需) 指向包含许可证原文的文本文件路径
 * @return bool     true 成功, false 失败
 */
bool cnote_license_run(allocer_t *alc, vec_t *targets, vec_t *exclusions,
                       const char *license_file);