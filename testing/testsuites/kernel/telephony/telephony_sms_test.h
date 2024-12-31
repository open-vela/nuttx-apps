/****************************************************************************
 * apps/testing/testsuites/kernel/telephony/telephony_sms_test.h
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

#ifndef TELEPHONY_SMS_TEST_H_
#define TELEPHONY_SMS_TEST_H_

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "telephony_test.h"

int sms_send_message_test(tapi_context context, int slot_id,
                          char *number, char *text);
int sms_set_service_center_number_test(int slot_id);
int sms_check_service_center_number_test(int slot_id);
int sms_send_data_message_test(int slot_id, char *to, int port,
                               char *text);
int sms_send_message_in_dialing(int slot_id, char *to, char *text);
int sms_send_data_message_in_dialing(int slot_id, char *to, char *text,
                                     int port);

#endif /* TELEPHONY_SMS_TEST_H_ */
