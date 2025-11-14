CC = clang
CFLAGS = -g -Wall -Wextra -Wno-unused-parameter -std=c23 -DDEBUG -Iinclude

FLUF_DIR = vendor/fluf
FLUF_INC = -I$(FLUF_DIR)/include
FLUF_LIB_FILE = $(FLUF_DIR)/lib/libfluf.a
FLUF_LNK = -L$(FLUF_DIR)/lib -lfluf

TARGET_DIR = bin
TARGET = $(TARGET_DIR)/cnote

SRCS = $(wildcard src/*.c)
OBJS = $(patsubst src/%.c,obj/%.o,$(SRCS))

# === 目标 ===

.PHONY: all clean test fluf

all: $(TARGET)

$(TARGET): $(OBJS) $(FLUF_LIB_FILE)
	@mkdir -p $(TARGET_DIR)
	@printf "  LD   $@\n"
	@$(CC) $(OBJS) $(LDFLAGS) $(FLUF_LNK) -o $@

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