# === 编译器和标志 ===
CC = clang
# 调试构建，启用所有 fluf 断言
# -Iinclude/ 让我们可以 #include <clean.h>
CFLAGS = -g -Wall -Wextra -Wno-unused-parameter -std=c23 -DDEBUG -Iinclude

# === 目录 ===
# fluf 库 (在 vendor/ 目录下)
FLUF_DIR = vendor/fluf
FLUF_INC = -I$(FLUF_DIR)/include
# fluf 静态库 (fluf 的 Makefile 会把它生成在 vendor/fluf/lib/libfluf.a)
FLUF_LIB_FILE = $(FLUF_DIR)/lib/libfluf.a
# 最终链接 fluf 库
FLUF_LNK = -L$(FLUF_DIR)/lib -lfluf

# 最终目标程序
TARGET_DIR = bin
TARGET = $(TARGET_DIR)/cnote

# === 源文件和对象 ===
# 自动查找所有 src/*.c 文件 (src/main.c, src/clean.c)
SRCS = $(wildcard src/*.c)
# 将 src/main.c 转换为 obj/main.o
OBJS = $(patsubst src/%.c,obj/%.o,$(SRCS))

# === 目标 ===

.PHONY: all clean test fluf

all: $(TARGET)

# (3) 最终链接目标程序
$(TARGET): $(OBJS) $(FLUF_LIB_FILE)
	@mkdir -p $(TARGET_DIR)
	@printf "  LD   $@\n"
	@$(CC) $(OBJS) $(LDFLAGS) $(FLUF_LNK) -o $@

# (2) 编译 cnote 的 .c 文件
obj/%.o: src/%.c
	@mkdir -p obj
	@printf "  CC   $@\n"
	@$(CC) $(CFLAGS) $(FLUF_INC) -c $< -o $@

# (1) 依赖：确保 fluf 被构建
#     (这会调用 vendor/fluf/Makefile 的 'all' 目标)
$(FLUF_LIB_FILE): fluf
fluf:
	@printf "  MAKE libfluf.a\n"
	@$(MAKE) -C $(FLUF_DIR)

# === 清理 ===
clean:
	@printf "  CLEAN\n"
	@rm -rf $(TARGET_DIR) obj
	@$(MAKE) -C $(FLUF_DIR) clean

# === 测试 (未来) ===
test:
	@echo "No tests yet"