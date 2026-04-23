MVM_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
include $(MVM_DIR)/vm.mk

.PHONY: print

print:
	@echo MVM_INC=$(MVM_INC)
	@echo MVM_SRC=$(MVM_SRC)
