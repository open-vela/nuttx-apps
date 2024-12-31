/****************************************************************************
 * apps/testing/testsuites/kernel/dfx/cases/dfx_test_event.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "dfx_debug.h"
#include "dfx_event.h"
#include <netutils/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <cmocka.h>

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void dfx_test_send_event(FAR void **state)
{
  (void)state;
  int ret;
  const char *dns_error = "{\"ErrorDesc\":\"Cannot found server\"}";
  ret = sendeventmisight(916012001, dns_error);
  assert_int_equal(ret, DFX_OK);
}

void dfx_test_send_syslog(FAR void **state)
{
  (void)state;
  int ret;
  const char *action = "{\"logger_action\":\"syslog\"}";
  ret = sendeventmisight(901009001, action);
  assert_int_equal(ret, DFX_OK);
}
