#####################################################################
# Target name: shall be the same as project folder
TARGET_NAME := $(notdir $(CURDIR)).exe

# Toolchain
MINGW_BIN := C:/mingw64/bin
CC := $(MINGW_BIN)/gcc.exe
AR := $(MINGW_BIN)/ar.exe
SDL2_ROOT := $(CURDIR)/SDL2-2.32.6/x86_64-w64-mingw32
SDL2_BIN := $(SDL2_ROOT)/bin

include OpenMophun/OpenMophun.mk

# Source and include paths
APP_SRC_DIRS := Src
APP_INC_DIRS := $(SDL2_ROOT)/include/SDL2

# Build flags
DEFINES := -DDEBUG -DMVM_COMPILED_LOG_LEVEL=5
CFLAGS := -Wall -g3 $(DEFINES)
LDFLAGS := -L$(SDL2_ROOT)/lib -lSDL2 -lwinmm

# Output paths
BUILD_PATH := Build
TARGET := $(TARGET_NAME)
OPENMOPHUN_STATIC_LIB := libOpenMophun.a
MVM_STATIC_APP := MVM-static.exe

# Find all .c files in the source directories
APP_SRC := $(foreach dir,$(APP_SRC_DIRS),$(wildcard $(dir)/*.c))
SRC := $(APP_SRC) $(OPENMOPHUN_SRC)
OBJ := $(patsubst %.c,$(BUILD_PATH)/%.o,$(SRC))
OPENMOPHUN_LIB_OBJ := $(patsubst %.c,$(BUILD_PATH)/%.o,$(OPENMOPHUN_SRC))
APP_OBJ := $(patsubst %.c,$(BUILD_PATH)/%.o,$(APP_SRC))
DEP := $(OBJ:.o=.d)
INCLUDES := $(addprefix -I,$(APP_INC_DIRS) $(OPENMOPHUN_INC))

# Windows shell commands
MKDIR_BUILD = if not exist "$(subst /,\,$(BUILD_PATH))" mkdir "$(subst /,\,$(BUILD_PATH))"
MKDIR_SUBDIR = if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
RM_BUILD = if exist "$(subst /,\,$(BUILD_PATH))" rmdir /S /Q "$(subst /,\,$(BUILD_PATH))"
RM_TARGET = if exist "$(TARGET)" del /Q "$(TARGET)"
RM_STATIC_LIB = if exist "$(OPENMOPHUN_STATIC_LIB)" del /Q "$(OPENMOPHUN_STATIC_LIB)"
RM_STATIC_APP = if exist "$(MVM_STATIC_APP)" del /Q "$(MVM_STATIC_APP)"
RUN_WITH_LOCAL_PATH = set "PATH=$(subst /,\,$(SDL2_BIN));%PATH%" && 

.PHONY: default all static-lib static-app clean run rebuild

default: all

all: $(TARGET)

static-lib: $(OPENMOPHUN_STATIC_LIB)

static-app: $(MVM_STATIC_APP)

$(OPENMOPHUN_STATIC_LIB): $(OPENMOPHUN_LIB_OBJ)
	$(AR) rcs $@ $^

$(MVM_STATIC_APP): $(APP_OBJ) $(OPENMOPHUN_STATIC_LIB)
	$(CC) $(CFLAGS) -o $@ $(APP_OBJ) $(OPENMOPHUN_STATIC_LIB) $(LDFLAGS)

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
	@echo Cleaning $(OPENMOPHUN_STATIC_LIB)
	@$(RM_STATIC_LIB)
	@echo Cleaning $(MVM_STATIC_APP)
	@$(RM_STATIC_APP)

-include $(DEP)
