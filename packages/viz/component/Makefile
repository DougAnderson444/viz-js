# packages/viz/component/Makefile

# --- Configuration ---
WASI_SDK_PATH = /opt/wasi-sdk
WASI_SYSROOT := $(WASI_SDK_PATH)/share/wasi-sysroot

# Graphviz configuration
GRAPHVIZ_VERSION=14.0.2
GRAPHVIZ_TARBALL=graphviz-${GRAPHVIZ_VERSION}.tar.gz
GRAPHVIZ_URL=https://gitlab.com/api/v4/projects/4207231/packages/generic/graphviz-releases/${GRAPHVIZ_VERSION}/${GRAPHVIZ_TARBALL}
GRAPHVIZ_DIR=graphviz-${GRAPHVIZ_VERSION}
GRAPHVIZ_BUILD_DIR=$(GRAPHVIZ_DIR)-wasi-build
GRAPHVIZ_INSTALL_DIR=$(GRAPHVIZ_DIR)-wasi-install

# Compiler and linker for WASI
CFLAGS := -D_WASI_EMULATED_SIGNAL -D_WASI_EMULATED_PROCESS_CLOCKS -mllvm -wasm-enable-sjlj
LDFLAGS := -lwasi-emulated-signal -Wl,--whole-archive -lc++abi -Wl,--no-whole-archive -lwasi-emulated-process-clocks
CC := $(WASI_SDK_PATH)/bin/clang --sysroot=$(WASI_SYSROOT) $(CFLAGS)
CXX := $(WASI_SDK_PATH)/bin/clang++ --sysroot=$(WASI_SYSROOT) $(CFLAGS) -fwasm-exceptions
AR := $(WASI_SDK_PATH)/bin/llvm-ar
RANLIB := $(WASI_SDK_PATH)/bin/llvm-ranlib
LD := $(WASI_SDK_PATH)/bin/wasm-ld

# wit-bindgen
WIT_BINDGEN := wit-bindgen

# Component files
WIT_FILE := wit/world.wit
COMPONENT_C_SRC := src/component.c
COMPONENT_NAME := viz

# Output files
BINDINGS_C := $(COMPONENT_NAME).c
BINDINGS_H := $(COMPONENT_NAME).h
BINDINGS_O := $(COMPONENT_NAME)_component_type.o
CORE_WASM := $(COMPONENT_NAME).core.wasm
COMPONENT_WASM := $(COMPONENT_NAME).wasm
CORE_WASM_ADAPTED := $(COMPONENT_NAME).core.adapted.wasm

WASM_TOOLS_DIR := $(shell mktemp -d -t wasmtime-XXXXXX)
WASI_ADAPTER_PATH := $(WASM_TOOLS_DIR)/wasmtime/target/wasm32-unknown-unknown/release/wasi_snapshot_preview1.wasm

# --- Targets ---

.PHONY: all clean graphviz-download graphviz-configure graphviz-build graphviz-install wit-bindings component-core component-wasm wasi-adapter

all: $(COMPONENT_WASM)

# --- WASI Adapter Build Steps ---
wasi-adapter:
	@echo "Cloning wasmtime repository into $(WASM_TOOLS_DIR)..."
	git clone https://github.com/bytecodealliance/wasmtime.git $(WASM_TOOLS_DIR)/wasmtime
	@echo "Building wasi_snapshot_preview1.wasm adapter..."
	cargo build -p wasi-preview1-component-adapter --target wasm32-unknown-unknown --release --manifest-path $(WASM_TOOLS_DIR)/wasmtime/Cargo.toml

# --- Graphviz Build Steps ---

graphviz-download: $(GRAPHVIZ_TARBALL)
$(GRAPHVIZ_TARBALL):
	@echo "Downloading Graphviz $(GRAPHVIZ_VERSION)..."
	curl -L -o $@ $(GRAPHVIZ_URL)

$(GRAPHVIZ_DIR): $(GRAPHVIZ_TARBALL)
	@echo "Extracting Graphviz..."
	tar -xzf $<

CONFIG_GUESS_URL := https://git.savannah.gnu.org/cgit/config.git/plain/config.guess
CONFIG_SUB_URL := https://git.savannah.gnu.org/cgit/config.git/plain/config.sub

download-config-scripts:
	@echo "Checking for config.guess and config.sub..."
	@if [ ! -f config.guess ]; then \
		echo "Downloading config.guess..."; \
		curl -L -o config.guess $(CONFIG_GUESS_URL); \
	fi
	@if [ ! -f config.sub ]; then \
		echo "Downloading config.sub..."; \
		curl -L -o config.sub $(CONFIG_SUB_URL); \
	fi

