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
GRAPHVIZ_INSTALL_DIR=$(CURDIR)/$(GRAPHVIZ_DIR)-wasi-install

# Compiler and linker for WASI
CFLAGS := -D_WASI_EMULATED_SIGNAL -D_WASI_EMULATED_PROCESS_CLOCKS -mllvm -wasm-enable-sjlj
LDFLAGS := -lwasi-emulated-signal -Wl,--whole-archive -lc++ -lc++abi -Wl,--no-whole-archive -lwasi-emulated-process-clocks
CC := $(WASI_SDK_PATH)/bin/clang --sysroot=$(WASI_SYSROOT) $(CFLAGS)
CXX := $(WASI_SDK_PATH)/bin/clang++ --sysroot=$(WASI_SYSROOT) $(CFLAGS) -fno-exceptions
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

# --- Targets ---

.PHONY: all clean graphviz-download graphviz-configure graphviz-build graphviz-install wit-bindings component-core component-wasm

all: $(COMPONENT_WASM)

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
	# Patch solve_VPSC.cpp to replace C++ exceptions with fprintf and exit
	sed -i 's/throw std::runtime_error("Unsatisfied constraint");/fprintf(stderr, "Error: Unsatisfied constraint\\n"); exit(EXIT_FAILURE);/g' $(GRAPHVIZ_DIR)/lib/vpsc/solve_VPSC.cpp
	sed -i 's/throw std::runtime_error("Cycle Error!");/fprintf(stderr, "Error: Cycle Error!\\n"); exit(EXIT_FAILURE);/g' $(GRAPHVIZ_DIR)/lib/vpsc/solve_VPSC.cpp
	sed -i 's/throw std::runtime_error(s.str());/fprintf(stderr, "Error: %s\\n", s.str().c_str()); exit(EXIT_FAILURE);/g' $(GRAPHVIZ_DIR)/lib/vpsc/solve_VPSC.cpp
	# Add cstdio and cstdlib includes
	sed -i '1i#include <cstdio>\n#include <cstdlib>' $(GRAPHVIZ_DIR)/lib/vpsc/solve_VPSC.cpp
	# Remove try-catch block from csolve_VPSC.cpp - replace entire function body
	sed -i '/void satisfyVPSC(VPSC\* vpsc) {/,/^}/c\void satisfyVPSC(VPSC* vpsc) {\n    vpsc->satisfy();\n}' $(GRAPHVIZ_DIR)/lib/vpsc/csolve_VPSC.cpp
	mkdir -p $(GRAPHVIZ_BUILD_DIR)
	cd $(GRAPHVIZ_BUILD_DIR) && \
	CC="$(CC)" CXX="$(CXX)" AR="$(AR)" RANLIB="$(RANLIB)" \
	../$(GRAPHVIZ_DIR)/configure \
		--host=wasm32-wasi \
		--prefix=$(GRAPHVIZ_INSTALL_DIR) \
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
				graphviz-14.0.2-wasi-install/lib/graphviz/libgvplugin_core.a \
				graphviz-14.0.2-wasi-install/lib/graphviz/libgvplugin_dot_layout.a \
				graphviz-14.0.2-wasi-install/lib/graphviz/libgvplugin_neato_layout.a \
				-lm
component-core-embedded: component-core
	@echo "Embedding WIT metadata into core WASM..."
	wasm-tools component embed $(WIT_FILE) $(CORE_WASM) -o $(CORE_WASM) --world $(COMPONENT_NAME)

$(COMPONENT_WASM): component-core-embedded
	@echo "Creating WASM component..."
	wasm-tools component new $(CORE_WASM) -o $@

# --- Clean ---

clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(GRAPHVIZ_TARBALL) $(GRAPHVIZ_DIR) $(GRAPHVIZ_BUILD_DIR) $(GRAPHVIZ_INSTALL_DIR)
	rm -f src/$(BINDINGS_C) src/$(BINDINGS_H) src/$(BINDINGS_O)
	rm -f $(CORE_WASM) $(COMPONENT_WASM)
