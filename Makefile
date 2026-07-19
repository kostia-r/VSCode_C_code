#####################################################################
# Target name: shall be the same as project folder
TARGET_NAME := $(notdir $(CURDIR)).exe

# Toolchain
MINGW_BIN := C:/mingw64/bin
CC := $(MINGW_BIN)/gcc.exe
AR := $(MINGW_BIN)/ar.exe
SDL2_ROOT := $(CURDIR)/SDL2-2.32.6/x86_64-w64-mingw32
SDL2_BIN := $(SDL2_ROOT)/bin

include MVM/vm.mk

# Source and include paths
APP_SRC_DIRS := Src
APP_INC_DIRS := $(SDL2_ROOT)/include/SDL2

# Build flags
DEFINES := -DDEBUG
CFLAGS := -Wall -g3 $(DEFINES)
LDFLAGS := -L$(SDL2_ROOT)/lib -lSDL2 -lwinmm

# Output paths
BUILD_PATH := Build
TARGET := $(TARGET_NAME)
MVM_STATIC_LIB := libmvm.a

# Find all .c files in the source directories
APP_SRC := $(foreach dir,$(APP_SRC_DIRS),$(wildcard $(dir)/*.c))
SRC := $(APP_SRC) $(MVM_SRC)
OBJ := $(patsubst %.c,$(BUILD_PATH)/%.o,$(SRC))
MVM_LIB_OBJ := $(patsubst %.c,$(BUILD_PATH)/%.o,$(MVM_LIBRARY_SRC))
DEP := $(OBJ:.o=.d)
INCLUDES := $(addprefix -I,$(APP_INC_DIRS) $(MVM_INC))

# Windows shell commands
MKDIR_BUILD = if not exist "$(subst /,\,$(BUILD_PATH))" mkdir "$(subst /,\,$(BUILD_PATH))"
MKDIR_SUBDIR = if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
RM_BUILD = if exist "$(subst /,\,$(BUILD_PATH))" rmdir /S /Q "$(subst /,\,$(BUILD_PATH))"
RM_TARGET = if exist "$(TARGET)" del /Q "$(TARGET)"
RM_STATIC_LIB = if exist "$(MVM_STATIC_LIB)" del /Q "$(MVM_STATIC_LIB)"
RUN_WITH_LOCAL_PATH = set "PATH=$(subst /,\,$(SDL2_BIN));%PATH%" && 

.PHONY: default all static-lib clean run rebuild

default: all

all: $(TARGET)

static-lib: $(MVM_STATIC_LIB)

$(MVM_STATIC_LIB): $(MVM_LIB_OBJ)
	$(AR) rcs $@ $^

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

$(BUILD_PATH)/%.o: %.c
	@$(MKDIR_SUBDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c -o $@ $<

$(BUILD_PATH):
	@$(MKDIR_BUILD)

run: all
	$(RUN_WITH_LOCAL_PATH) .\$(TARGET)

rebuild: clean all

clean:
	@echo Cleaning $(BUILD_PATH)
	@$(RM_BUILD)
	@echo Cleaning $(TARGET)
	@$(RM_TARGET)
	@echo Cleaning $(MVM_STATIC_LIB)
	@$(RM_STATIC_LIB)

-include $(DEP)
