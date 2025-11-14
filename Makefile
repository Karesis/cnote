CC = clang
CFLAGS = -g -Wall -Wextra -Wno-unused-parameter -std=c23 -DDEBUG -Iinclude
LDFLAGS = 
LDLIBS = 

FLUF_DIR = vendor/fluf
FLUF_INC = -I$(FLUF_DIR)/include
FLUF_LIB_FILE = $(FLUF_DIR)/lib/libfluf.a

# 将 fluf 的链接信息添加到 LDFLAGS 和 LDLIBS
LDFLAGS += -L$(FLUF_DIR)/lib
LDLIBS += -lfluf

TARGET_DIR = bin
TARGET = $(TARGET_DIR)/cnote

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%.c,obj/%.o,$(SRCS))

# === 安装路径 ===
# PREFIX 默认使用 /usr/local
# ?= 允许从外部覆盖, 例如: make PREFIX=~/.local install
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin


# === 目标 ===
.PHONY: all clean test fluf install uninstall update

all: $(TARGET)

$(TARGET): $(OBJS) $(FLUF_LIB_FILE)
	@mkdir -p $(TARGET_DIR)
	@printf "  LD   $@\n"
	@$(CC) $(OBJS) $(LDFLAGS) $(LDLIBS) -o $@

obj/%.o: src/%.c
	@mkdir -p obj
	@printf "  CC   $@\n"
	@$(CC) $(CFLAGS) $(FLUF_INC) -c $< -o $@

$(FLUF_LIB_FILE): fluf
fluf:
	@printf "  MAKE libfluf.a\n"
	@$(MAKE) -C $(FLUF_DIR)

clean:
	@printf "  CLEAN\n"
	@rm -rf $(TARGET_DIR) obj
	@$(MAKE) -C $(FLUF_DIR) clean

test:
	@echo "No tests yet"

# === 安装与卸载 ===

install: all
	@printf "  INSTALL $(BINDIR)/cnote\n"
	@mkdir -p $(BINDIR)
	@cp $(TARGET) $(BINDIR)/cnote
	@chmod +x $(BINDIR)/cnote

uninstall:
	@printf "  UNINSTALL $(BINDIR)/cnote\n"
	@rm -f $(BINDIR)/cnote

# === 更新 ===

update:
	@printf "  GIT  Pulling latest changes...\n"
	@git pull
	@printf "  GIT  Updating submodules...\n"
	@git submodule update --init --recursive
	@printf "  MAKE Rebuilding project...\n"
	@$(MAKE) all
	@printf "==> Update complete. Run 'sudo make install' to install.\n"