graphviz-configure: $(GRAPHVIZ_DIR) download-config-scripts
	@echo "Configuring Graphviz for WASI..."
	# Update config.sub and config.guess for WASI support
	cp config.guess $(GRAPHVIZ_DIR)/config/config.guess
	cp config.sub $(GRAPHVIZ_DIR)/config/config.sub
	chmod +x $(GRAPHVIZ_DIR)/config/config.guess
	chmod +x $(GRAPHVIZ_DIR)/config/config.sub
	# Patch gvloadimage_core.c to include unistd.h for read()
	sed -i '1i#include <unistd.h>' $(GRAPHVIZ_DIR)/plugin/core/gvloadimage_core.c
	# Patch code for WASI compatibility
	sed -i 's/if ((us->type == US_EPSF) && us->data)/if (0 \&\& (us->type == US_EPSF) \&\& us->data)/g' $(GRAPHVIZ_DIR)/plugin/core/gvloadimage_core.c
	sed -i 's/flockfile(file);/\/\* flockfile disabled for WASI \*\//g' $(GRAPHVIZ_DIR)/lib/util/lockfile.h
	sed -i 's/funlockfile(file);/\/\* funlockfile disabled for WASI \*\//g' $(GRAPHVIZ_DIR)/lib/util/lockfile.h
	mkdir -p $(GRAPHVIZ_BUILD_DIR)
	cd $(GRAPHVIZ_BUILD_DIR) && \
	CC="$(CC)" CXX="$(CXX)" AR="$(AR)" RANLIB="$(RANLIB)" LDFLAGS="$(LDFLAGS)" \
	../$(GRAPHVIZ_DIR)/configure \
		--host=wasm32-wasi \
		--prefix=$(abspath $(GRAPHVIZ_INSTALL_DIR)) \
		--disable-shared \
		--enable-static \
		--disable-dependency-tracking \
		--without-x \
		--disable-swig \
		--disable-tcl \
		--without-qt \
		--without-gtk \
		--without-gts \
		--without-devil \
		--without-webp \
		--without-rsvg \
		--without-lasi \
		--without-expat \
		--without-fontconfig \
		--without-freetype2 \
		--without-xpm \
		--without-jpeg \
		--without-png \
		--without-gif \
		--without-poppler \
		--without-cairo \
		--without-visio \
		--without-glitz \
		--without-gdk \
		--without-gdk-pixbuf \
		--without-pangocairo \
		--enable-tools=no \
		--disable-ltdl

graphviz-build: graphviz-configure
	@echo "Building and installing Graphviz libraries..."
	$(MAKE) -C $(GRAPHVIZ_BUILD_DIR)/lib install
	@echo "Building and installing Graphviz plugins..."
	$(MAKE) -C $(GRAPHVIZ_BUILD_DIR)/plugin install

graphviz-install: graphviz-build
	@echo "Graphviz libraries and plugins already installed."

# --- Component Build Steps ---

wit-bindings: $(WIT_FILE)
	@echo "Generating C bindings from $(WIT_FILE)..."
	mkdir -p src
	$(WIT_BINDGEN) c $(WIT_FILE) --out-dir src --world $(COMPONENT_NAME)

component-core: wit-bindings $(COMPONENT_C_SRC) graphviz-install
	@echo "Compiling component core WASM..."
	$(CC) \
		-I$(WASI_SYSROOT)/include \
		-I$(GRAPHVIZ_INSTALL_DIR)/include \
		-I$(GRAPHVIZ_INSTALL_DIR)/include/graphviz \
		-Isrc \
		-mexec-model=reactor \
		-Wl,--no-entry \
		-Wl,--export-all \
		-Wl,--allow-undefined \
		$(LDFLAGS) \
		-o $(CORE_WASM) \
		src/$(BINDINGS_C) \
		$(COMPONENT_C_SRC) \
		src/wasi_stubs.c \
		src/$(BINDINGS_O) \
        graphviz-14.0.2-wasi-install/lib/libgvc.a \
		graphviz-14.0.2-wasi-install/lib/libcgraph.a \
		graphviz-14.0.2-wasi-install/lib/libcdt.a \
		graphviz-14.0.2-wasi-install/lib/libpathplan.a \
		graphviz-14.0.2-wasi-install/lib/libxdot.a \
		-lm

component-core-embedded: component-core wasi-adapter
	@echo "Embedding WIT metadata into core WASM..."
	wasm-tools component embed $(WIT_FILE) $(CORE_WASM) -o $(CORE_WASM) --world $(COMPONENT_NAME)
	@echo "Adapting core WASM with wasi_snapshot_preview1.wasm..."
	wasm-tools component new $(CORE_WASM) --adapt $(WASI_ADAPTER_PATH) -o $(CORE_WASM_ADAPTED)

$(COMPONENT_WASM): component-core-embedded
	@echo "Creating WASM component..."
	mv $(CORE_WASM_ADAPTED) $@

# --- Clean ---

clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(GRAPHVIZ_TARBALL) $(GRAPHVIZ_DIR) $(GRAPHVIZ_BUILD_DIR) $(GRAPHVIZ_INSTALL_DIR)
	rm -f src/$(BINDINGS_C) src/$(BINDINGS_H) src/$(BINDINGS_O)
	rm -f $(CORE_WASM) $(COMPONENT_WASM) $(CORE_WASM_ADAPTED)
	rm -rf $(WASM_TOOLS_DIR)
