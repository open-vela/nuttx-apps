/****************************************************************************
 * apps/testing/testsuites/kernel/kv/include/KvTest.h
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 *The ASF licenses this file to you under the Apache License, Version 2.0
 *(the "License"); you may not use this file except in compliance with
 *the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 *WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 *implied.  See the License for the specific language governing
 *permissions and limitations under the License.
 *
 ****************************************************************************/

#ifndef __SYSCALLTEST_H
#define __SYSCALLTEST_H

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <nuttx/config.h>
#include <setjmp.h>
#include <cmocka.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* The test files generated during the 'syscall-test' are stored in this
 * directory
 */

#define TEST_KEY_LENGTH PROP_NAME_MAX - 1
#define TEST_VALUE_LENGTH PROP_VALUE_MAX - 1
#define MOUNT_DIR CONFIG_TESTS_TESTSUITES_MOUNT_DIR
#define PTHREAD_STACK_SIZE CONFIG_DEFAULT_TASK_STACKSIZE
#define DEFAULT_STACKSIZE CONFIG_DEFAULT_TASK_STACKSIZE
#define TASK_PRIORITY SCHED_PRIORITY_DEFAULT
#define KV_TEST_PREFIX "KVtest."

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

char *cm_random_string(int length);
void cm_kv_get_a_random_key(char *buf, int size);

int test_nuttx_kv_setup(void **state);
int test_nuttx_kv_teardown(void **state);

/* test case function */

/* cases/kv_test_001.c
 * *****************************************************/

void test_nuttx_kv01(FAR void **state);

/* cases/kv_test_002.c
 * *****************************************************/

void test_nuttx_kv02(FAR void **state);

/* cases/kv_test_003.c
 * *****************************************************/

void test_nuttx_kv03(FAR void **state);

/* cases/kv_test_004.c
 * *****************************************************/

void test_nuttx_kv04(FAR void **state);

/* cases/kv_test_005.c
 * *****************************************************/

void test_nuttx_kv05(FAR void **state);

/* cases/kv_test_006.c
 * *****************************************************/

void test_nuttx_kv06(FAR void **state);

/* cases/kv_test_007.c
 * *****************************************************/

void test_nuttx_kv07(FAR void **state);

/* cases/kv_test_008.c
 * *****************************************************/

void test_nuttx_kv08(FAR void **state);

/* cases/kv_test_009.c
 * *****************************************************/

void test_nuttx_kv09(FAR void **state);

/* cases/kv_test_010.c
 * *****************************************************/

void test_nuttx_kv10(FAR void **state);

/* cases/kv_test_011.c
 * *****************************************************/

void test_nuttx_kv11(FAR void **state);

/* cases/kv_test_012.c
 * *****************************************************/

void test_nuttx_kv12(FAR void **state);

/* cases/kv_test_013.c
 * *****************************************************/

void test_nuttx_kv13(FAR void **state);

/* cases/kv_test_014.c
 * *****************************************************/

void test_nuttx_kv14(FAR void **state);

/* cases/kv_test_015.c
 * *****************************************************/

void test_nuttx_kv15(FAR void **state);

/* cases/kv_test_016.c
 * *****************************************************/

void test_nuttx_kv16(FAR void **state);

/* cases/kv_test_017.c
 * *****************************************************/

void test_nuttx_kv17(FAR void **state);

/* cases/kv_test_018.c
 * *****************************************************/

void test_nuttx_kv18(FAR void **state);

/* cases/kv_test_019.c
 * *****************************************************/

void test_nuttx_kv19(FAR void **state);

/* cases/kv_test_020.c
 * *****************************************************/

void test_nuttx_kv20(FAR void **state);

/* cases/kv_test_021.c
 * *****************************************************/

void test_nuttx_kv21(FAR void **state);

/* cases/kv_test_022.c
 * *****************************************************/

void test_nuttx_kv22(FAR void **state);

/* cases/kv_test_023.c
 * *****************************************************/

void test_nuttx_kv23(FAR void **state);

/* cases/kv_test_024.c
 * *****************************************************/

void test_nuttx_kv24(FAR void **state);

/* cases/kv_test_025.c
 * *****************************************************/

void test_nuttx_kv25(FAR void **state);

/* cases/kv_test_026.c
 * *****************************************************/

void test_nuttx_kv26(FAR void **state);

/* cases/kv_test_027.c
 * *****************************************************/

void test_nuttx_kv27(FAR void **state);

/* cases/kv_test_028.c
 * *****************************************************/

void test_nuttx_kv28(FAR void **state);

/* cases/kv_test_029.c
 * *****************************************************/

void test_nuttx_kv29(FAR void **state);

/* cases/kv_test_030.c
 * *****************************************************/

void test_nuttx_kv30(FAR void **state);
#endif