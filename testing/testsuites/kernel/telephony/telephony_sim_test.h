/****************************************************************************
 * apps/testing/testsuites/kernel/telephony/telephony_sim_test.h
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
#ifndef TELEPHONY_SIM_TEST_H_
#define TELEPHONY_SIM_TEST_H_

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "telephony_test.h"
#define TEST_COUNT 10

int tapi_sim_has_icc_card_test(int slot_id);
int tapi_sim_multi_has_icc_card_test(int slot_id);
int tapi_sim_get_sim_operator_test(int slot_id, const char *expect_res);
int tapi_sim_multi_get_sim_operator(int slot_id, const char *expect_res);
int tapi_sim_get_sim_operator_name_test(int slot_id,
                                        const char *expect_res);
int tapi_sim_get_sim_operator_name_numerous(int slot_id,
                                            const char *expect_res);
int tapi_sim_get_sim_subscriber_id_test(int slot_id,
                                        const char *expect_res);
int tapi_sim_multi_get_sim_subscriber_id_test(int slot_id,
                                              const char *expect_res);
int tapi_sim_get_sim_iccid_test(int slot_id, const char *expect_res);
int tapi_sim_multi_get_sim_iccid_test(int slot_id,
                                      const char *expect_res);
int tapi_sim_get_ef_msisdn_test(int slot_id, const char *expect_res);
int tapi_sim_multi_get_ef_msisdn_test(int slot_id,
                                      const char *expect_res);
int tapi_sim_listen_sim_test(int slot_id, int event_id);
int tapi_sim_unlisten_sim_test(int slot_id, int watch_id);
int tapi_open_logical_channel_test(int slot_id);
int tapi_close_logical_channel_test(int slot_id);
int tapi_transmit_apdu_basic_channel_test(int slot_id);
int tapi_sim_get_state_test(int slot_id);
int tapi_sim_set_uicc_enablement_test(int slot_id);
int tapi_sim_get_uicc_enablement_test(int slot_id);
int tapi_sim_enter_pin_test(int slot_id);
int tapi_sim_change_pin_test(int slot_id, char *old_pin, char *new_pin);
int tapi_sim_lock_pin_test(int slot_id);
int tapi_sim_unlock_pin_test(int slot_id);
int tapi_phonebook_load_adn_entries_test(int slot_id);
int tapi_phonebook_load_fdn_entries_test(int slot_id);
int tapi_phonebook_insert_fdn_entry_test(int slot_id);
int tapi_phonebook_update_fdn_entry_test(int slot_id);
int tapi_phonebook_delete_fdn_entry_test(int slot_id);
int sim_open_close_logical_channel_numerous(int slot_id);
int tapi_transmit_apdu_logical_channel_test(int slot_id);
int sim_transmit_apdu_by_logical_channel(int slot_id);
int sim_change_pin_test(int slot_id);

#endif