
#pragma once

#include <core/mem/allocer.h>
#include <stdbool.h>

/**
 * @brief 从文件中移除 '//' 风格的注释并用 clang-format 格式化。
 *
 * @param alc      用于所有临时分配的分配器 (builder, read buffer)
 * @param filename 要处理的文件名 (原地修改)
 * @return bool     true 成功, false 失败 (例如文件无法读取)
 */
bool cnote_clean_file(allocer_t *alc, const char *filename);