# ─────────────────────────────────────────────────────────────
#  Nolqu Language Runtime — Makefile
#  Builds the standalone `nq` binary from C source
# ─────────────────────────────────────────────────────────────

CC      = gcc
TARGET  = nq
SRCDIR  = src
OBJDIR  = build

# ─── Compiler flags ───────────────────────────────────────────
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic \
          -Wno-unused-parameter \
          -I$(SRCDIR)
LDFLAGS = -lm

# Release build (default)
CFLAGS_RELEASE = $(CFLAGS) -O2 -DNDEBUG

# Debug build (make debug)
CFLAGS_DEBUG = $(CFLAGS) -g3 -O0 -DNQ_DEBUG_TRACE -DDEBUG \
               -fsanitize=address,undefined

# ─── Source files ─────────────────────────────────────────────
SOURCES = \
    $(SRCDIR)/main.c      \
    $(SRCDIR)/memory.c    \
    $(SRCDIR)/value.c     \
    $(SRCDIR)/chunk.c     \
    $(SRCDIR)/object.c    \
    $(SRCDIR)/table.c     \
    $(SRCDIR)/lexer.c     \
    $(SRCDIR)/ast.c       \
    $(SRCDIR)/parser.c    \
    $(SRCDIR)/compiler.c  \
    $(SRCDIR)/vm.c        \
    $(SRCDIR)/repl.c

OBJECTS_RELEASE = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/release/%.o, $(SOURCES))
OBJECTS_DEBUG   = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/debug/%.o,   $(SOURCES))

# ─── Default target: release ──────────────────────────────────
.PHONY: all release debug clean install test

all: release

release: $(OBJDIR)/release $(TARGET)

$(TARGET): $(OBJECTS_RELEASE)
	@echo "  LD    $(TARGET)"
	@$(CC) $(CFLAGS_RELEASE) -o $@ $^ $(LDFLAGS)
	@echo ""
	@echo "  ✓ Build berhasil: ./$(TARGET)"
	@echo "    Gunakan: ./$(TARGET) program.nq"
	@echo "    REPL:    ./$(TARGET) repl"

$(OBJDIR)/release/%.o: $(SRCDIR)/%.c
	@echo "  CC    $<"
	@$(CC) $(CFLAGS_RELEASE) -c -o $@ $<

# ─── Debug build ──────────────────────────────────────────────
debug: $(OBJDIR)/debug $(TARGET)-debug

$(TARGET)-debug: $(OBJECTS_DEBUG)
	@echo "  LD    $(TARGET)-debug"
	@$(CC) $(CFLAGS_DEBUG) -o $@ $^ $(LDFLAGS)
	@echo "  ✓ Debug build: ./$(TARGET)-debug"

$(OBJDIR)/debug/%.o: $(SRCDIR)/%.c
	@echo "  CC    $< [debug]"
	@$(CC) $(CFLAGS_DEBUG) -c -o $@ $<

# ─── Directories ──────────────────────────────────────────────
$(OBJDIR)/release:
	@mkdir -p $@

$(OBJDIR)/debug:
	@mkdir -p $@

# ─── Install ──────────────────────────────────────────────────
INSTALL_DIR = /usr/local/bin

install: release
	@echo "  Installing nq to $(INSTALL_DIR)..."
	@cp $(TARGET) $(INSTALL_DIR)/$(TARGET)
	@chmod 755 $(INSTALL_DIR)/$(TARGET)
	@echo "  ✓ Installed: $(INSTALL_DIR)/$(TARGET)"

uninstall:
	@rm -f $(INSTALL_DIR)/$(TARGET)
	@echo "  ✓ Uninstalled nq"

# ─── Test ─────────────────────────────────────────────────────
test: release
	@echo "── Menjalankan program contoh ──"
	@./$(TARGET) examples/hello.nq
	@echo ""
	@./$(TARGET) examples/fibonacci.nq
	@echo ""
	@./$(TARGET) examples/functions.nq

# ─── Clean ────────────────────────────────────────────────────
clean:
	@rm -rf $(OBJDIR) $(TARGET) $(TARGET)-debug
	@echo "  ✓ Clean"
