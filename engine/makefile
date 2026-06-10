# ------------------------------------------------------------------------------
# Project
# ------------------------------------------------------------------------------

ASSEMBLY        := zen
BUILD_DIR       := bin
OBJ_DIR         := obj
SRC_DIR         := src

CXX             := clang++
CXX_FLAGS       := -std=c++11 -Wall -Wextra -Wshadow -fvisibility=hidden
LD_FLAGS        := -shared

INCLUDE_FLAGS   := -Isrc -Ivendor
DEFINES         := -DZEN_EXPORT

EXTENSION       :=

# ------------------------------------------------------------------------------
# Platform Detection
# ------------------------------------------------------------------------------

ifeq ($(OS),Windows_NT)
	PLATFORM   := win64
	EXTENSION  := .dll
	LIB_DIR    := lib\win64

	# Recursive wildcard
	rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))
	SRC_FILES   := $(call rwildcard,$(SRC_DIR)/,*.cpp)
	DIRECTORY   := $(subst /,\,${CURDIR})
	SHELL       := cmd
	DIRECTORIES := \$(SRC_DIR) $(subst $(DIRECTORY),,$(shell dir $(SRC_DIR) /S /AD /B | findstr /i $(SRC_DIR)))

	LD_FLAGS += \
		-L$(LIB_DIR) \
		-lSDL3 \
		-luser32 \
		-lws2_32 \
		-lwinmm

else
	UNAME_S := $(shell uname -s)

	SRC_FILES := $(shell find $(SRC_DIR) -type f \( -name "*.cpp" -o -name "*.mm" \))
	DIRECTORIES := $(shell find src -type d)

	ifeq ($(UNAME_S),Darwin)
		PLATFORM := macos
		EXTENSION  := .dylib
		LIB_DIR  := lib/macos

		LDFLAGS += \
			-L$(LIB_DIR) \
			-F$(LIB_DIR) \
			-framework SDL3
	endif

	ifeq ($(UNAME_S),Linux)
		PLATFORM := linux
		EXTENSION  := .so
		LIB_DIR  := lib/linux64

		LDFLAGS += \
			-lSDL3
	endif

endif

# ------------------------------------------------------------------------------
# Source/Object Lists
# ------------------------------------------------------------------------------

OBJ_FILES := $(SRC_FILES:%=$(OBJ_DIR)/%.o)

# ------------------------------------------------------------------------------
# Target Recipes
# ------------------------------------------------------------------------------

.PHONY: debug
debug: debug_flags scaffold compile link

.PHONY: release
release: release_flags scaffold compile link

.PHONY: debug_flags
debug_flags: CXX_FLAGS += -g -O0
debug_flags: LD_FLAGS += -g
debug_flags: DEFINES += -D_DEBUG

.PHONY: release_flags
release_flags: CXX_FLAGS += -O2

# ------------------------------------------------------------------------------
# Scaffold
# ------------------------------------------------------------------------------

.PHONY: scaffold
scaffold:
	@echo Scaffolding...
ifeq ($(PLATFORM),win64)
	@mkdir $(addprefix $(OBJ_DIR), $(DIRECTORIES)) 2>NUL || cd .
	@mkdir $(BUILD_DIR) 2>NUL || cd .
else
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(addprefix $(OBJ_DIR)/,$(DIRECTORIES))
endif
	@echo Done.

# ------------------------------------------------------------------------------
# Compile
# ------------------------------------------------------------------------------

# This phony just prints messages before compilation
.PHONY: compile
compile:
	@echo Compiler flags: $(CXX_FLAGS)
	@echo Defines:       $(DEFINES)
	@echo Compiling...

# Compile cpp to object
$(OBJ_DIR)/%.cpp.o: %.cpp
	@echo   $<...
	@$(CXX) $< $(CXX_FLAGS) -c -o $@ $(DEFINES) $(INCLUDE_FLAGS)

# ------------------------------------------------------------------------------
# Link
# ------------------------------------------------------------------------------

.PHONY: link
link: scaffold $(OBJ_FILES)
	@echo Linker flags $(LINKER_FLAGS)
	@echo Linking $(ASSEMBLY)...
ifeq ($(PLATFORM),win64)
	@$(CXX) $(OBJ_FILES) -o $(BUILD_DIR)\$(ASSEMBLY)$(EXTENSION) $(LD_FLAGS)
else
	@$(CXX) $(OBJ_FILES) -o $(BUILD_DIR)/$(ASSEMBLY)$(EXTENSION) $(LD_FLAGS)
endif

# ------------------------------------------------------------------------------
# Clean
# ------------------------------------------------------------------------------

.PHONY: clean
clean:
	@echo Cleaning...

ifeq ($(PLATFORM),win64)
	@if exist $(BUILD_DIR)\$(ASSEMBLY)$(EXTENSION) del $(BUILD_DIR)\$(TARGET)$(EXTENSION)
	@if exist $(OBJ_DIR) rmdir /s /q $(OBJ_DIR)
else
	@rm -rf $(OBJ_DIR)
	@rm -f $(BUILD_DIR)/$(ASSEMBLY)$(EXTENSION)
endif

# ------------------------------------------------------------------------------
# Utility Targets
# ------------------------------------------------------------------------------

.PHONY: libcopy
libcopy:
ifeq ($(PLATFORM),win64)
	-@setlocal enableextensions enabledelayedexpansion && xcopy $(LIB_DIR) $(BUILD_DIR) /Y
endif
ifeq ($(PLATFORM),macos)
	-@cp $(LIB_DIR)/*.a $(BUILD_DIR)/
	-@cp $(LIB_DIR)/*.dylib $(BUILD_DIR)/
endif
ifeq ($(PLATFORM),linux)
	-@cp $(LIB_DIR)/* $(BUILD_DIR)/
endif
