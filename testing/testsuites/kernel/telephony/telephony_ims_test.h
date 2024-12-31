/****************************************************************************
 * apps/testing/testsuites/kernel/telephony/telephony_ims_test.h
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

#ifndef TELEPHONY_IMS_TEST_H_
#define TELEPHONY_IMS_TEST_H_

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "telephony_test.h"

int tapi_ims_listen_ims_test(int slot_id);
int tapi_ims_turn_on_test(int slot_id);
int tapi_ims_turn_off_test(int slot_id);
int tapi_ims_get_registration_test(int slot_id);
int tapi_ims_get_enabled_test(int slot_id);
int tapi_ims_set_service_status_test(int slot_id, int status);

#endif