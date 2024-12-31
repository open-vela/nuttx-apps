/****************************************************************************
 * apps/testing/testsuites/kernel/telephony/telephony_ss_test.h
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
#ifndef TELEPHONY_SS_TEST_H_
#define TELEPHONY_SS_TEST_H_

#include "telephony_test.h"

int tapi_listen_ss_test(int slot_id);
int tapi_unlisten_ss_test(void);
int tapi_ss_request_call_barring_test(int slot_id);
int tapi_ss_set_call_barring_option_test(int slot_id, char *facility,
                                         char *pin2);
int tapi_ss_get_call_barring_option_test(int slot_id, char *key,
                                         char *expect);
int tapi_ss_change_call_barring_password_test(int slot_id,
                                              char *old_passwd,
                                              char *new_passwd);
int tapi_ss_disable_all_incoming_test(int slot_id, char *passwd);
int tapi_ss_disable_all_outgoing_test(int slot_id, char *passwd);
int tapi_ss_disable_all_call_barrings_test(int slot_id, char *passwd);
int tapi_ss_set_call_forwarding_option_test(int slot_id, int cf_type,
                                            char *number);
int tapi_ss_get_call_forwarding_option_test(int slot_id, int cf_type);
int tapi_ss_set_call_waiting_test(int slot_id, bool enable);
int tapi_ss_get_call_waiting_test(int slot_id, bool expect);
int tapi_ss_enable_fdn_test(int slot_id, bool enable, char *passwd);
int tapi_ss_query_fdn_test(int slot_id, bool expect);

#endif /* TELEPHONY_SS_TEST_H_ */
