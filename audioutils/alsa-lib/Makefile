############################################################################
# apps/audioutils/alsa-lib/Makefile
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations
# under the License.
#
############################################################################

include $(APPDIR)/Make.defs

CSRCS += $(filter-out aplay.c arecord.c pcm_dmix.c, $(wildcard *.c))

ifneq ($(CONFIG_AUDIOUTILS_ALSA_LIB_DEVICE_DMIX),)
CSRCS += pcm_dmix.c
endif

CFLAGS += -DOUTSIDE_SPEEX -DRANDOM_PREFIX=nuttx

ifneq ($(CONFIG_AUDIOUTILS_ALSA_LIB_APLAY),)
MAINSRC   += aplay.c
PROGNAME  += aplay
PRIORITY  += SCHED_PRIORITY_DEFAULT
STACKSIZE += $(CONFIG_DEFAULT_TASK_STACKSIZE)
endif

ifneq ($(CONFIG_AUDIOUTILS_ALSA_LIB_ARECORD),)
MAINSRC   += arecord.c
PROGNAME  += arecord
PRIORITY  += SCHED_PRIORITY_DEFAULT
STACKSIZE += $(CONFIG_DEFAULT_TASK_STACKSIZE)
endif

ifneq ($(CONFIG_AUDIOUTILS_ALSA_LIB_ALSACTL),)
MAINSRC   += alsactl.c
PROGNAME  += alsactl
PRIORITY  += SCHED_PRIORITY_DEFAULT
STACKSIZE += $(CONFIG_DEFAULT_TASK_STACKSIZE)
endif

include $(APPDIR)/Application.mk
