# Mophun VM component make fragment.
#
# Integration:
#   include path/to/VM/vm.mk
#   C_SOURCES += $(MOPHUN_VM_SRC)
#   C_INCLUDES += $(addprefix -I,$(MOPHUN_VM_INC))

MOPHUN_VM_ROOT := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

MOPHUN_VM_PUBLIC_INC := \
  $(MOPHUN_VM_ROOT)/inc

MOPHUN_VM_INTERNAL_INC := \
  $(MOPHUN_VM_ROOT)/core/inc \
  $(MOPHUN_VM_ROOT)/debug/inc \
  $(MOPHUN_VM_ROOT)/loader/inc \
  $(MOPHUN_VM_ROOT)/pip/inc \
  $(MOPHUN_VM_ROOT)/runtime/inc

MOPHUN_VM_INC := \
  $(MOPHUN_VM_PUBLIC_INC) \
  $(MOPHUN_VM_INTERNAL_INC)

MOPHUN_VM_SRC := \
  $(MOPHUN_VM_ROOT)/core/src/mophun_vm.c \
  $(MOPHUN_VM_ROOT)/debug/src/mophun_trace.c \
  $(MOPHUN_VM_ROOT)/debug/src/vmgp_debug.c \
  $(MOPHUN_VM_ROOT)/loader/src/vmgp_loader.c \
  $(MOPHUN_VM_ROOT)/pip/src/pip_exec.c \
  $(MOPHUN_VM_ROOT)/runtime/src/mophun_caps.c \
  $(MOPHUN_VM_ROOT)/runtime/src/mophun_decompress.c \
  $(MOPHUN_VM_ROOT)/runtime/src/mophun_heap.c \
  $(MOPHUN_VM_ROOT)/runtime/src/mophun_misc.c \
  $(MOPHUN_VM_ROOT)/runtime/src/mophun_runtime_common.c \
  $(MOPHUN_VM_ROOT)/runtime/src/mophun_streams.c \
  $(MOPHUN_VM_ROOT)/runtime/src/mophun_strings.c \
  $(MOPHUN_VM_ROOT)/runtime/src/mophun_syscall_dispatch.c \
  $(MOPHUN_VM_ROOT)/runtime/src/mophun_time_random.c
