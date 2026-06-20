# ------------------------------------------------------------------------------
# Project
# ------------------------------------------------------------------------------

ASSEMBLY        := zen
BUILD_DIR       := bin
OBJ_DIR         := obj
SRC_DIR         := src

CXX             := clang++
CXX_FLAGS       := -std=c++17 -Wall -Wextra -Wshadow -fno-exceptions -Wno-missing-designated-field-initializers
LD_FLAGS        :=

INCLUDE_FLAGS   := -Isrc -Ivendor
DEFINES         := -D_CRT_SECURE_NO_WARNINGS

EXTENSION       :=

# ------------------------------------------------------------------------------
# Build configuration
# ------------------------------------------------------------------------------

ifeq ($(RELEASE),1)
	CXX_FLAGS += -O2
else
	CXX_FLAGS += -g -O0
	LD_FLAGS += -g
	DEFINES += -DZEN_DEBUG
endif

# ------------------------------------------------------------------------------
# Platform Detection
# ------------------------------------------------------------------------------

ifeq ($(OS),Windows_NT)
	PLATFORM   := win64
	EXTENSION  := .exe
	LIB_DIR    := lib\win64

	# Recursive wildcard
	rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))
	SRC_FILES   := $(call rwildcard,$(SRC_DIR)/,*.cpp)
	DIRECTORY   := $(subst /,\,${CURDIR})
	SHELL       := cmd
	DIRECTORIES := \$(SRC_DIR) $(subst $(DIRECTORY),,$(shell dir $(SRC_DIR) /S /AD /B | findstr /i $(SRC_DIR)))

	SHADER_SRC_DIR := res\shader
	SHADER_FILES := \
	    $(call rwildcard,$(SHADER_SRC_DIR)/,*.vert.glsl) \
    	$(call rwildcard,$(SHADER_SRC_DIR)/,*.frag.glsl)
	SHADER_DIRECTORIES := \$(SHADER_SRC_DIR) $(subst $(DIRECTORY),,$(shell dir $(SHADER_SRC_DIR) /S /AD /B | findstr /i $(SHADER_SRC_DIR)))

	LD_FLAGS += \
		-L$(LIB_DIR) \
		-lSDL3 \
		-lvulkan-1

else
	UNAME_S := $(shell uname -s)

	SRC_FILES := $(shell find $(SRC_DIR) -type f \( -name "*.cpp" -o -name "*.mm" \))
	DIRECTORIES := $(shell find src -type d)

	SHADER_SRC_DIR := res/shader
	SHADER_FILES := \
    	$(shell find $(SHADER_SRC_DIR) -type f \( -name "*.vert.glsl" \)) \
    	$(shell find $(SHADER_SRC_DIR) -type f \( -name "*.frag.glsl" \))
	SHADER_DIRECTORIES := $(shell find $(SHADER_SRC_DIR) -type d)

	ifeq ($(UNAME_S),Darwin)
		PLATFORM := macos
		LIB_DIR  := lib/macos

		LD_FLAGS += \
			-L$(LIB_DIR) \
			-lvulkan \
			-F$(LIB_DIR) \
			-framework SDL3
		DEFINES += \
			-Wno-deprecated-declarations

		# rpath
		ifeq ($(RELEASE),1)
			LD_FLAGS += -Wl,-rpath,.
		else
			# In debug mode, the rpath points to ../lib/macos
			# This way we don't have to libcopy
			LD_FLAGS += -Wl,-rpath,../$(LIB_DIR)
		endif
	endif

	ifeq ($(UNAME_S),Linux)
		PLATFORM := linux
		LIB_DIR  := lib/linux64

		LD_FLAGS += \
			-lSDL3
	endif

endif

# ------------------------------------------------------------------------------
# Source/Object Lists
# ------------------------------------------------------------------------------

OBJ_FILES := $(SRC_FILES:%=$(OBJ_DIR)/%.o)
SPV_FILES = $(patsubst $(SHADER_SRC_DIR)/%,$(BUILD_DIR)/$(SHADER_SRC_DIR)/%,$(SHADER_FILES:.glsl=.spv))

# ------------------------------------------------------------------------------
# Target Recipes
# ------------------------------------------------------------------------------

.PHONY: all
all: scaffold compile shader-compile link

# ------------------------------------------------------------------------------
# Scaffold
# ------------------------------------------------------------------------------

.PHONY: scaffold
scaffold:
	@echo Scaffolding...
ifeq ($(PLATFORM),win64)
	@mkdir $(addprefix $(OBJ_DIR), $(DIRECTORIES)) 2>NUL || cd .
	@mkdir $(BUILD_DIR) 2>NUL || cd .
	@mkdir $(addprefix $(BUILD_DIR), $(SHADER_DIRECTORIES)) 2>NUL || cd .
else
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(addprefix $(OBJ_DIR)/,$(DIRECTORIES))
	@mkdir -p $(addprefix $(BUILD_DIR)/,$(SHADER_DIRECTORIES))
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
# Shader Compile
# ------------------------------------------------------------------------------

.PHONY: shader-compile
shader-compile: $(SPV_FILES)

# Compile vertex glsl to spir-v
$(BUILD_DIR)/%.vert.spv: %.vert.glsl
	@echo   $<...
	@glslc -fshader-stage=vert $< -o $@

# Compile fragment glsl to spir-v
$(BUILD_DIR)/%.frag.spv: %.frag.glsl
	@echo   $<...
	@glslc -fshader-stage=frag $< -o $@

# ------------------------------------------------------------------------------
# Link
# ------------------------------------------------------------------------------

.PHONY: link
link: scaffold $(OBJ_FILES)
	@echo Linker flags $(LD_FLAGS)
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
	@if exist $(BUILD_DIR)\$(ASSEMBLY)$(EXTENSION) del $(BUILD_DIR)\$(ASSEMBLY)$(EXTENSION)
	@if exist $(OBJ_DIR) rmdir /s /q $(OBJ_DIR)
	@if exist $(BUILD_DIR)\res rmdir /s /q $(BUILD_DIR)\res
else
	@rm -rf $(OBJ_DIR)
	@rm -f $(BUILD_DIR)/$(ASSEMBLY)$(EXTENSION)
	@rm -rf $(BUILD_DIR)/res
endif

# ------------------------------------------------------------------------------
# Utility Targets
# ------------------------------------------------------------------------------

.PHONY: libcopy
libcopy:
ifeq ($(PLATFORM),win64)
	-@setlocal enableextensions && xcopy $(LIB_DIR) $(BUILD_DIR) /Y
endif
ifeq ($(PLATFORM),macos)
	-@cp $(LIB_DIR)/*.dylib $(BUILD_DIR)/
endif
ifeq ($(PLATFORM),linux)
	-@cp $(LIB_DIR)/* $(BUILD_DIR)/
endif
