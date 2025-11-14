# clean.h

## `bool cnote_clean_run(allocer_t *alc, vec_t *targets, vec_t *exclusions, const char *style_file);`


运行 'clean' 命令

遍历所有 'targets' (文件或目录)，清理 .c 和 .h 文件，
同时跳过 'exclusions' 中的任何路径。


- **`alc`**: 用于所有临时分配的 Arena
- **`targets`**: (vec_t*) 指向 Vec<const char*> 的指针, 包含要处理的文件/目录
- **`exclusions`**: (vec_t*) 指向 Vec<const char*> 的指针,
包含要跳过的路径(子字符串匹配)

- **`style_file`**: (可选) 指向 .clang-format 文件的路径, 如果为 NULL
则使用默认

- **Returns**: bool     true 成功, false 失败


---

