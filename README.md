# cnote

cnote is a useful tool that can help you manage your C codes.

## Features

* **`clean`**: Recursively removes `//` comments and applies `clang-format` to C source files.
* **`doc`**: Generates simple Markdown API documentation from `/** @brief ... */` style comments in source files.
* **`license`**: Applies or updates a license header to all source files, skipping files that already have it.

## Requirements

### Build-Time Dependencies

To build `cnote` from source, you will need:
* `clang` (fluf depends on some features that only clang support)
* `make`
* `git` (to clone the repository and its submodules)

### Run-Time Dependencies

To use `cnote`, you must have the following tools installed and available in your system's `PATH`:
* **`clang-format`**: Required by the `clean` command.
> NOTE: most of the times, if you installed `clang`, you will get clang-format on your system

## Building from Source

`cnote` uses a `git submodule` for its core library (`fluf`).

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/Karesis/cnote.git
    cd cnote
    ```

2.  **Initialize and update the submodule:**
    ```bash
    git submodule update --init --recursive
    ```

3.  **Build the project:**
    ```bash
    make
    ```

4.  **Install the binary (optional):**
    This will install `cnote` to `/usr/local/bin` (or a custom `PREFIX`).
    ```bash
    sudo make install
    ```

    To uninstall:
    ```bash
    sudo make uninstall
    ```

    To update:
    ```bash
    # update files
    make update
    
    # then install
    sudo make install
    ```

## Usage

`cnote` is a command-line tool. You can get help at any time:

```bash
$ cnote --help
Usage: cnote <command> [options] [targets...]

Commands:
  clean [opts] <paths...>    Removes '//' comments and runs clang-format.
  doc <src_dir> <out_dir>     Generates markdown documentation (mdBook compatible).
  license [opts] <paths...>   Applies or maintains a license header.

General Options:
  -h, --help                 Show this help message.

'clean' Options:
  -e, --exclude <path>       Exclude a file/directory.
  -s, --style <file>         Path to .clang-format file to use.

'license' Options:
  -e, --exclude <path>       Exclude a file/directory.
  -f, --file <license_file>  (Required) Path to the license text file.
````

### Examples

#### `clean`

Clean all `.c` and `.h` files in the `src` and `include` directories, excluding the `vendor` directory:

```bash
cnote clean -e vendor/ src/ include/
```

Use a specific `.clang-format` file:

```bash
cnote clean -s ./.clang-format src/
```

#### `doc`

Generate documentation from all sources in `include/` and write to `docs/api.md`:

```bash
cnote doc include/ docs/api.md
```

#### `license`

Apply the license header from a file (e.g., `MY_LICENSE`) to all files in `src/` and `include/`:

```bash
cnote license -f MY_LICENSE src/ include/
```

## API Documentation

For information on the internal C API (e.g., for contributing to `cnote`), see the generated [API Documentation](docs/api.md).

## License

This project is licensed under the Apache-2.0 License. See the [LICENSE](LICENSE) file for details.

