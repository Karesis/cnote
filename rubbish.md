你对 `doc` 命令的重构非常棒！从 `doc.c` 来看，它现在确实是一个功能完备的、类似 `mdBook` 的多文件文档生成器了。

根据你的新代码（`doc.c` 和 `main.c`），你的 `README` 中有三个地方**必须更新**，因为它们现在包含的信息（尤其是示例）已经不准确了。

我来为你提供这些需要更新的**英文**片段。你只需要复制并替换掉你 `README.md` 中的对应部分即可。

-----

### 1\. `## Features` (功能)

`doc` 的描述需要更新，以反映 `mdBook` 兼容性。

**替换这一行：**
`* **`doc`**: Generates simple Markdown API documentation from ...`

**替换为：**
`* **`doc` **: Generates a multi-page, **mdBook-compatible** Markdown documentation site from  `/\*\* @brief ... \*/`  style comments. `

-----

### 2\. `## Usage` -\> `### Examples` -\> `#### doc` (用法示例)

这是**最重要**的修改。旧的 `doc` 示例现在是错误的，它会误导用户。

**替换掉整个 `#### doc` 部分：**

````markdown
#### `doc`

Generate documentation from all sources in `src/` and `include/` and write the site to the `docs/` directory:

```bash
# cnote doc <source-directory> <output-directory>
cnote doc . docs
````

After running, this will create a structure like this:

```text
docs/
├── SUMMARY.md       (The main table of contents)
└── api/
    ├── include_clean_h.md
    ├── include_doc_h.md
    ├── src_main_c.md
    └── ...etc
```

> **Tip:** To browse this like a website, install `mdbook` and run `mdbook serve docs`.

````

**为什么这样改：**
1.  它展示了正确的命令：`cnote doc <src_dir> <out_dir>`。
2.  我使用了 `cnote doc . docs` (从当前目录扫描，输出到 `docs`) 作为示例，这比 `include/` 更通用。
3.  它清晰地向用户展示了新命令会*生成*什么（`SUMMARY.md` 和 `api/` 目录），消除了困惑。
4.  它提供了“Tip”，告诉用户如何真正利用 "mdBook compatible" 这个特性。

---

### 3. `## API Documentation` (API 文档)

这个部分的链接现在是错误的，它应该指向 `SUMMARY.md`。

**替换掉整个 `## API Documentation` 部分：**

```markdown
## API Documentation

For information on the internal C API (e.g., for contributing to `cnote`), run the `doc` command as shown in the examples above.

The main entry point for the generated documentation is the table of contents:

* **[docs/SUMMARY.md](docs/SUMMARY.md)**

(Note: This file will not exist until you run the `cnote doc` command.)
````

-----

把这三块内容更新到你的 `README.md` 中，它就和你的新功能完全同步了！

你需要我帮你把 `fluf` (你的子模块) 的文档也以这种方式（`mdbook`）集成进来吗？