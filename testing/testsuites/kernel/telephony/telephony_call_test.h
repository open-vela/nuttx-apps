/****************************************************************************
 * apps/testing/testsuites/kernel/telephony/telephony_call_test.h
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

#ifndef TELEPHONY_CALL_TEST_H_
#define TELEPHONY_CALL_TEST_H_

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "telephony_test.h"
#include <uv.h>

#define CALL_LOCAL_HANGUP 0x01
#define CALL_STATE_CHANGE_TO_ACTIVE 0x02
#define CALL_REMOTE_HANGUP 0x03
#define NEW_CALL_INCOMING 0x04
#define INCOMING_CALL_WITH_NETWORK_NAME 0x05
#define HANGUP_DUE_TO_NETWORK_EXCEPTION 0x06
#define CALL_STATE_CHANGE_TO_HOLD 0x07
#define GET_ALL_CALLS 0x08
#define GET_TWO_CALL_STATES 0x09
#define GET_HOLD_CALL_ID 0x10
#define GET_CURRENT_CALL_STATE 0x11
#define NEW_CALL_WAITING 0x12
#define NEW_CALL_DIALING 0x13
#define EVENT_REQUEST_START_DTMF_DONE 0x14
#define EVENT_REQUEST_STOP_DTMF_DONE 0x15

int tapi_call_listen_call_test(int slot_id);
int tapi_call_dial_test(int slot_id, char *phone_number,
                        int hide_caller_id);
int tapi_call_hanup_current_call_test(int slot_id);
int tapi_call_unlisten_call_test(void);
int tapi_call_dial_using_phone_number_with_area_code_test(int slot_id);
int tapi_call_dial_using_phone_number_with_pause_code_test(int slot_id);
int tapi_call_dial_using_phone_number_with_wait_code_test(int slot_id);
int tapi_call_dial_using_phone_number_with_numerous_code_test(
    int slot_id);
int tapi_dial_number(int slot_id);
int tapi_dial_ecc_number(int slot_id);
int tapi_dial_with_enable_hide_callerid(int slot_id);
int tapi_dial_with_disabled_hide_callerid(int slot_id);
int tapi_dial_with_default_hide_callerid(int slot_id);
int tapi_dial_with_long_phone_number(int slot_id);
int tapi_dial_with_short_phone_number(int slot_id);
int tapi_start_dtmf_test(int slot_id);
int tapi_stop_dtmf_test(int slot_id);
int tapi_call_load_ecc_list_test(int slot_id);
int tapi_call_get_call_test(int slot_id);
int tapi_call_answer_call_test(int slot_id, char *call_id);
int tapi_ss_listen_test(int slot_id);
int tapi_ss_unlisten_test(void);
int tapi_ss_listen_error_code_test(int slot_id);
void set_callback_data(int expect);
int judge_callback_result(void);
void incoming_first_call(void);
void incoming_another_call(void);
int call_hangup_between_dialing_and_answering(int slot_id);
int call_dial_with_area_code(int slot_id);
int call_dial_after_caller_reject(int slot_id);
int call_hangup_after_caller_answer(int slot_id);
int call_dial_active_and_hangup_by_caller(int slot_id);
int call_dial_caller_reject_and_incoming(int slot_id);
int call_dial_caller_reject_and_dial_another(int slot_id);
int call_incoming_answer_and_hangup_by_dialer(int slot_id);
int call_dial_to_phone_in_call(int slot_id);
int call_dial_to_phone_out_of_service(int slot_id);
int call_dial_without_sim_card(int slot_id);
int call_dial_ecc_number_without_sim_card(int slot_id);
int call_incoming_and_hangup_by_dialer_before_answer(int slot_id);
int call_incoming_and_hangup_by_dialer_before_answer_numerous(
    int slot_id);
int call_display_the_network_of_incoming_call(int slot_id,
                                              char *network_name);
int call_dial_active_hangup_due_to_dialer_network_exception(int slot_id);
int call_dial_active_hangup_due_to_caller_network_exception(int slot_id);
int call_incoming_hangup_first_answer_second(int slot_id);
int call_incoming_hold_and_recover_by_dialer(int slot_id);
int call_dialer_hold_recover_and_hold_by_caller(int slot_id);
int call_incoming_hold_and_recover_by_caller(int slot_id);
int call_swap_dial_reject_swap(int slot_id);
int call_release_and_answer(int slot_id);
int call_hold_and_hangup(int slot_id);
int call_hold_incoming_hangup_second_recover_first(int slot_id);
int call_hold_incoming_answer_hangup_second_recover_first(int slot_id);
int call_incoming_second_call_swap_answer_hangup_swap(int slot_id);
int call_hold_current_call_and_reject_new_incoming(int slot_id);
int call_incoming_and_hangup_new_call(int slot_id);
int call_hold_first_call_and_answer_second_call(int slot_id);
int call_dial_second_call_active_and_hangup_by_dialer(int slot_id);
int call_dial_second_call_and_hangup_by_caller_before_answering(
    int slot_id);
int call_dial_second_call_active_and_hangup_by_caller(int slot_id);
int call_hangup_hold_call_in_two_calls(int slot_id);
int call_dial_to_empty_number(int slot_id);
int call_incoming_answer_and_hangup(int slot_id);
int call_display_the_network_of_incoming_call_in_call_process(
    int slot_id, char *network_name);
int call_check_status_in_dialing(int slot_id);
int call_dial_and_check_status_in_call_process(int slot_id);
int call_check_status_in_incoming(int slot_id);
int call_incoming_answer_and_check_status_in_call_process(int slot_id);
int call_incoming_answer_caller_hold_and_check_status(int slot_id);
int call_incoming_answer_dialer_hold_and_check_status(int slot_id);
int call_check_call_status_in_multi_call(int slot_id);
int call_check_call_status_in_call_waiting_with_multi_call(int slot_id);
int call_check_call_status_in_dialing_with_multi_call(int slot);
int call_hold_old_call_incoming_new_call_and_check_status(int slot_id);
int call_swap_in_two_calling(int slot_id);
int call_hangup_current_call_and_recover_hold_call(int slot_id);
int call_hangup_all_call_in_two_calling(int slot_id);
int incoming_new_call_in_calling_and_hangup_all_call(int slot_id);
int call_dial_third_call(int slot_id);
int call_incoming_third_call(int slot_id);
int call_listen_and_unlisten_ss(int slot_id);
int call_listen_error_ss_code(int slot_id);
int tapi_call_hangup_all_test(int slot_id);
int tapi_get_call_count(int slot_id);
int tapi_call_set_default_voicecall_slot_test(int slot_id);
int tapi_call_get_default_voicecall_slot_test(int expect_res);
int call_clear_voicecall_slot_set(void);
#endif