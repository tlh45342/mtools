# Makefile for minimal mtools-like utilities (mformat, mdir, minfo, mcp, mdel)

# ---- Toolchain ----
CC        ?= gcc
CFLAGS    ?= -Wall -Wextra -O2
CPPFLAGS  ?=
LDFLAGS   ?=
LDLIBS    ?=
INSTALL   ?= install
STRIP     ?= strip

# ---- Host detection (for install path & exe suffix) ----
UNAME_S := $(shell uname -s 2>/dev/null)
ifeq ($(strip $(UNAME_S)),)
  UNAME_S := Windows_NT
endif

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

# ---- Executable suffix (Windows-like => .exe) ----
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
PROGS     := mformat mdir minfo mcp mdel mmd
SRCS      := $(addprefix $(SRC_DIR)/,$(addsuffix .c,$(PROGS)))
BINARIES  := $(addprefix $(BUILD_DIR)/,$(addsuffix $(EXEEXT),$(PROGS)))

# ---- Default target ----
.PHONY: all
all: $(BUILD_DIR) $(BINARIES)

# Ensure build directory exists
$(BUILD_DIR):
	mkdir -p "$(BUILD_DIR)"

# ---- Pattern rule: src/<name>.c -> build/<name>$(EXEEXT) ----
$(BUILD_DIR)/%$(EXEEXT): $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)

# ---- Convenience targets (e.g., `make mdir`) ----
.PHONY: $(PROGS)
$(PROGS): %: $(BUILD_DIR)/%$(EXEEXT)
	@echo "Built $<"

# ---- Install / uninstall ----
.PHONY: install uninstall install-strip show-config
install: $(BINARIES)
	@echo "Host           : $(UNAME_S) (WINDOWS-like=$(HOST_WIN))"
	@echo "Installing to  : $(DESTDIR)$(BINDIR)"
	mkdir -p "$(DESTDIR)$(BINDIR)"
	@set -e; for b in $(BINARIES); do \
		n="$$(basename $$b)"; \
		$(INSTALL) -m 0755 "$$b" "$(DESTDIR)$(BINDIR)/$$n"; \
	done

install-strip: $(BINARIES)
	@echo "Host           : $(UNAME_S) (WINDOWS-like=$(HOST_WIN))"
	@echo "Installing (strip) to: $(DESTDIR)$(BINDIR)"
	mkdir -p "$(DESTDIR)$(BINDIR)"
	@set -e; for b in $(BINARIES); do \
		$(STRIP) "$$b" || true; \
		n="$$(basename $$b)"; \
		$(INSTALL) -m 0755 "$$b" "$(DESTDIR)$(BINDIR)/$$n"; \
	done

uninstall:
	@echo "Uninstalling from $(DESTDIR)$(BINDIR)"
	@set -e; for b in $(BINARIES); do \
		n="$$(basename $$b)"; \
		rm -f "$(DESTDIR)$(BINDIR)/$$n"; \
	done

# ---- Clean ----
.PHONY: clean
clean:
	rm -rf "$(BUILD_DIR)"
	# remove any test artifacts you may create
	-rm -f floppy.img

# ---- Debug helper ----
show-config:
	@echo "UNAME_S = $(UNAME_S)"
	@echo "HOST_WIN = $(HOST_WIN)"
	@echo "EXEEXT = $(EXEEXT)"
	@echo "PREFIX = $(PREFIX)"
	@echo "BINDIR = $(BINDIR)"
	@echo "DESTDIR = $(DESTDIR)"

