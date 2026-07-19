MVM_CONFIG_DIR := Tools/integration/external-config-smoke

include MVM/vm.mk

CC := C:/mingw64/bin/gcc.exe
OUT := Runs/ExternalConfigSmoke/external-config-smoke.exe

.PHONY: all run

all: $(OUT)

$(OUT): $(MVM_SRC) Tools/integration/external-config-smoke/main.c
	@if not exist "Runs\ExternalConfigSmoke" mkdir "Runs\ExternalConfigSmoke"
	$(CC) -std=gnu11 -Wall -Wextra -DMVM_ENABLE_DEFAULT_LOGGER=0 -DMVM_MAX_LOG_LEVEL=0 \
	  $(addprefix -I,$(MVM_INC)) -o $@ $^

run: $(OUT)
	$< mpn/T310/DeepAbyss_v.1.4_T310_decrypted.mpn
