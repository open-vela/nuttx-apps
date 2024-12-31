/****************************************************************************
 * apps/testing/testsuites/kernel/telephony/telephony_data_test.h
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

#ifndef TELEPHONY_DATA_TEST_H_
#define TELEPHONY_DATA_TEST_H_

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "telephony_test.h"

int tapi_data_listen_data_test(int slot_id);
int tapi_data_unlisten_data_test(void);
int tapi_data_enable_test(int state);
int tapi_data_load_apn_contexts_test(int slot_id);
int tapi_data_save_apn_context_test(char *slot_id, char *type,
                                    char *name, char *apn, char *proto,
                                    char *auth);
int tapi_data_edit_apn_context_test(char *slot_id, char *id, char *type,
                                    char *name, char *apn, char *proto,
                                    char *auth);
int tapi_data_remove_apn_context_test(char *slot_id, char *id);
int tapi_data_reset_apn_contexts_test(char *slot_id);
int tapi_data_request_network_test(int slot_id, char *target_state);
int tapi_data_release_network_test(int slot_id, char *target_state);
int tapi_data_enable_roaming_test(int);
bool tapi_data_get_roaming_enabled_test(bool *result);
int tapi_data_get_enabled_test(bool *result);
int tapi_data_is_ps_attached_test(int slot_id);
int tapi_data_get_network_type_test(int slot_id);
int tapi_data_set_preferred_apn_test(int slot_id, char *apn_id);
int tapi_data_get_preferred_apn_test(int slot_id);
int tapi_data_set_default_data_slot_test(int slot_id);
int tapi_data_get_default_data_slot_test(void);
int tapi_data_set_data_allow_test(int slot_id);
int tapi_data_send_screen_stat_test(int slot_id);
int tapi_data_get_data_call_list_test(int slot_id);
int data_enabled_test(int slot_id);
int data_disabled_test(int slot_id);
int data_release_network_test(int slot_id);
int data_request_network_test(int slot_id);
int data_request_ims(int slot_id);
int data_release_ims(int slot_id);
int data_get_call_list(int slot_id);
int data_enable_roaming_test(void);
int data_disable_roaming_test(void);

#endif /* TELEPHONY_DATA_TEST_H_ */
