#####################################################################
# Target name: shall be the same as project folder
TARGET_NAME := $(notdir $(CURDIR)).exe

# Toolchain
MINGW_BIN := C:/mingw64/bin
CC := $(MINGW_BIN)/gcc.exe

include MVM/vm.mk

# Source and include paths
APP_SRC_DIRS := Src
APP_INC_DIRS :=

# Build flags
DEFINES := -DDEBUG
CFLAGS := -Wall -g3 $(DEFINES)
LDFLAGS :=

# Output paths
BUILD_PATH := Build
TARGET := $(TARGET_NAME)

# Find all .c files in the source directories
APP_SRC := $(foreach dir,$(APP_SRC_DIRS),$(wildcard $(dir)/*.c))
SRC := $(APP_SRC) $(MVM_SRC)
OBJ := $(patsubst %.c,$(BUILD_PATH)/%.o,$(SRC))
INCLUDES := $(addprefix -I,$(APP_INC_DIRS) $(MVM_INC))

# Windows shell commands
MKDIR_BUILD = if not exist "$(subst /,\,$(BUILD_PATH))" mkdir "$(subst /,\,$(BUILD_PATH))"
MKDIR_SUBDIR = if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
RM_BUILD = if exist "$(subst /,\,$(BUILD_PATH))" rmdir /S /Q "$(subst /,\,$(BUILD_PATH))"
RM_TARGET = if exist "$(TARGET)" del /Q "$(TARGET)"

.PHONY: default all clean run rebuild

default: all

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

$(BUILD_PATH)/%.o: %.c
	@$(MKDIR_SUBDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILD_PATH):
	@$(MKDIR_BUILD)

run: all
	.\$(TARGET)

rebuild: clean all

clean:
	@echo Cleaning $(BUILD_PATH)
	@$(RM_BUILD)
	@echo Cleaning $(TARGET)
	@$(RM_TARGET)
