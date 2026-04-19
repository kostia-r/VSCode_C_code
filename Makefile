#####################################################################
# Target name: shall be the same as project folder
TARGET_NAME := $(notdir $(CURDIR)).exe

# Toolchain
MINGW_BIN := C:/mingw64/bin
CC := $(MINGW_BIN)/gcc.exe

# Source and include paths
SRC_DIRS := Src
INC_DIRS := Inc

# Build flags
DEFINES := -DDEBUG
CFLAGS := -Wall -g3 $(DEFINES)
LDFLAGS :=

# Output paths
BUILD_PATH := Build
TARGET := $(TARGET_NAME)

# Find all .c files in the source directories
SRC := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
OBJ := $(patsubst %.c,$(BUILD_PATH)/%.o,$(SRC))
INCLUDES := $(addprefix -I,$(INC_DIRS))

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
