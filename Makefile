#####################################################################
# Target name: shall be the same as project folder
TARGET_NAME := $(notdir $(CURDIR))
# List of source paths
SRC_DIRS += Src
# List of include paths
INC_DIRS += Inc
# Debug Compilation flags: DEBUG, VERSION="2.1" (as string)
DEFINES_DEBUG := -DDEBUG -DVERSION=\"2.1\"
# Release Compilation flags: RELEASE
DEFINES_RELEASE := -DRELEASE
#####################################################################

# Detect the OS
ifeq ($(OS),Windows_NT)
    # Windows settings
    RM = rmdir /S /Q
    MKDIR = if not exist "$(OBJ_PATH)" mkdir "$(OBJ_PATH)"
    MKDIR_SUBDIR = if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
    TARGET_NAME := $(addsuffix .exe,$(TARGET_NAME))
    RUN_CMD_DEBUG = $(DEBUG_TARGET)
    RUN_CMD_RELEASE = $(RELEASE_TARGET)
else
    # Linux settings
    RM = rm -rf
    MKDIR = mkdir -p $(OBJ_PATH)
    MKDIR_SUBDIR = mkdir -p $(dir $@)
    RUN_CMD_DEBUG = ./$(DEBUG_TARGET)
    RUN_CMD_RELEASE = ./$(RELEASE_TARGET)
endif

# tool macros
CC := gcc
CCFLAGS_DEBUG := -Wall $(DEFINES_DEBUG)
CCFLAGS_RELEASE := -Wall $(DEFINES_RELEASE)
DBGFLAGS := -g3
RELFLAGS := -O2
CCOBJFLAGS_DEBUG := $(CCFLAGS_DEBUG) -c
CCOBJFLAGS_RELEASE := $(CCFLAGS_RELEASE) -c

# Binary output path
BIN_PATH = Debug
REL_PATH = Release

# path macros
OBJ_PATH = $(BIN_PATH)
REL_OBJ_PATH = $(REL_PATH)

# Generate include flags for the compiler (-I for each include directory)
INCLUDES := $(addprefix -I,$(INC_DIRS))

# compile macros
DEBUG_TARGET := $(BIN_PATH)/$(TARGET_NAME)
RELEASE_TARGET := $(REL_PATH)/$(TARGET_NAME)

# Find all .c files in the SRC_DIRS
SRC := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))

# Generate the corresponding .o files in the Debug/src/ directory
OBJ := $(patsubst %.c,$(OBJ_PATH)/%.o,$(SRC))
REL_OBJ := $(patsubst %.c,$(REL_OBJ_PATH)/%.o,$(SRC))

# clean files list
DISTCLEAN_LIST := $(OBJ)

# default rule
default: makedir all

# non-phony targets:

# linking debug mode
$(DEBUG_TARGET): $(OBJ)
#	@echo "Linking debug target $(DEBUG_TARGET) ..."
#	@echo "Object files: $(OBJ)"
	$(CC) $(CCFLAGS_DEBUG) -o $@ $(OBJ)

# linking release mode
$(RELEASE_TARGET): $(REL_OBJ)
#	@echo "Linking release target $(RELEASE_TARGET) ..."
#	@echo "Object files: $(REL_OBJ)"
	$(CC) $(CCFLAGS_RELEASE) $(RELFLAGS) -o $@ $(REL_OBJ)

# Rule for compiling .c files into .o object files
$(OBJ_PATH)/%.o: %.c
#	@echo "Creating directory: $(dir $@)"
	@$(MKDIR_SUBDIR)
#	@echo "Compiling $< into $@ ..."
	$(CC) $(CCOBJFLAGS_DEBUG) $(DBGFLAGS) $(INCLUDES) -o $@ $<

$(REL_OBJ_PATH)/%.o: %.c
	@$(MKDIR_SUBDIR)
#	@echo "Compiling $< into $@ ..."
	$(CC) $(CCOBJFLAGS_RELEASE) $(RELFLAGS) $(INCLUDES) -o $@ $<

# phony rules
.PHONY: makedir
makedir:
	$(MKDIR)

.PHONY: all
all: $(DEBUG_TARGET) $(RELEASE_TARGET)

.PHONY: debug
debug: $(DEBUG_TARGET)

.PHONY: release
release: $(RELEASE_TARGET)

.PHONY: run
run: makedir all
	$(RUN_CMD_RELEASE)

.PHONY: clean
clean:
	@echo "Cleaning $(BIN_PATH)"
	$(RM) $(BIN_PATH)
	@echo "Cleaning $(REL_PATH)"
	$(RM) $(REL_PATH)

.PHONY: distclean
distclean:
	@echo "Cleaning $(DISTCLEAN_LIST)"
	$(RM) $(DISTCLEAN_LIST)
