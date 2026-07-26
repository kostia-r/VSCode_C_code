# OpenMophun library make fragment.
#
# Integration:
#   include path/to/OpenMophun/OpenMophun.mk
#   C_SOURCES += $(OPENMOPHUN_SRC)
#   C_INCLUDES += $(addprefix -I,$(OPENMOPHUN_INC))

OPENMOPHUN_ROOT := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

OPENMOPHUN_PUBLIC_INC := \
  $(OPENMOPHUN_ROOT)/inc

OPENMOPHUN_INTERNAL_INC := \
  $(OPENMOPHUN_ROOT)/src

OPENMOPHUN_INC := \
  $(OPENMOPHUN_PUBLIC_INC) \
  $(OPENMOPHUN_INTERNAL_INC)

OPENMOPHUN_SRC := \
  $(OPENMOPHUN_ROOT)/src/MVM_Core.c \
  $(OPENMOPHUN_ROOT)/src/MVM_Log.c \
  $(OPENMOPHUN_ROOT)/src/MVM_VmgpDebug.c \
  $(OPENMOPHUN_ROOT)/src/MVM_VmgpLoader.c \
  $(OPENMOPHUN_ROOT)/src/MVM_PipExec.c \
  $(OPENMOPHUN_ROOT)/src/MVM_Imports.c \
  $(OPENMOPHUN_ROOT)/src/MVM_Render.c \
  $(OPENMOPHUN_ROOT)/src/MVM_RuntimeCommon.c \
  $(OPENMOPHUN_ROOT)/src/MVM_RuntimeDispatch.c
