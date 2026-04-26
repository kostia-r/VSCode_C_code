# Mophun VM component make fragment.
#
# Integration:
#   include path/to/MVM/vm.mk
#   C_SOURCES += $(MVM_SRC)
#   C_INCLUDES += $(addprefix -I,$(MVM_INC))

MVM_ROOT := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

MVM_PUBLIC_INC := \
  $(MVM_ROOT)/inc

MVM_INTERNAL_INC := \
  $(MVM_ROOT)/core/inc \
  $(MVM_ROOT)/debug/inc \
  $(MVM_ROOT)/runtime/inc \
  $(MVM_ROOT)/Config

MVM_INC := \
  $(MVM_PUBLIC_INC) \
  $(MVM_INTERNAL_INC)

MVM_SRC := \
  $(MVM_ROOT)/Config/MVM_Lcfg.c \
  $(MVM_ROOT)/core/src/MVM_Core.c \
  $(MVM_ROOT)/core/src/MVM_Log.c \
  $(MVM_ROOT)/debug/src/MVM_Trace.c \
  $(MVM_ROOT)/debug/src/MVM_VmgpDebug.c \
  $(MVM_ROOT)/loader/src/MVM_VmgpLoader.c \
  $(MVM_ROOT)/pip/src/MVM_PipExec.c \
  $(MVM_ROOT)/runtime/src/MVM_Imports.c \
  $(MVM_ROOT)/runtime/src/MVM_RuntimeCommon.c \
  $(MVM_ROOT)/runtime/src/MVM_RuntimeDispatch.c
