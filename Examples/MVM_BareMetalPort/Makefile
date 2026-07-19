MVM_CONFIG_DIR := Examples/MVM_BareMetalPort

include MVM/vm.mk

CC := C:/mingw64/bin/gcc.exe
OUT := Runs/BareMetalPortBuild/mvm-bare-metal-port.exe
FULL_OUT := Runs/BareMetalPortBuild/mvm-bare-metal-port-full.exe
PORT_SRC := Examples/MVM_BareMetalPort/MVM_Port.c Examples/MVM_BareMetalPort/main.c

.PHONY: all full run

all: $(OUT)

$(OUT): $(MVM_SRC) $(PORT_SRC)
	@if not exist "Runs\BareMetalPortBuild" mkdir "Runs\BareMetalPortBuild"
	$(CC) -std=gnu11 -Wall -DMVM_PORT_BUILD_CHECK=1 -DMVM_ENABLE_DEFAULT_LOGGER=0 \
	  $(addprefix -I,$(MVM_INC)) -o $@ $^

$(FULL_OUT): $(MVM_SRC) $(PORT_SRC)
	@if not exist "Runs\BareMetalPortBuild" mkdir "Runs\BareMetalPortBuild"
	$(CC) -std=gnu11 -Wall -DMVM_ENABLE_DEFAULT_LOGGER=0 $(addprefix -I,$(MVM_INC)) -o $@ $^

full: $(FULL_OUT)

run: $(OUT) $(FULL_OUT)
	$<
