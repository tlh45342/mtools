# Makefile for minimal mtools-like utilities (mformat, mdir, minfo, mcp, mdel)

# ---- Toolchain ----
CC       ?= gcc
CFLAGS   ?= -Wall -Wextra -O2
CPPFLAGS ?=
LDFLAGS  ?=
LDLIBS   ?=

# ---- Host detection ----
UNAME_S := $(shell uname -s 2>/dev/null)
# If uname fails (rare), assume Windows_NT
ifeq ($(strip $(UNAME_S)),)
  UNAME_S := Windows_NT
endif

# Windows-like environments: Cygwin / MSYS / MinGW
HOST_WIN := 0
ifneq (,$(findstring CYGWIN,$(UNAME_S)))
  HOST_WIN := 1
endif
ifneq (,$(findstring MINGW,$(UNAME_S)))
  HOST_WIN := 1
endif
ifneq (,$(findstring MSYS,$(UNAME_S)))
  HOST_WIN := 1
endif

# ---- Executable suffix (Windows/Cygwin/MinGW => .exe) ----
EXEEXT := $(if $(findstring 1,$(HOST_WIN)),.exe,)

# ---- Install locations ----
PREFIX  ?= /usr/local
ifeq ($(HOST_WIN),1)
  # Default for your Windows/Cygwin setup
  BINDIR ?= /cygdrive/c/cygwin64/bin
else
  BINDIR ?= $(PREFIX)/bin
endif
DESTDIR ?=

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
.PHONY: install uninstall install-strip show-config
install: $(BINARIES)
	@echo "Host           : $(UNAME_S)  (WINDOWS-like=$(HOST_WIN))"
	@echo "Installing to  : $(DESTDIR)$(BINDIR)"
	mkdir -p "$(DESTDIR)$(BINDIR)"
	cp -f $(BINARIES) "$(DESTDIR)$(BINDIR)"

# Optional: install stripped binaries (Unix-y)
install-strip: CFLAGS += -s
install-strip: $(BINARIES)
	@echo "Host           : $(UNAME_S)  (WINDOWS-like=$(HOST_WIN))"
	@echo "Installing (strip) to: $(DESTDIR)$(BINDIR)"
	mkdir -p "$(DESTDIR)$(BINDIR)"
	@set -e; for b in $(BINARIES); do \
		strip "$$b" || true; \
		cp -f "$$b" "$(DESTDIR)$(BINDIR)"; \
	done

uninstall:
	@echo "Uninstalling from $(DESTDIR)$(BINDIR)"
	rm -f $(addprefix $(DESTDIR)$(BINDIR)/,$(notdir $(BINARIES)))

# ---- Clean ----
.PHONY: clean
clean:
	rm -rf "$(BUILD_DIR)"
	-rm -f floppy.img

# ---- Debug helper ----
show-config:
	@echo "UNAME_S = $(UNAME_S)"
	@echo "HOST_WIN = $(HOST_WIN)"
	@echo "EXEEXT = $(EXEEXT)"
	@echo "PREFIX = $(PREFIX)"
	@echo "BINDIR = $(BINDIR)"
	@echo "DESTDIR = $(DESTDIR)"
