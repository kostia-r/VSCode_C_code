MVM_CONFIG_DIR := Tools/integration/external-config-smoke

include MVM/vm.mk

CC := C:/mingw64/bin/gcc.exe
AR := C:/mingw64/bin/ar.exe
BUILD := Runs/StaticLibrarySmoke
OBJ_ROOT := $(BUILD)/obj
LIB := $(BUILD)/libmvm.a
OUT := $(BUILD)/static-library-smoke.exe
LIB_OBJ := $(patsubst %.c,$(OBJ_ROOT)/%.o,$(MVM_LIBRARY_SRC))
PARENT_SRC := $(MVM_CONFIG_SOURCE) Tools/integration/external-config-smoke/main.c
LIB_CFLAGS := -std=gnu11 -Wall -DMVM_ENABLE_DEFAULT_LOGGER=0
PARENT_INC := $(MVM_CONFIG_DIR) $(MVM_PUBLIC_INC)

.PHONY: all run

all: $(OUT)

$(OBJ_ROOT)/%.o: %.c
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
	$(CC) $(LIB_CFLAGS) $(addprefix -I,$(MVM_INC)) -c -o $@ $<

$(LIB): $(LIB_OBJ)
	$(AR) rcs $@ $^

$(OUT): $(LIB) $(PARENT_SRC)
	$(CC) -std=gnu11 -Wall $(addprefix -I,$(PARENT_INC)) -o $@ $(PARENT_SRC) $(LIB)

run: $(OUT)
	$< mpn/T310/DeepAbyss_v.1.4_T310_decrypted.mpn
