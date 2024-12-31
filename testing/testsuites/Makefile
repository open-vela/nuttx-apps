# apps/tests/testsuites/Makefile
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

include $(APPDIR)/Make.defs


PRIORITY = $(CONFIG_TESTS_TESTSUITES_PRIORITY)
STACKSIZE = $(CONFIG_TESTS_TESTSUITES_STACKSIZE)
MODULE = $(CONFIG_TESTS_TESTSUITES)

# treat all warning as error
CFLAGS += -Werror

ifneq ($(CONFIG_CM_FS_TEST),)
CFLAGS += -I$(CURDIR)/kernel/fs/include
CFLAGS += -I$(APPDIR)/nshlib
CSRCS += $(wildcard kernel/fs/cases/*.c)
CSRCS += $(wildcard kernel/fs/common/*.c)
PROGNAME += cmocka_fs_test
MAINSRC += $(CURDIR)/kernel/fs/cmocka_fs_test.c
endif

ifneq ($(CONFIG_CM_SCHED_TEST),)
CFLAGS += -I$(CURDIR)/kernel/sched/include
CSRCS += $(wildcard kernel/sched/cases/*.c)
CSRCS += $(wildcard kernel/sched/common/*.c)
PROGNAME += cmocka_sched_test
MAINSRC += $(CURDIR)/kernel/sched/cmocka_sched_test.c
endif

ifneq ($(CONFIG_CM_SYSCALL_TEST),)
CFLAGS += -I$(CURDIR)/kernel/syscall/include
CFLAGS += -I$(APPDIR)/nshlib
CSRCS += $(wildcard kernel/syscall/cases/*.c)
CSRCS += $(wildcard kernel/syscall/common/*.c)
PROGNAME += cmocka_syscall_test
MAINSRC += $(CURDIR)/kernel/syscall/cmocka_syscall_test.c
endif

ifneq ($(CONFIG_CM_MM_TEST),)
CFLAGS += -I$(CURDIR)/kernel/mm/include
CSRCS += $(wildcard kernel/mm/cases/*.c)
CSRCS += $(wildcard kernel/mm/common/*.c)
PROGNAME += cmocka_mm_test
MAINSRC += $(CURDIR)/kernel/mm/cmocka_mm_test.c
PROGNAME += mem_batch_opt_perf_test
MAINSRC += $(CURDIR)/kernel/mm/tool/mem_batch_opt_perf_test.c
PROGNAME += mem_cycle_opt_perf_test
MAINSRC += $(CURDIR)/kernel/mm/tool/mem_cycle_opt_perf_test.c
endif

ifneq ($(CONFIG_CM_KVDB_TEST),)
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)/frameworks/kvdb
CFLAGS += -I$(CURDIR)/kernel/kv/include
CSRCS += $(wildcard kernel/kv/cases/*.c)
CSRCS += $(wildcard kernel/kv/common/*.c)
PROGNAME += cmocka_kv_test
MAINSRC += $(CURDIR)/kernel/kv/cmocka_kv_test.c
endif

ifneq ($(CONFIG_CM_TIME_TEST),)
CFLAGS += -I$(CURDIR)/kernel/time/include
CSRCS += $(wildcard kernel/time/cases/*.c)
PROGNAME += cmocka_time_test
MAINSRC += $(CURDIR)/kernel/time/cmocka_time_test.c
endif

ifneq ($(CONFIG_CM_SOCKET_TEST),)
CFLAGS += -I$(CURDIR)/kernel/socket/include
CSRCS += $(wildcard kernel/socket/cases/*.c)
PROGNAME += cmocka_socket_test
MAINSRC += $(CURDIR)/kernel/socket/cmocka_socket_test.c
endif

ifneq ($(CONFIG_CM_PTHREAD_TEST),)
CFLAGS += -I$(CURDIR)/kernel/pthread/include
CSRCS += $(wildcard kernel/pthread/cases/*.c)
CSRCS += $(wildcard kernel/pthread/common/*.c)
PROGNAME += cmocka_pthread_test
MAINSRC += $(CURDIR)/kernel/pthread/cmocka_pthread_test.c
endif

ifneq ($(CONFIG_CM_MUTEX_TEST),)
CFLAGS += -I$(CURDIR)/kernel/mutex/include
CSRCS += $(wildcard kernel/mutex/cases/*.c)
CSRCS += $(wildcard kernel/mutex/common/*.c)
PROGNAME += cmocka_mutex_test
MAINSRC += $(CURDIR)/kernel/mutex/cmocka_mutex_test.c
endif

ifneq ($(CONFIG_TEST_TELEPHONY),)
	CFLAGS += -I$(APPDIR)/frameworks/telephony/include
	CFLAGS += -I$(APPDIR)/testing/cmocka/cmocka/include
	CSRCS += $(CURDIR)/kernel/telephony/telephony_sim_test.c \
		 $(CURDIR)/kernel/telephony/telephony_call_test.c \
		 $(CURDIR)/kernel/telephony/telephony_data_test.c \
		 $(CURDIR)/kernel/telephony/telephony_sms_test.c \
		 $(CURDIR)/kernel/telephony/telephony_network_test.c \
		 $(CURDIR)/kernel/telephony/telephony_common_test.c \
		 $(CURDIR)/kernel/telephony/telephony_ims_test.c \
		 $(CURDIR)/kernel/telephony/telephony_ss_test.c
	PROGNAME += cmocka_telephony_test
	ifneq ($(CONFIG_GOLDFISH_RIL),)
	MAINSRC += $(CURDIR)/kernel/telephony/cmocka_telephony_test.c
	else
	MAINSRC += $(CURDIR)/kernel/telephony/product_telephony_test.c
	endif

depend::
	$(Q) touch $(MAINSRC)
endif

ifneq ($(CONFIG_CM_DFX_TEST),)
CFLAGS += -I$(CURDIR)/kernel/dfx/include
CFLAGS += -I$(APPDIR)/frameworks/dfx/include
CSRCS += $(wildcard kernel/dfx/cases/*.c)
PROGNAME += cmocka_dfx_test
MAINSRC += $(CURDIR)/kernel/dfx/cmocka_dfx_test.c
endif

include $(APPDIR)/Application.mk
