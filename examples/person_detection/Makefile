############################################################################
# apps/examples/person_detection/Makefile
############################################################################

include $(APPDIR)/Make.defs

TFLM_DIR := $(APPDIR)/mlearning/tflite-micro/tflite-micro
UVC_DIR  := $(APPDIR)/examples/uvc_test

PROGNAME  = $(CONFIG_EXAMPLES_PERSON_DETECTION_PROGNAME)
PRIORITY  = $(CONFIG_EXAMPLES_PERSON_DETECTION_PRIORITY)
STACKSIZE = $(CONFIG_EXAMPLES_PERSON_DETECTION_STACKSIZE)
MODULE    = $(CONFIG_EXAMPLES_PERSON_DETECTION)

# Sources

CXXEXT  := .cc
MAINSRC = person_detection_main.cc
CSRCS   = $(UVC_DIR)/tjpgd.c

# TFLite Micro framework sources

CXXSRCS += $(wildcard $(TFLM_DIR)/tensorflow/lite/c/*.cc)
CXXSRCS += $(wildcard $(TFLM_DIR)/tensorflow/lite/core/c/*.cc)
CXXSRCS += $(wildcard $(TFLM_DIR)/tensorflow/lite/core/api/*.cc)
CXXSRCS += $(wildcard $(TFLM_DIR)/tensorflow/lite/kernels/*.cc)
CXXSRCS += $(wildcard $(TFLM_DIR)/tensorflow/lite/kernels/internal/*.cc)
CXXSRCS += $(wildcard $(TFLM_DIR)/tensorflow/lite/kernels/internal/optimized/*.cc)
CXXSRCS += $(wildcard $(TFLM_DIR)/tensorflow/lite/kernels/internal/reference/*.cc)
CXXSRCS += $(wildcard $(TFLM_DIR)/tensorflow/lite/micro/*.cc)
CXXSRCS += $(wildcard $(TFLM_DIR)/tensorflow/lite/micro/arena_allocator/*.cc)
CXXSRCS += $(wildcard $(TFLM_DIR)/tensorflow/lite/micro/kernels/*.cc)
CXXSRCS += $(wildcard $(TFLM_DIR)/tensorflow/lite/micro/memory_planner/*.cc)
CXXSRCS += $(wildcard $(TFLM_DIR)/tensorflow/lite/micro/tflite_bridge/*.cc)
CXXSRCS += $(wildcard $(TFLM_DIR)/tensorflow/lite/schema/*.cc)
CXXSRCS := $(filter-out %test.cc, $(CXXSRCS))

# Include paths

COMMON_FLAGS += ${INCDIR_PREFIX}$(TFLM_DIR)
COMMON_FLAGS += ${INCDIR_PREFIX}$(APPDIR)/math/gemmlowp/gemmlowp
COMMON_FLAGS += ${INCDIR_PREFIX}$(APPDIR)/math/kissfft/kissfft
COMMON_FLAGS += ${INCDIR_PREFIX}$(APPDIR)/math/ruy/ruy
COMMON_FLAGS += ${INCDIR_PREFIX}$(UVC_DIR)

# Compiler flags

COMMON_FLAGS += -fno-lto
COMMON_FLAGS += -fno-exceptions
COMMON_FLAGS += -Wno-shadow
COMMON_FLAGS += -Wno-sign-compare
COMMON_FLAGS += -Wno-undef
COMMON_FLAGS += -Wno-unused-variable
COMMON_FLAGS += -DTFLITE_EMULATE_FLOAT
COMMON_FLAGS += -DTF_LITE_DISABLE_X86_NEON

ifneq ($(CONFIG_TFLITEMICRO_DEBUG),)
COMMON_FLAGS += -DTF_LITE_SHOW_MEMORY_USE
COMMON_FLAGS += -DTF_LITE_USE_CTIME
else
COMMON_FLAGS += -DTF_LITE_STRIP_ERROR_STRINGS
endif

# CMSIS-NN support (BEFORE CFLAGS assignment!)

ifneq ($(CONFIG_MLEARNING_CMSIS_NN),)
COMMON_FLAGS += -DCMSIS_NN
COMMON_FLAGS += ${INCDIR_PREFIX}$(APPDIR)/mlearning/cmsis-nn/cmsis-nn
COMMON_FLAGS += ${INCDIR_PREFIX}$(APPDIR)/mlearning/cmsis-nn/cmsis-nn/Include

CMSIS_NN_SRCS := $(wildcard $(TFLM_DIR)/tensorflow/lite/micro/kernels/cmsis_nn/*.cc)
UNNEEDED_SRCS := $(addprefix $(TFLM_DIR)/tensorflow/lite/micro/kernels/, $(notdir $(CMSIS_NN_SRCS)))
CXXSRCS := $(filter-out $(UNNEEDED_SRCS), $(CXXSRCS))
CXXSRCS += $(CMSIS_NN_SRCS)
endif

# NOW assign all flags

CFLAGS   += ${COMMON_FLAGS}
CXXFLAGS += ${COMMON_FLAGS}

include $(APPDIR)/Application.mk
