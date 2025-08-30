# Makefile for minimal mtools-like utilities (mformat, mdir, minfo, mcp, mdel)

# ---- Toolchain ----
CC       ?= gcc
CFLAGS   ?= -Wall -Wextra -O2
CPPFLAGS ?=
LDFLAGS  ?=
LDLIBS   ?=

# ---- Install location ----
BINDIR   ?= /cygdrive/c/cygwin64/bin
DESTDIR  ?=

# ---- Executable suffix (Windows/Cygwin/MinGW => .exe) ----
UNAME_S  := $(shell uname -s 2>/dev/null)
EXEEXT   := $(if $(findstring CYGWIN,$(UNAME_S)),.exe, \
            $(if $(findstring MINGW,$(UNAME_S)),.exe, \
            $(if $(findstring MSYS,$(UNAME_S)),.exe,)))

# ---- Layout ----
SRC_DIR   := src
BUILD_DIR := build

# ---- Programs & sources ----
PROGS     := mformat mdir minfo mcp mdel
SRCS      := $(addprefix $(SRC_DIR)/,$(addsuffix .c,$(PROGS)))
BINARIES  := $(addprefix $(BUILD_DIR)/,$(addsuffix $(EXEEXT),$(PROGS)))

# ---- Default target ----
.PHONY: all
all: $(BUILD_DIR) $(BINARIES)

# Ensure build directory exists
$(BUILD_DIR):
	mkdir -p "$(BUILD_DIR)"

# ---- Pattern rule: src/<name>.c -> build/<name>.exe ----
$(BUILD_DIR)/%$(EXEEXT): $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)

# ---- Convenience targets (e.g., `make mdir`) ----
.PHONY: $(PROGS)
$(PROGS): %: $(BUILD_DIR)/%$(EXEEXT)
	@echo "Built $<"

# ---- Install / uninstall ----
.PHONY: install uninstall
install: $(BINARIES)
	@echo "Installing to $(DESTDIR)$(BINDIR)"
	mkdir -p "$(DESTDIR)$(BINDIR)"
	cp -f $(BINARIES) "$(DESTDIR)$(BINDIR)"

uninstall:
	@echo "Uninstalling from $(DESTDIR)$(BINDIR)"
	rm -f $(addprefix $(DESTDIR)$(BINDIR)/,$(notdir $(BINARIES)))

# ---- Clean ----
.PHONY: clean
clean:
	rm -rf "$(BUILD_DIR)"
	# remove any test artifacts you may create
	-rm -f floppy.img

# ---
