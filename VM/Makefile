VM_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
include $(VM_DIR)/vm.mk

.PHONY: print

print:
	@echo MOPHUN_VM_INC=$(MOPHUN_VM_INC)
	@echo MOPHUN_VM_SRC=$(MOPHUN_VM_SRC)
