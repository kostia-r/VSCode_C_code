# Mophun VM component make fragment.
#
# Integration:
#   MVM_CONFIG_DIR := path/to/parent/config  # optional, before include
#   include path/to/MVM/vm.mk
#   C_SOURCES += $(MVM_SRC)
#   C_INCLUDES += $(addprefix -I,$(MVM_INC))

MVM_ROOT := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
MVM_BUNDLED_CONFIG_DIR := $(MVM_ROOT)/Config

# A parent project may override this before including vm.mk. Its MVM_Cfg.h and
# MVM_Lcfg.c then replace the bundled integration config without modifying MVM.
MVM_CONFIG_DIR ?= $(MVM_BUNDLED_CONFIG_DIR)
MVM_CONFIG_SOURCE ?= $(MVM_CONFIG_DIR)/MVM_Lcfg.c

MVM_PUBLIC_INC := \
  $(MVM_ROOT)/inc

MVM_INTERNAL_INC := \
  $(MVM_ROOT)/core/inc \
  $(MVM_ROOT)/debug/inc \
  $(MVM_ROOT)/runtime/inc

MVM_INC := \
  $(MVM_CONFIG_DIR) \
  $(MVM_PUBLIC_INC) \
  $(MVM_INTERNAL_INC) \
  $(filter-out $(MVM_CONFIG_DIR),$(MVM_BUNDLED_CONFIG_DIR))

MVM_LIBRARY_SRC := \
  $(MVM_ROOT)/core/src/MVM_Core.c \
  $(MVM_ROOT)/core/src/MVM_Log.c \
  $(MVM_ROOT)/debug/src/MVM_Trace.c \
  $(MVM_ROOT)/debug/src/MVM_VmgpDebug.c \
  $(MVM_ROOT)/loader/src/MVM_VmgpLoader.c \
  $(MVM_ROOT)/pip/src/MVM_PipExec.c \
  $(MVM_ROOT)/runtime/src/MVM_Imports.c \
  $(MVM_ROOT)/runtime/src/MVM_Render.c \
  $(MVM_ROOT)/runtime/src/MVM_RuntimeCommon.c \
  $(MVM_ROOT)/runtime/src/MVM_RuntimeDispatch.c

# Source-included integrations use the complete list. Static libraries archive
# MVM_LIBRARY_SRC only and link MVM_CONFIG_SOURCE in the parent executable.
MVM_SRC := \
  $(MVM_CONFIG_SOURCE) \
  $(MVM_LIBRARY_SRC)
