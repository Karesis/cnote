# license.h

## `bool cnote_license_run(allocer_t *alc, vec_t *targets, vec_t *exclusions, const char *license_file);`


运行 'license' 命令

遍历 'targets' (文件或目录)，应用或维护 'license_file'
中的许可证头, 同时跳过 'exclusions'。


- **`alc`**: 用于所有临时分配的 Arena
- **`targets`**: (vec_t*) 指向 Vec<const char*> 的指针, 包含要处理的文件/目录
- **`exclusions`**: (vec_t*) 指向 Vec<const char*> 的指针, 包含要跳过的路径
- **`license_file`**: (必需) 指向包含许可证原文的文本文件路径
- **Returns**: bool     true 成功, false 失败


---

