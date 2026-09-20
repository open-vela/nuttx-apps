############################################################################
# apps/mimo_client/Makefile
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

# MiMo client library sources

CSRCS  = mimo_client.c
CSRCS += mimo_http.c
CSRCS += mimo_modality.c
CSRCS += mimo_text.c
CSRCS += mimo_vision.c
CSRCS += mimo_tts.c
CSRCS += mimo_asr.c

# Include paths

CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/mimo_client/include

include $(APPDIR)/Application.mk
