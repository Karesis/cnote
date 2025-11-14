# doc.h

## `bool cnote_doc_run(allocer_t *alc, const char *src_dir, const char *out_dir);`


运行文档生成器 (mdBook 模式)

扫描 `src_dir` (递归)，为 .c 和 .h 文件生成文档，
并将所有内容按文件结构输出到 `out_dir` (例如 'docs/')。


- **`alc`**: 用于所有操作的 Arena 分配器
- **`src_dir`**: 要扫描的源目录
- **`out_dir`**: 要写入 Markdown 文件的输出目录
- **Returns**: true 成功, false 失败


---

