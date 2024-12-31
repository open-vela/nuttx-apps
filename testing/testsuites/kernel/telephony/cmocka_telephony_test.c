/****************************************************************************
 * apps/testing/testsuites/kernel/telephony/cmocka_telephony_test.c
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
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sched.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>

#include <cmocka.h>

#include "telephony_call_test.h"
#include "telephony_common_test.h"
#include "telephony_data_test.h"
#include "telephony_ims_test.h"
#include "telephony_network_test.h"
#include "telephony_sim_test.h"
#include "telephony_sms_test.h"
#include "telephony_ss_test.h"
#include "telephony_test.h"

#define REPEAT_TEST_MORE_FOR for (int _i = 0; _i < 10; _i++)
#define REPEAT_TEST_LESS_FOR for (int _i = 0; _i < 3; _i++)

/****************************************************************************
 * Public Functions
 ****************************************************************************/

char *phone_num = NULL;
static uv_async_t g_uv_exit;
static int ready_done;
tapi_context context = NULL;
static int count = 0;
typedef enum
{
  CASE_NORMAL_MODE = 0,
  CASE_AIRPLANE_MODE = 1,
  CASE_CALL_DIALING = 2,
  CASE_MODEM_POWEROFF = 3,
} case_type;

struct judge_type judge_data;

char *short_english_text = "test";
char *long_english_text =
    "testtesttesttesttesttesttesttesttesttesttesttest"
    "testtesttesttesttesttesttesttesttesttesttesttesttesttest"
    "testtesttesttesttesttesttesttesttesttesttesttesttesttesttest"
    "testtesttesttesttesttesttesttesttesttesttesttesttesttesttest"
    "testtesttesttesttesttesttesttesttesttesttesttesttesttesttest"
    "testtesttesttesttesttesttesttesttesttesttesttesttesttesttest"
    "testtesttesttesttesttesttesttesttesttesttesttesttesttesttest"
    "testtesttesttesttesttesttesttesttesttesttesttesttesttesttest"
    "testtesttest";
char *short_chinese_text = "测试";
char *long_chinese_text =
    "测试测试测试测试测试测试测试测试测试测试"
    "测试测试测试测试测试测试测试测试测试测试"
    "测试测试测试测试测试测试测试测试测试测试"
    "测试测试测试测试测试测试测试测试测试测试"
    "测试测试测试测试测试测试测试测试测试测试"
    "测试测试测试测试测试测试测试测试测试测试"
    "测试测试测试测试测试测试测试测试测试测试"
    "测试测试测试测试测试测试测试测试测试测试"
    "测试测试测试测试测试测试测试测试测试测试";

static void exit_async_cleanup(uv_async_t *handle)
{
  /* let's close the handle and stop the loop here as
   * we must be running in the same thread where the `uv_run` is
   * NOTE that uv_stop is not thread-safe!
   */

  uv_stop(uv_default_loop());
}

tapi_context get_tapi_ctx(void)
{
  return context;
}

int judge(void)
{
  int timeout = TIMEOUT;

  while (timeout-- > 0)
    {
      if (judge_data.flag == judge_data.expect)
        {
          if (judge_data.result != 0)
            syslog(LOG_ERR, "result error\n");
          else
            syslog(LOG_INFO, "result correct\n");

          return 0;
        }

      sleep(1);
      syslog(LOG_INFO, "There is %d second(s) remain.\n", timeout);
    }

  syslog(LOG_ERR, "judge timeout\n");
  assert(0);
  return -ETIME;
}

void judge_data_init(void)
{
  judge_data.flag = INVALID_VALUE;
  judge_data.expect = INVALID_VALUE;
  judge_data.result = INVALID_VALUE;
}

static void test_nuttx_has_icc_card(void **state)
{
  (void)state;
  int ret = tapi_sim_has_icc_card_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_has_icc_card_numerous_times(void **state)
{
  (void)state;
  int ret = tapi_sim_multi_has_icc_card_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_get_sim_operator(void **state)
{
  (void)state;
  int ret = tapi_sim_get_sim_operator_test(0, "310260");
  assert_int_equal(ret, OK);
}

static void test_nuttx_get_sim_operator_name(void **state)
{
  (void)state;
  int ret = tapi_sim_get_sim_operator_name_test(0, "T-Mobile");
  assert_int_equal(ret, OK);
}

static void test_nuttx_get_sim_operator_name_numeroustimes(void **state)
{
  (void)state;
  int ret = tapi_sim_get_sim_operator_name_numerous(0, "T-Mobile");
  assert_int_equal(ret, OK);
}

static void test_nuttx_get_sim_operator_numeroustimes(void **state)
{
  (void)state;
  int ret = tapi_sim_multi_get_sim_operator(0, "310260");
  assert_int_equal(ret, OK);
}

static void test_nuttx_get_sim_subscriberid(void **state)
{
  (void)state;
  int ret = tapi_sim_get_sim_subscriber_id_test(0, "310260000000000");
  assert_int_equal(ret, OK);
}

static void test_nuttx_get_sim_subscriberid_numerous_times(void **state)
{
  (void)state;
  int ret =
      tapi_sim_multi_get_sim_subscriber_id_test(0, "310260000000000");
  assert_int_equal(ret, OK);
}

static void test_nuttx_get_iccid(void **state)
{
  (void)state;
  int ret = tapi_sim_get_sim_iccid_test(0, "89860318640220133897");
  assert_int_equal(ret, OK);
}

static void test_nuttx_get_iccid_numeroustimes(void **state)
{
  (void)state;
  int ret = tapi_sim_multi_get_sim_iccid_test(0, "89860318640220133897");
  assert_int_equal(ret, OK);
}

static void test_nuttx_get_msisdn(void **state)
{
  (void)state;
  int ret = tapi_sim_get_ef_msisdn_test(0, "+15551234567");
  assert_int_equal(ret, OK);
}

static void test_nuttx_get_msisdn_numeroustimes(void **state)
{
  (void)state;
  int ret = tapi_sim_multi_get_ef_msisdn_test(0, "+15551234567");
  assert_int_equal(ret, OK);
}

static void test_nuttx_transmit_apdu_inbasic_channel(void **state)
{
  (void)state;
  int ret = tapi_transmit_apdu_basic_channel_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_get_sim_state(void **state)
{
  (void)state;
  int ret = tapi_sim_get_state_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_open_logical_channel(void **state)
{
  (void)state;
  int ret = tapi_open_logical_channel_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_close_logical_channel(void **state)
{
  (void)state;
  int ret = tapi_close_logical_channel_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_logical_channel_open_close_numerous(void **state)
{
  (void)state;
  int ret = sim_open_close_logical_channel_numerous(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_transmit_apduin_logical_channel(void **state)
{
  int ret = sim_transmit_apdu_by_logical_channel(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_set_uiccenablement(void **state)
{
  (void)state;
  int ret = tapi_sim_set_uicc_enablement_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_get_uiccenablement(void **state)
{
  (void)state;
  int ret = tapi_sim_get_uicc_enablement_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_transmit_apdu_basic_channel(void **state)
{
  (void)state;
  int ret = tapi_transmit_apdu_basic_channel_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_enter_pin(void **state)
{
  (void)state;
  int ret = tapi_sim_enter_pin_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_change_pin(void **state)
{
  (void)state;
  int ret = sim_change_pin_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_lockpin(void **state)
{
  (void)state;
  int ret = tapi_sim_lock_pin_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_unlockpin(void **state)
{
  (void)state;
  int ret = tapi_sim_unlock_pin_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_load_adnentries(void **state)
{
  (void)state;
  int ret = tapi_phonebook_load_adn_entries_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_load_fdnentries(void **state)
{
  (void)state;
  int ret = tapi_phonebook_load_fdn_entries_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_insert_fdnentry(void **state)
{
  (void)state;
  int ret = tapi_phonebook_insert_fdn_entry_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_update_fdnentry(void **state)
{
  (void)state;
  int ret = tapi_phonebook_update_fdn_entry_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_delete_fdnentry(void **state)
{
  (void)state;
  int ret = tapi_phonebook_delete_fdn_entry_test(0);
  assert_int_equal(ret, OK);
}

/* static void cmocka_tapi_sim_listen_sim_state_change(void **state)
 * {
 *    int ret = tapi_sim_listen_sim_test(0, 23);
 *    assert_int_equal(ret, OK);
 * }

 * static void cmocka_tapi_sim_listen_sim_uicc_app_enabled_change(void
 * **state)
 * {
 *     int ret = tapi_sim_listen_sim_test(0, 24);
 *     assert_int_equal(ret, OK);
 * }

 * static void test_nuttx_load_ecclist(void** state)
 * {
 *     (void)state;
 *     int ret = tapi_call_load_ecc_list_test(0);
 *     assert_int_equal(ret, 0);
 * }
 */

static void test_nuttx_set_voice_call_slot(void **state)
{
  (void)state;
  int ret = tapi_call_set_default_voicecall_slot_test(0);
  assert_int_equal(ret, 0);
}

static void test_nuttx_get_voice_call_slot(void **state)
{
  (void)state;
  int ret = tapi_call_get_default_voicecall_slot_test(0);
  assert_int_equal(ret, 0);
}

static void test_nuttx_clear_voice_call_slotset(void **state)
{
  (void)state;
  int ret = call_clear_voicecall_slot_set();
  assert_int_equal(ret, 0);
}

static void test_nuttx_listen_call(void **state)
{
  (void)state;
  int ret = tapi_call_listen_call_test(0);
  assert_int_equal(ret, 0);
}

static void test_nuttx_unlisten_call(void **state)
{
  (void)state;
  int ret = tapi_call_unlisten_call_test();
  assert_int_equal(ret, 0);
}

static void test_nuttx_start_dtmf(void **state)
{
  (void)state;
  int ret = tapi_start_dtmf_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_stop_dtmf(void **state)
{
  (void)state;
  int ret = tapi_stop_dtmf_test(0);
  assert_int_equal(ret, OK);
}

/* static void test_nuttx_hangup_betweendialingandanswering(void** state)
 * {
 *     (void)state;
 *     sleep(2);
 *     int ret = call_hangup_between_dialing_and_answering(0);
 *     assert_int_equal(ret, 0);
 * }

 * static void test_nuttx_answerandhanguptheincomingcall(void ** state) {
 *     int ret = call_incoming_answer_and_hangup(0);
 *     assert_int_equal(ret, 0);
 * }

 * static void test_nuttx_dial_againafterremotehangup(void **state) {
 *     int ret = call_dial_after_caller_reject(0);
 *     assert_int_equal(ret, 0);
 * }

 * static void test_nuttx_hangup_callafterremoteanswer(void **state) {
 *     int ret = call_hangup_after_caller_answer(0);
 *     assert_int_equal(ret, 0);
 * }

 * static void test_nuttx_dial_thenremoteanswerandhangup(void **state) {
 *     int ret = call_dial_active_and_hangup_by_caller(0);
 *     assert_int_equal(ret, 0);
 * }

 * static void test_nuttx_remotehangupthenincomingnewcall(void **state) {
 *     int ret = call_dial_caller_reject_and_incoming(0);
 *     assert_int_equal(ret, 0);
 * }

 * static void test_nuttx_remotehangupthendialanother(void **state) {
 *     int ret = call_dial_caller_reject_and_dial_another(0);
 *     assert_int_equal(ret, 0);
 * }
 */

static void test_nuttx_hangup_currentcall(void **state)
{
  (void)state;
  int ret = tapi_call_hanup_current_call_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_get_callcount(void **state)
{
  (void)state;
  int ret = tapi_get_call_count(0);
  assert_true(ret >= 0);
}

static void test_nuttx_dial_number(void **state)
{
  (void)state;
  int ret = tapi_dial_number(0);
  assert_int_equal(ret, OK);
  sleep(2);
}

static void test_nuttx_dial_eccnumber(void **state)
{
  (void)state;
  int ret = tapi_dial_ecc_number(0);
  assert_int_equal(ret, OK);
  sleep(2);
  ret = tapi_call_hanup_current_call_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_dial_long_phonenumber(void **state)
{
  (void)state;
  int ret = tapi_dial_with_long_phone_number(0);
  assert_int_equal(ret, OK);
  sleep(2);
  ret = tapi_call_hanup_current_call_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_dial_shot_phonenumber(void **state)
{
  (void)state;
  int ret = tapi_dial_with_short_phone_number(0);
  assert_int_equal(ret, OK);
  sleep(2);
  ret = tapi_call_hanup_current_call_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_dial_with_enable_hide_callid(void **state)
{
  (void)state;
  int ret = tapi_dial_with_enable_hide_callerid(0);
  assert_int_equal(ret, OK);
  sleep(2);
  ret = tapi_call_hanup_current_call_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_dial_with_disabled_hide_callid(void **state)
{
  (void)state;
  int ret = tapi_dial_with_disabled_hide_callerid(0);
  assert_int_equal(ret, OK);
  sleep(2);
  ret = tapi_call_hanup_current_call_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_dial_with_default_hide_callid(void **state)
{
  (void)state;
  int ret = tapi_dial_with_default_hide_callerid(0);
  assert_int_equal(ret, OK);
  sleep(2);
  ret = tapi_call_hanup_current_call_test(0);
  assert_int_equal(ret, OK);
}

/* static void test_nuttx_incomingnewcallthenremotehangup(void **state) {
 *     int ret = call_incoming_and_hangup_by_dialer_before_answer(0);
 *     assert_int_equal(ret, 0);
 * }
 */

static void test_nuttx_dial_with_areacode(void **state)
{
  (void)state;
  int ret = tapi_call_dial_using_phone_number_with_area_code_test(0);
  assert_int_equal(ret, OK);
  sleep(2);
  ret = tapi_call_hanup_current_call_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_dial_with_pausecode(void **state)
{
  (void)state;
  int ret = tapi_call_dial_using_phone_number_with_pause_code_test(0);
  assert_int_equal(ret, OK);
  sleep(2);
  ret = tapi_call_hanup_current_call_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_dial_with_waitcode(void **state)
{
  (void)state;
  int ret = tapi_call_dial_using_phone_number_with_wait_code_test(0);
  assert_int_equal(ret, OK);
  sleep(2);
  ret = tapi_call_hanup_current_call_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_dial_with_numerouscode(void **state)
{
  (void)state;
  int ret = tapi_call_dial_using_phone_number_with_numerous_code_test(0);
  assert_int_equal(ret, OK);
  sleep(2);
  ret = tapi_call_hanup_current_call_test(0);
  assert_int_equal(ret, OK);
}

/* static void test_nuttx_checkstatusin_dialing(void **state)
 * {
 *     int ret = call_check_status_in_dialing(0);
 *     assert_int_equal(ret, 0);
 * }
 */

/* data testcases */

static void test_nuttx_data_load_apn_contexts(void **state)
{
  (void)state;
  int ret = tapi_data_load_apn_contexts_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_save_apn_context_supl(void **state)
{
  (void)state;
  int ret = tapi_data_save_apn_context_test("0", "3", "supl", "supl",
                                            "2", "2");
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_save_apn_context_emergency(void **state)
{
  (void)state;
  int ret = tapi_data_save_apn_context_test("0", "7", "emergency",
                                            "emergency", "2", "2");
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_save_long_apn_contex(void **state)
{
  (void)state;
  int ret = tapi_data_save_apn_context_test(
      "0", "1",
      "longname-----------------------------------------"
      "-----------------------------------------longname",
      "cmnet4", "2", "2");
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_save_apn_context(void **state)
{
  (void)state;
  int ret = tapi_data_save_apn_context_test("0", "1", "cmcc1", "cmnet1",
                                            "2", "2");
  assert_int_equal(ret, 0);
}

static void test_nuttx_data_remove_apn_context(void **state)
{
  (void)state;
  int ret = tapi_data_remove_apn_context_test("0", "/ril_0/context3");
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_reset_apn_contexts(void **state)
{
  (void)state;
  int ret = tapi_data_reset_apn_contexts_test("0");
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_reset_apn_contexts_repeatedly(void **state)
{
  (void)state;
  REPEAT_TEST_MORE_FOR
    {
      int ret = tapi_data_reset_apn_contexts_test("0");
      assert_int_equal(ret, OK);
    }
}

static void test_nuttx_data_edit_apn_name(void **state)
{
  (void)state;
  int ret;
  ret = tapi_data_save_apn_context_test("0", "1", "cmcc1", "cmnet1", "2",
                                        "2");
  assert_int_equal(ret, OK);
  ret = tapi_data_edit_apn_context_test("0", "/ril_0/context3", "1",
                                        "cmname", "cmname", "2", "2");
  assert_int_equal(ret, OK);
  ret = tapi_data_load_apn_contexts_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_edit_apn_type(void **state)
{
  (void)state;
  int ret = tapi_data_edit_apn_context_test(
      "0", "/ril_0/context3", "3", "cmname", "cmname", "2", "2");
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_edit_apn_proto(void **state)
{
  (void)state;
  int ret = tapi_data_edit_apn_context_test(
      "0", "/ril_0/context3", "3", "cmname", "cmname", "0", "2");
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_edit_apn_auth(void **state)
{
  (void)state;
  int ret = tapi_data_edit_apn_context_test(
      "0", "/ril_0/context3", "3", "cmname", "cmname", "0", "0");
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_edit_apn_all(void **state)
{
  (void)state;
  int ret;
  ret = tapi_data_edit_apn_context_test(
      "0", "/ril_0/context3", "2", "cmnameall", "cmnameall", "1", "1");
  assert_int_equal(ret, OK);
  ret = tapi_data_load_apn_contexts_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_edit_apn_and_remove(void **state)
{
  (void)state;
  int ret;
  ret = tapi_data_edit_apn_context_test(
      "0", "/ril_0/context3", "2", "cmnameall", "cmnameall", "2", "2");
  assert_int_equal(ret, OK);
  ret = tapi_data_remove_apn_context_test("0", "/ril_0/context3");
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_edit_apn_and_reset(void **state)
{
  (void)state;
  int ret;
  ret = tapi_data_save_apn_context_test("0", "1", "cmcc1", "cmnet1", "2",
                                        "2");
  assert_int_equal(ret, OK);
  ret = tapi_data_edit_apn_context_test("0", "/ril_0/context3", "1",
                                        "cmname", "cmname", "2", "2");
  assert_int_equal(ret, OK);
  ret = tapi_data_reset_apn_contexts_test("0");
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_edit_apn_repeatedly_and_load(void **state)
{
  (void)state;
  int ret;
  ret = tapi_data_save_apn_context_test("0", "1", "cmcc1", "cmnet1", "2",
                                        "2");
  assert_int_equal(ret, OK);
  ret = tapi_data_edit_apn_context_test("0", "/ril_0/context1", "1",
                                        "cmname11", "cmname", "2", "2");
  assert_int_equal(ret, OK);
  ret = tapi_data_edit_apn_context_test("0", "/ril_0/context1", "1",
                                        "cmname22", "cmname", "2", "2");
  assert_int_equal(ret, OK);
  ret = tapi_data_edit_apn_context_test("0", "/ril_0/context1", "1",
                                        "cmname33", "cmname", "2", "2");
  assert_int_equal(ret, OK);
  ret = tapi_data_load_apn_contexts_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_enable(void **state)
{
  (void)state;
  int ret = data_enabled_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_disable(void **state)
{
  (void)state;
  int ret = data_disabled_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_enable_repeatedly(void **state)
{
  (void)state;
  REPEAT_TEST_MORE_FOR
    {
      test_nuttx_data_enable(state);
      test_nuttx_data_disable(state);
    }
}

static void test_nuttx_data_isenable(void **state)
{
  (void)state;
  bool enable = false;
  int ret = tapi_data_get_enabled_test(&enable);
  assert_int_equal(ret, OK);
  assert_int_equal(enable, 1);
}

static void test_nuttx_data_isdisable(void **state)
{
  (void)state;
  bool enable = true;
  int ret = tapi_data_get_enabled_test(&enable);
  assert_int_equal(ret, OK);
  assert_int_equal(enable, 0);
}

static void test_nuttx_data_register(void **state)
{
  (void)state;
  int ret = tapi_data_listen_data_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_unregister(void **state)
{
  (void)state;
  int ret = tapi_data_unlisten_data_test();
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_request_network_internet(void **state)
{
  (void)state;
  int ret = data_request_network_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_release_network_internet(void **state)
{
  (void)state;
  int ret = data_release_network_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_release_network_internet_repeatedly(void **state)
{
  (void)state;
  REPEAT_TEST_MORE_FOR
    {
      test_nuttx_data_release_network_internet(state);
      test_nuttx_data_request_network_internet(state);
    }
}

static void test_nuttx_data_request_network_ims(void **state)
{
  (void)state;
  int ret = data_request_ims(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_release_network_ims(void **state)
{
  (void)state;
  int ret = data_release_ims(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_release_network_ims_repeatedly(void **state)
{
  (void)state;
  REPEAT_TEST_MORE_FOR
    {
      test_nuttx_data_request_network_ims(state);
      test_nuttx_data_release_network_ims(state);
    }
}

static void test_nuttx_data_set_preferred_apn(void **state)
{
  (void)state;
  int ret = tapi_data_set_preferred_apn_test(0, "/ril_0/context1");
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_get_preferred_apn(void **state)
{
  (void)state;
  int ret = tapi_data_get_preferred_apn_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_send_screen_state(void **state)
{
  (void)state;
  int ret = tapi_data_send_screen_stat_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_ispsattached(void **state)
{
  (void)state;
  int ret = tapi_data_is_ps_attached_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_get_network_type(void **state)
{
  (void)state;
  int ret = tapi_data_get_network_type_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_set_default_data_slot(void **state)
{
  (void)state;
  int ret = tapi_data_set_default_data_slot_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_get_default_data_slot(void **state)
{
  (void)state;
  sleep(2);
  int ret = tapi_data_get_default_data_slot_test();
  assert_int_equal(ret, OK);
}

static void test_nuttx_data_set_data_allow(void **state)
{
  (void)state;
  int ret = tapi_data_set_data_allow_test(0);
  assert_true(ret == OK);
}

static void test_nuttx_data_get_call_list(void **state)
{
  (void)state;
  int ret = data_get_call_list(0);
  assert_true(ret == OK);
}

static void test_nuttx_enable_data_roaming(void **state)
{
  (void)state;
  int ret = data_enable_roaming_test();
  assert_true(ret == OK);
}

static void test_nuttx_disable_data_roaming(void **state)
{
  (void)state;
  int ret = data_disable_roaming_test();
  assert_true(ret == OK);
}

static void test_nuttx_sms_set_service_center_num(void **state)
{
  (void)state;
  int ret = sms_set_service_center_number_test(0);
  assert_int_equal(ret, 0);
}

static void test_nuttx_sms_get_service_center_num(void **state)
{
  (void)state;
  sleep(5);
  int ret = sms_check_service_center_number_test(0);
  assert_int_equal(ret, 0);
}

static void test_nuttx_sms_send_short_message_in_english(void **state)
{
  (void)state;
  int ret = sms_send_message_test(get_tapi_ctx(), 0, phone_num,
                                  short_english_text);
  assert_int_equal(ret, 0);
}

static void test_nuttx_sms_send_short_message_in_chinese(void **state)
{
  (void)state;
  int ret = sms_send_message_test(get_tapi_ctx(), 0, phone_num,
                                  short_chinese_text);
  assert_int_equal(ret, 0);
}

static void test_nuttx_sms_send_short_data_message_in_english(void **state)
{
  (void)state;
  int ret =
      sms_send_data_message_test(0, phone_num, 0, short_english_text);
  assert_int_equal(ret, 0);
}

static void test_nuttx_sms_send_short_data_message_in_chinese(void **state)
{
  (void)state;
  int ret =
      sms_send_data_message_test(0, phone_num, 0, short_chinese_text);
  assert_int_equal(ret, 0);
}

static void test_nuttx_sms_send_long_message_in_english(void **state)
{
  (void)state;
  int ret = sms_send_message_test(get_tapi_ctx(), 0, phone_num,
                                  long_english_text);
  assert_int_equal(ret, 0);
}

static void test_nuttx_sms_send_long_message_in_chinese(void **state)
{
  (void)state;
  int ret = sms_send_message_test(get_tapi_ctx(), 0, phone_num,
                                  long_chinese_text);
  assert_int_equal(ret, 0);
}

static void test_nuttx_sms_send_long_data_message_in_english(void **state)
{
  (void)state;
  int ret =
      sms_send_data_message_test(0, phone_num, 0, long_english_text);
  assert_int_equal(ret, 0);
}

static void test_nuttx_sms_send_long_data_message_in_chinese(void **state)
{
  (void)state;
  int ret =
      sms_send_data_message_test(0, phone_num, 0, long_chinese_text);
  assert_int_equal(ret, 0);
}

static void
test_nuttx_sms_send_short_english_message_in_dialing
(void **state)
{
  (void)state;
  int ret =
      sms_send_message_in_dialing(0, phone_num, short_english_text);
  assert_int_equal(ret, 0);
}

static void
test_nuttx_sms_send_short_chinese_message_in_dialing
(void **state)
{
  (void)state;
  int ret =
      sms_send_message_in_dialing(0, phone_num, short_chinese_text);
  assert_int_equal(ret, 0);
}

static void test_nuttx_sms_send_long_english_message_in_dialing(void **state)
{
  (void)state;
  int ret = sms_send_message_in_dialing(0, phone_num, long_english_text);
  assert_int_equal(ret, 0);
}

static void test_nuttx_sms_send_long_chinese_message_in_dialing(void **state)
{
  (void)state;
  int ret = sms_send_message_in_dialing(0, phone_num, long_chinese_text);
  assert_int_equal(ret, 0);
}

static void
test_nuttx_sms_send_short_english_data_message_in_dialing(void **state)
{
  (void)state;
  int ret = sms_send_data_message_in_dialing(0, phone_num,
                                             short_english_text, 0);
  assert_int_equal(ret, 0);
}

static void
test_nuttx_sms_send_short_chinese_data_message_in_dialing(void **state)
{
  (void)state;
  int ret = sms_send_data_message_in_dialing(0, phone_num,
                                             short_chinese_text, 0);
  assert_int_equal(ret, 0);
}

static void
test_nuttx_sms_send_long_english_data_message_in_dialing
(void **state)
{
  (void)state;
  int ret = sms_send_data_message_in_dialing(0, phone_num,
                                             long_english_text, 0);
  assert_int_equal(ret, 0);
}

static void
test_nuttx_sms_send_long_chinese_data_message_in_dialing
(void **state)
{
  (void)state;
  int ret = sms_send_data_message_in_dialing(0, phone_num,
                                             long_chinese_text, 0);
  assert_int_equal(ret, 0);
}

static void test_nuttx_send_english_message_in_voice_imscap(void **state)
{
  (void)state;
  int ret = sms_send_message_test(get_tapi_ctx(), 0, phone_num,
                                  short_english_text);
  assert_int_equal(ret, 0);
}

static void test_nuttx_send_chinese_message_in_voice_imscap(void **state)
{
  (void)state;
  int ret = sms_send_message_test(get_tapi_ctx(), 0, phone_num,
                                  short_chinese_text);
  assert_int_equal(ret, 0);
}

static void
test_nuttx_send_long_english_message_in_voice_imscap
(void **state)
{
  (void)state;
  int ret = sms_send_message_test(get_tapi_ctx(), 0, phone_num,
                                  long_english_text);
  assert_int_equal(ret, 0);
}

static void
test_nuttx_send_long_chinese_message_in_voice_imscap
(void **state)
{
  (void)state;
  int ret = sms_send_message_test(get_tapi_ctx(), 0, phone_num,
                                  long_chinese_text);
  assert_int_equal(ret, 0);
}

static void
test_nuttx_send_english_data_message_in_voice_imscap
(void **state)
{
  (void)state;
  int ret =
      sms_send_data_message_test(0, phone_num, 0, short_english_text);
  assert_int_equal(ret, 0);
}

static void
test_nuttx_send_chinese_data_message_in_voice_imscap
(void **state)
{
  (void)state;
  int ret =
      sms_send_data_message_test(0, phone_num, 0, short_chinese_text);
  assert_int_equal(ret, 0);
}

static void
test_nuttx_send_long_english_data_message_in_voice_imscap(void **state)
{
  (void)state;
  int ret =
      sms_send_data_message_test(0, phone_num, 0, long_english_text);
  assert_int_equal(ret, 0);
}

static void
test_nuttx_send_long_chinese_data_message_in_voice_imscap(void **state)
{
  (void)state;
  int ret =
      sms_send_data_message_test(0, phone_num, 0, long_chinese_text);
  assert_int_equal(ret, 0);
}

static void test_nuttx_send_english_message_in_smsimscap(void **state)
{
  (void)state;
  int ret = sms_send_message_test(get_tapi_ctx(), 0, phone_num,
                                  short_english_text);
  assert_int_equal(ret, 0);
}

static void test_nuttx_send_chinese_message_in_smsimscap(void **state)
{
  (void)state;
  int ret = sms_send_message_test(get_tapi_ctx(), 0, phone_num,
                                  short_chinese_text);
  assert_int_equal(ret, 0);
}

static void test_nuttx_send_long_english_message_in_smsimscap(void **state)
{
  (void)state;
  int ret = sms_send_message_test(get_tapi_ctx(), 0, phone_num,
                                  long_english_text);
  assert_int_equal(ret, 0);
}

static void test_nuttx_send_long_chinese_message_in_smsimscap(void **state)
{
  (void)state;
  int ret = sms_send_message_test(get_tapi_ctx(), 0, phone_num,
                                  long_chinese_text);
  assert_int_equal(ret, 0);
}

static void test_nuttx_send_english_data_message_in_smsimscap(void **state)
{
  (void)state;
  int ret =
      sms_send_data_message_test(0, phone_num, 0, short_english_text);
  assert_int_equal(ret, 0);
}

static void test_nuttx_send_chinese_data_message_in_smsimscap(void **state)
{
  (void)state;
  int ret =
      sms_send_data_message_test(0, phone_num, 0, short_chinese_text);
  assert_int_equal(ret, 0);
}

static void
test_nuttx_send_long_english_data_message_in_smsimscap
(void **state)
{
  (void)state;
  int ret =
      sms_send_data_message_test(0, phone_num, 0, long_english_text);
  assert_int_equal(ret, 0);
}

static void
test_nuttx_send_long_chinese_data_message_in_smsimscap
(void **state)
{
  (void)state;
  int ret =
      sms_send_data_message_test(0, phone_num, 0, long_chinese_text);
  assert_int_equal(ret, 0);
}

static void test_nuttx_send_english_message_insmsvoicecap(void **state)
{
  (void)state;
  int ret =
      sms_send_data_message_test(0, phone_num, 0, short_english_text);
  assert_int_equal(ret, 0);
}

static void test_nuttx_send_chinese_message_insmsvoicecap(void **state)
{
  (void)state;
  int ret =
      sms_send_data_message_test(0, phone_num, 0, short_chinese_text);
  assert_int_equal(ret, 0);
}

static void test_nuttx_send_long_english_message_insmsvoicecap(void **state)
{
  (void)state;
  int ret = sms_send_message_test(get_tapi_ctx(), 0, phone_num,
                                  long_english_text);
  assert_int_equal(ret, 0);
}

static void test_nuttx_send_long_chinese_message_insmsvoicecap(void **state)
{
  (void)state;
  int ret = sms_send_message_test(get_tapi_ctx(), 0, phone_num,
                                  long_chinese_text);
  assert_int_equal(ret, 0);
}

static void test_nuttx_send_english_data_message_in_smsvoicecap(void **state)
{
  (void)state;
  int ret =
      sms_send_data_message_test(0, phone_num, 0, short_english_text);
  assert_int_equal(ret, 0);
}

static void test_nuttx_send_chinese_data_message_in_smsvoicecap(void **state)
{
  (void)state;
  int ret =
      sms_send_data_message_test(0, phone_num, 0, short_chinese_text);
  assert_int_equal(ret, 0);
}

static void
test_nuttx_send_long_english_data_message_in_smsvoicecap(void **state)
{
  (void)state;
  int ret =
      sms_send_data_message_test(0, phone_num, 0, long_english_text);
  assert_int_equal(ret, 0);
}

static void
test_nuttx_send_long_chinese_data_message_in_smsvoicecap(void **state)
{
  (void)state;
  int ret =
      sms_send_data_message_test(0, phone_num, 0, long_chinese_text);
  assert_int_equal(ret, 0);
}

static void test_nuttx_set_sms_default_slot(void **state)
{
  (void)state;
  int ret = tapi_sms_set_default_slot(get_tapi_ctx(), 0);
  sleep(5);
  assert_int_equal(ret, 0);
}

static void test_nuttx_get_sms_default_slot(void **state)
{
  (void)state;
  int result = -1;
  int ret = tapi_sms_get_default_slot(context, &result);
  syslog(LOG_INFO, "%s, ret: %d, result: %d", __func__, ret, result);
  assert_int_equal(ret, 0);
  assert_int_equal(result, 0);
}

static void test_nuttx_sms_set_cell_broad_cast_power(void **state)
{
  (void)state;
  int ret = tapi_sms_set_cell_broadcast_power_on(get_tapi_ctx(), 0, 1);
  sleep(5);
  assert_int_equal(ret, 0);
}

static void test_nuttx_sms_get_cell_broad_cast_power(void **state)
{
  (void)state;
  bool result = false;
  int ret =
      tapi_sms_get_cell_broadcast_power_on(get_tapi_ctx(), 0, &result);
  syslog(LOG_INFO, "%s, ret: %d, result: %d", __func__, 0, (int)result);
  assert_int_equal(ret, 0);
  assert_int_equal(result, 1);
}

static void test_nuttx_sms_set_cell_broad_cast_topics(void **state)
{
  (void)state;
  int ret = tapi_sms_set_cell_broadcast_topics(get_tapi_ctx(), 0, "1");
  sleep(5);
  assert_int_equal(ret, 0);
}

static void test_nuttx_sms_get_cell_broad_cast_topics(void **state)
{
  (void)state;
  char *result = NULL;
  int ret =
      tapi_sms_get_cell_broadcast_topics(get_tapi_ctx(), 0, &result);
  assert_int_equal(ret, 0);
  assert_int_equal(strcmp(result, "1"), 0);
}

static void test_nuttx_net_select_auto(void **state)
{
  (void)state;
  int ret = tapi_net_select_auto_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_net_select_manual(void **state)
{
  (void)state;
  sleep(4);
  int ret = tapi_net_select_manual_test(0, "310", "260", "lte");
  sleep(4);
  assert_int_equal(ret, OK);
}

static void test_nuttx_net_scan(void **state)
{
  (void)state;
  int ret = tapi_net_scan_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_net_get_serving_cellinfos(void **state)
{
  (void)state;
  int ret = tapi_net_get_serving_cellinfos_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_net_get_neighbouring_cellinfos(void **state)
{
  (void)state;
  int ret = tapi_net_get_neighbouring_cellinfos_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_net_registration_info(void **state)
{
  (void)state;
  int ret = tapi_net_registration_info_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_net_get_operator_name(void **state)
{
  (void)state;
  int ret = tapi_net_get_operator_name_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_net_query_signal_strength(void **state)
{
  (void)state;
  int ret = tapi_net_query_signalstrength_test(0);
  assert_int_equal(ret, OK);
}

/* static void test_nuttx_net_set_cellinfo_listrate(void **state)
 * {
 *     int ret = tapi_net_set_cell_info_list_rate_test(0, 10);
 *     assert_int_equal(ret, OK);
 * }
 */

static void test_nuttx_net_getvoiceregistered(void **state)
{
  (void)state;
  int ret = tapi_net_get_voice_registered_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_get_voicenwtype(void **state)
{
  (void)state;
  tapi_network_type type = NETWORK_TYPE_UNKNOWN;
  int ret =
      tapi_network_get_voice_network_type(get_tapi_ctx(), 0, &type);
  syslog(LOG_INFO, "%s, ret: %d, type: %d", __func__, ret, (int)type);
  assert_int_equal(ret, OK);
  assert_int_equal((int)type, 13);
}

static void test_nuttx_get_voiceroaming(void **state)
{
  (void)state;
  bool value = true;
  int ret = tapi_network_is_voice_roaming(get_tapi_ctx(), 0, &value);
  syslog(LOG_INFO, "%s, ret: %d, value: %d", __func__, ret, (int)value);
  assert_int_equal(ret, OK);
  assert_int_equal((int)value, 0);
}

/* modem */

static void test_nuttx_modem_getimei(void **state)
{
  (void)state;
  int ret = tapi_get_imei_test(0);
  assert_int_equal(ret, OK);
}

/* static void test_nuttx_modem_set_umts_pref_net_mode(void** state)
 * {
 *     // defalut rat mode is NETWORK_PREF_NET_TYPE_LTE_GSM_WCDMA (9)
 *     tapi_pref_net_mode set_value = NETWORK_PREF_NET_TYPE_UMTS;
 *     int ret = tapi_set_pref_net_mode_test(0, set_value);
 *     assert_int_equal(ret, OK);
 * }

 * static void test_nuttx_modem_set_gsm_only_pref_net_mode(void** state)
 * {
 *     tapi_pref_net_mode set_value = NETWORK_PREF_NET_TYPE_GSM_ONLY;
 *     int ret = tapi_set_pref_net_mode_test(0, set_value);
 *     assert_int_equal(ret, OK);
 * }

 * static void test_nuttx_modem_set_wcdma_only_pref_netmode(void** state)
 * {
 *     tapi_pref_net_mode set_value = NETWORK_PREF_NET_TYPE_WCDMA_ONLY;
 *     int ret = tapi_set_pref_net_mode_test(0, set_value);
 *     assert_int_equal(ret, OK);
 * }

 * static void test_nuttx_modem_set_lte_only_pref_net_mode(void** state)
 * {
 *     tapi_pref_net_mode set_value = NETWORK_PREF_NET_TYPE_LTE_ONLY;
 *     int ret = tapi_set_pref_net_mode_test(0, set_value);
 *     assert_int_equal(ret, OK);
 * }

 * static void test_nuttx_modem_set_lte_wcdma_pref_net_mode(void** state)
 * {
 *     tapi_pref_net_mode set_value = NETWORK_PREF_NET_TYPE_LTE_WCDMA;
 *     int ret = tapi_set_pref_net_mode_test(0, set_value);
 *     assert_int_equal(ret, OK);
 * }

 * static void test_nuttx_modem_set_lte_gsmwcdma_pref_net_mode(void** state)
 * {
 *     tapi_pref_net_mode set_value =
 *     NETWORK_PREF_NET_TYPE_LTE_GSM_WCDMA; int ret =
 *     tapi_set_pref_net_mode_test(0, set_value); assert_int_equal(ret,
 *     OK);
 * }
 */

static void test_nuttx_modem_get_pref_net_mode(void **state)
{
  sleep(5);
  tapi_pref_net_mode get_value = NETWORK_PREF_NET_TYPE_ANY;
  int ret = tapi_get_pref_net_mode_test(0, &get_value);
  assert_int_equal(ret, OK);
}

static void test_nuttx_modem_set_radio_poweron(void **state)
{
  (void)state;
  int ret = tapi_set_radio_power_test(0, 1);
  assert_int_equal(ret, OK);
  sleep(10);
}

static void test_nuttx_modem_set_radio_poweroff(void **state)
{
  (void)state;
  int ret = tapi_set_radio_power_test(0, 0);
  assert_int_equal(ret, OK);
  sleep(10);
}

static void test_nuttx_modem_set_radio_poweron_off_repeatedly(void **state)
{
  (void)state;
  REPEAT_TEST_LESS_FOR
    {
      test_nuttx_modem_set_radio_poweron(state);
      test_nuttx_modem_set_radio_poweroff(state);
    }
}

static void test_nuttx_get_modem_enable_status(void **state)
{
  (void)state;
  int get_state = 1;
  int ret = tapi_get_modem_status_test(0, &get_state);
  assert_int_equal(ret, OK);
}

static void test_nuttx_get_modem_dsiable_status(void **state)
{
  (void)state;
  int get_state = 0;
  int ret = tapi_get_modem_status_test(0, &get_state);
  assert_int_equal(ret, OK);
}

static void test_nuttx_modem_enable(void **state)
{
  (void)state;
  int ret = tapi_enable_modem_test(0, 1);
  assert_int_equal(ret, OK);
  sleep(10);
}

static void test_nuttx_modem_disable(void **state)
{
  (void)state;
  int ret = tapi_enable_modem_test(0, 0);
  assert_int_equal(ret, OK);
  sleep(10);
}

static void test_nuttx_modem_enable_disable_repeatedly(void **state)
{
  (void)state;
  REPEAT_TEST_LESS_FOR
    {
      test_nuttx_modem_enable(state);
      test_nuttx_get_modem_enable_status(state);
      test_nuttx_modem_disable(state);
      test_nuttx_get_modem_dsiable_status(state);
    }
}

static void test_nuttx_modem_register(void **state)
{
  (void)state;
  int ret = tapi_modem_register_test(0);
  assert_int_equal(ret, OK);
}

static void test_nuttx_modem_unregister(void **state)
{
  (void)state;
  int ret = tapi_modem_unregister_test();
  assert_true(ret == OK);
}

static void test_nuttx_get_modem_revision(void **state)
{
  int ret;
  ret = tapi_get_modem_revision_test(0);
  assert_int_equal(ret, OK);
}

/* static void test_nuttx_ims_servicestatus(void** state)
 * {
 *     case_type* mode = *state;
 *     // device default info.reg_info is 1, info.ext_info is 5
 *     REPEAT_TEST_LESS_FOR
 *     {
 *         tapi_ims_registration_info info;
 *         int ret = tapi_ims_set_service_status_test(0, 1);
 *         if (*mode == CASE_NORMAL_MODE || *mode == CASE_AIRPLANE_MODE
 *            || *mode == CASE_CALL_DIALING) {
 *            assert_int_equal(ret, OK);
 *            ret = tapi_ims_get_registration_test(0, &info);
 *            assert_int_equal(ret, OK);
 *            assert_int_equal(info.reg_info, 1);
 *            assert_int_equal(info.ext_info, 1);
 *         } else {
 *             assert_int_equal(ret, -5);
 *         }
 *         sleep(1);

 *         ret = tapi_ims_set_service_status_test(0, 5);
 *         if (*mode == CASE_NORMAL_MODE || *mode == CASE_AIRPLANE_MODE
 *             || *mode == CASE_CALL_DIALING) {
 *             assert_int_equal(ret, OK);
 *             ret = tapi_ims_get_registration_test(0, &info);
 *             assert_int_equal(ret, OK);
 *             assert_int_equal(info.reg_info, 1);
 *             assert_int_equal(info.ext_info, 5);
 *             sleep(1);
 *         } else {
 *             assert_int_equal(ret, -5);
 *         }
 *     }
 * }

 *  *static void test_nuttx_modem_dialcall(void** state)
 * {
 *     int slot_id = 0;
 *     int ret1 = tapi_call_listen_call_test(slot_id);
 *     int ret2 = tapi_call_dial_test(slot_id, phone_num, 0);
 *     int ret = ret1 || ret2;
 *     assert_int_equal(ret, OK);
 *     sleep(2);
 * }

 * static void test_nuttx_modem_hangupcall(void** state)
 * {
 *     int slot_id = 0;
 *     int ret1 = tapi_call_hanup_current_call_test(slot_id);
 *     int ret2 = tapi_call_unlisten_call_test();
 *     int ret = ret1 || ret2;
 *     assert_int_equal(ret, OK);
 * }
 */

static void test_nuttx_modem_invokeoem_shot_ril_request_raw(void **state)
{
  int ret = tapi_invoke_oem_ril_request_raw_test(0, "01A0B023", 4);
  assert_int_equal(ret, OK);
}

static void test_nuttx_modem_invokeoem_long_ril_request_raw(void **state)
{
  int ret = tapi_invoke_oem_ril_request_raw_test(
      0, "01A0B02301A0B02301A0B02301A0B02301A0B02301", 21);
  assert_int_equal(ret, OK);
}

static void test_nuttx_modem_invokeoem_seperate_ril_request_raw(void **state)
{
  int ret = tapi_invoke_oem_ril_request_raw_test(0, "10|22", 2);
  assert_int_equal(ret, OK);
}

static void test_nuttx_modem_invokeoem_normal_ril_request_raw(void **state)
{
  int ret = tapi_invoke_oem_ril_request_raw_test(0, "01A0B023", 2);
  assert_int_equal(ret, OK);
}

static void test_nuttx_modem_invokeoem_ril_request_atcmdstrings(void **state)
{
  char *req_data = "AT+CPIN?";
  int ret = tapi_invoke_oem_ril_request_strings_test(0, req_data, 1);
  assert_int_equal(ret, OK);
}

static void
test_nuttx_modem_invokeoem_ril_request_notatcmdstrings(void **state)
{
  /* not AT cmd */

  char *req_data = "10|22";
  int ret = tapi_invoke_oem_ril_request_strings_test(0, req_data, 2);
  assert_int_equal(ret, OK);
}

static void test_nuttx_modem_invokeoem_ril_request_hexstrings(void **state)
{
  char *req_data = "0x10|0x01";
  int ret = tapi_invoke_oem_ril_request_strings_test(0, req_data, 2);
  assert_int_equal(ret, OK);
}

/* static void test_nuttx_modem_invokeoemrilrequestlongstrings(void **state)
 * {
 *     char req_data[MAX_INPUT_ARGS_LEN];

 *     // test error
 *     // FIXME: tapi_invoke_oem_ril_request_strings_test interface
 *     buffer overflow
 *     // when req_data len is 21, current max len is 20
 *     strcpy(req_data,
 *         "10|22|10|22|10|22|10|22|10|22|10|22|10|22|10|22|10|22|10|22");
 *     int ret = tapi_invoke_oem_ril_request_strings_test(0, req_data,
 *     20); assert_int_equal(ret, -1);

 *     // strcpy(req_data, "10|22");
 *     // // FIXME: _dbus_check_is_valid_utf8 is called by
 *     dbus_message_iter_append_basic
 *     // // cannot handle \0 in char * string;
 *     // ret = tapi_invoke_oem_ril_request_strings_test(0, req_data,
 *     20);
 *     // assert_int_equal(ret, -1);
 * }

 * static void cmocka_set_radio_power_on_off_test(void **state)
 * {
 *     for(int i = 0; i < 3; i++)
 *     {
 *         tapi_set_radio_power_test(0, 0);
 *         sleep(1);
 *         tapi_set_radio_power_test(0, 1);
 *         sleep(1);
 *     }
 *     int ret = tapi_get_radio_power_test(0);
 *     assert_int_equal(ret, 1);
 * }
 */

static void test_nuttx_ims_listen(void **state)
{
  (void)state;
  int ret = tapi_ims_listen_ims_test(0);
  assert_int_equal(ret, 0);
}

static void test_nuttx_ims_turnon(void **state)
{
  (void)state;
  int ret = tapi_ims_turn_on_test(0);
  assert_int_equal(ret, 0);
}

static void test_nuttx_ims_get_registration(void **state)
{
  (void)state;
  int ret = tapi_ims_get_registration_test(0);
  assert_int_equal(ret, 0);
}

static void test_nuttx_ims_getenabled(void **state)
{
  (void)state;
  int ret = tapi_ims_get_enabled_test(0);
  assert_int_equal(ret, 0);
}

static void test_nuttx_ims_set_service_status(void **state)
{
  (void)state;
  int ret = tapi_ims_set_service_status_test(0, 5);
  assert_int_equal(ret, 0);
  sleep(5);
}

static void test_nuttx_ims_set_sms_cap(void **state)
{
  (void)state;
  int ret = tapi_ims_set_service_status_test(0, 4);
  assert_int_equal(ret, 0);
}

static void test_nuttx_ims_setsmsvoicecap(void **state)
{
  (void)state;
  int ret = tapi_ims_set_service_status_test(0, 5);
  assert_int_equal(ret, 0);
}

static void test_nuttx_ims_reset_ims_cap(void **state)
{
  (void)state;
  int ret = tapi_ims_set_service_status_test(0, 1);
  assert_int_equal(ret, 0);
}

static void test_nuttx_ims_turnoff(void **state)
{
  (void)state;
  int ret = tapi_ims_turn_off_test(0);
  assert_int_equal(ret, 0);
}

static void test_nuttx_ims_turnonoff(void **state)
{
  (void)state;
  REPEAT_TEST_LESS_FOR
    {
      test_nuttx_ims_turnon(state);
      test_nuttx_ims_getenabled(state);
      test_nuttx_ims_get_registration(state);
      test_nuttx_ims_turnoff(state);
    }
}

static void test_nuttx_ssregister(void **state)
{
  (void)state;
  int ret = tapi_listen_ss_test(0);
  assert_int_equal(ret, 0);
}

static void test_nuttx_ssunregister(void **state)
{
  (void)state;
  int ret = tapi_unlisten_ss_test();
  assert_int_equal(ret, 0);
}

static void test_nuttx_request_call_barring(void **state)
{
  (void)state;
  int ret = tapi_ss_request_call_barring_test(0);
  assert_int_equal(ret, 0);
}

static void test_nuttx_set_call_barring(void **state)
{
  (void)state;
  int ret = tapi_ss_set_call_barring_option_test(0, "AI", "1234");
  assert_int_equal(ret, 0);
}

static void test_nuttx_get_call_barring(void **state)
{
  (void)state;
  sleep(5);
  int ret =
      tapi_ss_get_call_barring_option_test(0, "VoiceIncoming", "always");
  assert_int_equal(ret, 0);
}

static void test_nuttx_change_call_barring_password(void **state)
{
  (void)state;
  int ret = tapi_ss_change_call_barring_password_test(0, "1234", "2345");
  assert_int_equal(ret, 0);
}

static void test_nuttx_reset_call_barring_password(void **state)
{
  (void)state;
  int ret = tapi_ss_change_call_barring_password_test(0, "2345", "1234");
  assert_int_equal(ret, 0);
}

static void test_nuttx_disable_all_in_coming(void **state)
{
  (void)state;
  int ret = tapi_ss_disable_all_incoming_test(0, "1234");
  assert_int_equal(ret, 0);
}

static void test_nuttx_disable_all_out_going(void **state)
{
  (void)state;
  int ret = tapi_ss_disable_all_outgoing_test(0, "1234");
  assert_int_equal(ret, 0);
}

static void test_nuttx_disable_all_call_barrings(void **state)
{
  (void)state;
  int ret = tapi_ss_disable_all_call_barrings_test(0, "1234");
  assert_int_equal(ret, 0);
}

static void test_nuttx_set_call_forwarding_unconditional(void **state)
{
  (void)state;
  int ret = tapi_ss_set_call_forwarding_option_test(0, 0, "10086");
  assert_int_equal(ret, 0);
}

static void test_nuttx_get_call_forwarding_unconditional(void **state)
{
  (void)state;
  int ret = tapi_ss_get_call_forwarding_option_test(0, 0);
  assert_int_equal(ret, 0);
}

static void test_nuttx_clear_call_forwarding_unconditional(void **state)
{
  (void)state;
  int ret = tapi_ss_set_call_forwarding_option_test(0, 0, "\0");
  assert_int_equal(ret, 0);
}

static void test_nuttx_set_call_forwarding_busy(void **state)
{
  (void)state;
  int ret = tapi_ss_set_call_forwarding_option_test(0, 1, "10086");
  assert_int_equal(ret, 0);
}

static void test_nuttx_get_call_forwarding_busy(void **state)
{
  (void)state;
  int ret = tapi_ss_get_call_forwarding_option_test(0, 1);
  assert_int_equal(ret, 0);
}

static void test_nuttx_clear_call_forwarding_busy(void **state)
{
  (void)state;
  int ret = tapi_ss_set_call_forwarding_option_test(0, 1, "\0");
  assert_int_equal(ret, 0);
}

static void test_nuttx_set_call_forwarding_noreply(void **state)
{
  (void)state;
  int ret = tapi_ss_set_call_forwarding_option_test(0, 2, "10086");
  assert_int_equal(ret, 0);
}

static void test_nuttx_get_call_forwarding_noreply(void **state)
{
  (void)state;
  int ret = tapi_ss_get_call_forwarding_option_test(0, 2);
  assert_int_equal(ret, 0);
}

static void test_nuttx_clear_call_forwarding_noreply(void **state)
{
  (void)state;
  int ret = tapi_ss_set_call_forwarding_option_test(0, 2, "\0");
  assert_int_equal(ret, 0);
}

static void test_nuttx_set_call_forwarding_not_reachable(void **state)
{
  (void)state;
  int ret = tapi_ss_set_call_forwarding_option_test(0, 3, "10086");
  assert_int_equal(ret, 0);
}

static void test_nuttx_get_call_forwarding_not_reachable(void **state)
{
  (void)state;
  int ret = tapi_ss_get_call_forwarding_option_test(0, 3);
  assert_int_equal(ret, 0);
}

static void test_nuttx_clear_call_forwarding_not_reachable(void **state)
{
  (void)state;
  int ret = tapi_ss_set_call_forwarding_option_test(0, 3, "\0");
  assert_int_equal(ret, 0);
}

static void test_nuttx_enable_call_waiting(void **state)
{
  (void)state;
  int ret = tapi_ss_set_call_waiting_test(0, true);
  assert_int_equal(ret, 0);
}

static void test_nuttx_get_enable_call_waiting(void **state)
{
  (void)state;
  int ret = tapi_ss_get_call_waiting_test(0, true);
  assert_int_equal(ret, 0);
}

static void test_nuttx_disable_call_waiting(void **state)
{
  (void)state;
  int ret = tapi_ss_set_call_waiting_test(0, false);
  assert_int_equal(ret, 0);
}

static void test_nuttx_get_disable_call_waiting(void **state)
{
  (void)state;
  int ret = tapi_ss_get_call_waiting_test(0, false);
  assert_int_equal(ret, 0);
}

static void test_nuttx_enablefdn(void **state)
{
  (void)state;
  int ret = tapi_ss_enable_fdn_test(0, true, "1234");
  assert_int_equal(ret, 0);
}

static void test_nuttx_get_fdn_enabled(void **state)
{
  (void)state;
  int ret = tapi_ss_query_fdn_test(0, true);
  assert_int_equal(ret, 0);
}

static void test_nuttx_disable_fdn(void **state)
{
  (void)state;
  int ret = tapi_ss_enable_fdn_test(0, false, "1234");
  assert_int_equal(ret, 0);
}

static void test_nuttx_get_fdn_disabled(void **state)
{
  (void)state;
  int ret = tapi_ss_query_fdn_test(0, false);
  assert_int_equal(ret, 0);
}

static void on_tapi_client_ready(const char *client_name,
                                 void *user_data)
{
  if (client_name != NULL)
    syslog(LOG_DEBUG, "tapi is ready for %s\n", client_name);

  ready_done = 1;
}

static void *run_test_loop(void *args)
{
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  uv_loop_close(uv_default_loop());

  return NULL;
}

static void tapi_cb(tapi_async_result *result)
{
  if (result->msg_id == EVENT_MODEM_ENABLE_DONE && result->status == OK)
    {
      ready_done = 1;
    }
  else if (result->msg_id == MSG_VOICE_REGISTRATION_STATE_CHANGE_IND)
    {
      ready_done = 1;
    }
  else
    {
      ready_done = -1;
    }
}

static int wait_for_async_result(const char *str)
{
  while (ready_done != 1)
    {
      if (ready_done == -1 || count >= 10)
        {
          syslog(LOG_ERR, "%s\n", str);
          tapi_close(context);
          return -1;
        }
      else
        {
          sleep(1);
          count++;
        }
    }

  return 0;
}

int main(int argc, char *argv[])
{
#ifndef CONFIG_TEST_PHONE_NUMBER
  printf("Please config phone number in Kconfig!\n");
  return 0;
#endif
  int ret;

  ready_done = 0;
  phone_num = CONFIG_TEST_PHONE_NUMBER;

  context = tapi_open("vela.telephony.test", on_tapi_client_ready, NULL);
  if (context == NULL)
    {
      return 0;
    }

  /* initialize async handler before the thread creation
   * in case we have some race issues
   */

  uv_async_init(uv_default_loop(), &g_uv_exit, exit_async_cleanup);

  pthread_t thread;
  pthread_attr_t attr;
  struct sched_param param;
  pthread_attr_init(&attr);
  param.sched_priority = 100;
  pthread_attr_setschedparam(&attr, &param);
  pthread_attr_setstacksize(&attr, 262144);
  ret = pthread_create(&thread, &attr, run_test_loop, NULL);
  if (ret != 0)
    {
      tapi_close(context);
      return ret;
    }

  while (!ready_done)
    sleep(1);

  ready_done = 0;
  count = 0;
  tapi_enable_modem(get_tapi_ctx(), 0, EVENT_MODEM_ENABLE_DONE, 1,
                    tapi_cb);
  ret = wait_for_async_result(
      "modem failed to start and cannot be tested");
  if (ret == -1)
      return -1;

  ready_done = 0;
  count = 0;
  tapi_network_register(get_tapi_ctx(), 0,
                        MSG_VOICE_REGISTRATION_STATE_CHANGE_IND, NULL,
                        tapi_cb);
  ret = wait_for_async_result(
      "Network connection failure, unable to perform the test.");
  if (ret == -1)
      return -1;

  const struct CMUnitTest simtestsuites[] =
  {
      cmocka_unit_test(test_nuttx_has_icc_card),
      cmocka_unit_test(test_nuttx_has_icc_card_numerous_times),
      cmocka_unit_test(test_nuttx_get_sim_operator_name),
      cmocka_unit_test(test_nuttx_get_sim_operator_name_numeroustimes),
      cmocka_unit_test(test_nuttx_get_sim_operator),
      cmocka_unit_test(test_nuttx_get_sim_operator_numeroustimes),
      cmocka_unit_test(test_nuttx_get_sim_subscriberid),
      cmocka_unit_test(test_nuttx_get_sim_subscriberid_numerous_times),
      cmocka_unit_test(test_nuttx_get_iccid),
      cmocka_unit_test(test_nuttx_get_iccid_numeroustimes),
      cmocka_unit_test(test_nuttx_get_msisdn),
      cmocka_unit_test(test_nuttx_get_msisdn_numeroustimes),
      cmocka_unit_test(test_nuttx_transmit_apdu_inbasic_channel),
      cmocka_unit_test(test_nuttx_open_logical_channel),
      cmocka_unit_test(test_nuttx_close_logical_channel),
      cmocka_unit_test(test_nuttx_logical_channel_open_close_numerous),
      cmocka_unit_test(test_nuttx_transmit_apduin_logical_channel),
      cmocka_unit_test(test_nuttx_set_uiccenablement),
      cmocka_unit_test(test_nuttx_get_uiccenablement),
      cmocka_unit_test(test_nuttx_transmit_apdu_basic_channel),
      cmocka_unit_test(test_nuttx_get_sim_state),
      cmocka_unit_test(test_nuttx_enter_pin),
      cmocka_unit_test(test_nuttx_change_pin),
      cmocka_unit_test(test_nuttx_lockpin),
      cmocka_unit_test(test_nuttx_unlockpin),
      cmocka_unit_test(test_nuttx_load_adnentries),
      cmocka_unit_test(test_nuttx_load_fdnentries),
      cmocka_unit_test(test_nuttx_insert_fdnentry),
      cmocka_unit_test(test_nuttx_update_fdnentry),
      cmocka_unit_test(test_nuttx_delete_fdnentry),
  };

  const struct CMUnitTest calltestsuites[] =
  {
      cmocka_unit_test(test_nuttx_listen_call),
      cmocka_unit_test(test_nuttx_dial_eccnumber),
      cmocka_unit_test(test_nuttx_dial_long_phonenumber),
      cmocka_unit_test(test_nuttx_dial_shot_phonenumber),
      cmocka_unit_test(test_nuttx_dial_with_enable_hide_callid),
      cmocka_unit_test(test_nuttx_dial_with_disabled_hide_callid),
      cmocka_unit_test(test_nuttx_dial_with_default_hide_callid),
      cmocka_unit_test(test_nuttx_dial_with_areacode),
      cmocka_unit_test(test_nuttx_dial_with_pausecode),
      cmocka_unit_test(test_nuttx_dial_with_waitcode),
      cmocka_unit_test(test_nuttx_dial_with_numerouscode),
      cmocka_unit_test(test_nuttx_start_dtmf),
      cmocka_unit_test(test_nuttx_stop_dtmf),
      cmocka_unit_test(test_nuttx_dial_number),
      cmocka_unit_test(test_nuttx_get_callcount),
      cmocka_unit_test(test_nuttx_hangup_currentcall),

      /* cmocka_unit_test(test_nuttx_load_ecclist), */

      cmocka_unit_test(test_nuttx_set_voice_call_slot),
      cmocka_unit_test(test_nuttx_get_voice_call_slot),
      cmocka_unit_test(test_nuttx_clear_voice_call_slotset),
      cmocka_unit_test(test_nuttx_unlisten_call),

#if 0
        /* hangup between dialing and answering */

        cmocka_unit_test(test_nuttx_hangup_betweendialingandanswering),

        /* answer the incoming call then hangup it */

        cmocka_unit_test(test_nuttx_answerandhanguptheincomingcall),

        /* dial again after remote hangup */

        /* cmocka_unit_test(test_nuttx_dial_againafterremotehangup), */

        /* hangup after remote answer */

        cmocka_unit_test(test_nuttx_hangup_callafterremoteanswer),

        /* dial then remote answer and hangup */

        /* cmocka_unit_test(test_nuttx_dial_thenremoteanswerandhangup), */

        /* remote hangup then incoming new call */

        /* cmocka_unit_test(test_nuttx_remotehangupthenincomingnewcall), */

        /* remote hangup then dial another */

        /* cmocka_unit_test(test_nuttx_remotehangupthendialanother), */

        /* dial with numerous hide call id */

        cmocka_unit_test(test_nuttx_dial_with_numeroushidecallid),

        /* incoming new call then remote hangup */

        /* cmocka_unit_test(test_nuttx_incomingnewcallthenremotehangup), */

        /* dial using a phone number with area code */

        cmocka_unit_test(test_nuttx_dial_with_areacode),
#endif
  };

  const struct CMUnitTest datatestsuites[] =
  {
      cmocka_unit_test(test_nuttx_data_register),
      cmocka_unit_test(test_nuttx_data_unregister),
      cmocka_unit_test(test_nuttx_data_register),
      cmocka_unit_test(test_nuttx_data_load_apn_contexts),
      cmocka_unit_test(test_nuttx_data_save_apn_context),
      cmocka_unit_test(test_nuttx_data_remove_apn_context),
      cmocka_unit_test(test_nuttx_data_reset_apn_contexts),
      cmocka_unit_test(test_nuttx_data_reset_apn_contexts_repeatedly),
      cmocka_unit_test(test_nuttx_data_edit_apn_name),
      cmocka_unit_test(test_nuttx_data_edit_apn_type),
      cmocka_unit_test(test_nuttx_data_edit_apn_proto),
      cmocka_unit_test(test_nuttx_data_edit_apn_auth),
      cmocka_unit_test(test_nuttx_data_edit_apn_all),
      cmocka_unit_test(test_nuttx_data_edit_apn_and_remove),
      cmocka_unit_test(test_nuttx_data_edit_apn_and_reset),
      cmocka_unit_test(test_nuttx_data_edit_apn_repeatedly_and_load),
      cmocka_unit_test(test_nuttx_data_reset_apn_contexts),
      cmocka_unit_test(test_nuttx_data_enable),
      cmocka_unit_test(test_nuttx_data_isenable),
      cmocka_unit_test(test_nuttx_data_disable),
      cmocka_unit_test(test_nuttx_data_isdisable),
      cmocka_unit_test(test_nuttx_data_enable_repeatedly),
      cmocka_unit_test(test_nuttx_data_enable),
      cmocka_unit_test(test_nuttx_data_release_network_internet),
      cmocka_unit_test(test_nuttx_data_request_network_internet),
      cmocka_unit_test(test_nuttx_data_release_network_internet_repeatedly),
      cmocka_unit_test(test_nuttx_data_disable),
      cmocka_unit_test(test_nuttx_data_request_network_ims),
      cmocka_unit_test(test_nuttx_data_release_network_ims),
      cmocka_unit_test(test_nuttx_data_release_network_ims_repeatedly),
      cmocka_unit_test(test_nuttx_data_save_apn_context_supl),
      cmocka_unit_test(test_nuttx_data_save_apn_context_emergency),
      cmocka_unit_test(test_nuttx_data_reset_apn_contexts),
      cmocka_unit_test(test_nuttx_data_set_preferred_apn),
      cmocka_unit_test(test_nuttx_data_get_preferred_apn),
      cmocka_unit_test(test_nuttx_data_send_screen_state),
      cmocka_unit_test(test_nuttx_data_get_network_type),
      cmocka_unit_test(test_nuttx_data_ispsattached),
      cmocka_unit_test(test_nuttx_data_set_default_data_slot),
      cmocka_unit_test(test_nuttx_data_get_default_data_slot),
      cmocka_unit_test(test_nuttx_data_set_data_allow),
      cmocka_unit_test(test_nuttx_data_get_call_list),
      cmocka_unit_test(test_nuttx_data_save_long_apn_contex),
      cmocka_unit_test(test_nuttx_data_reset_apn_contexts),
      cmocka_unit_test(test_nuttx_enable_data_roaming),
      cmocka_unit_test(test_nuttx_disable_data_roaming),
      cmocka_unit_test(test_nuttx_data_unregister),
  };

  const struct CMUnitTest smstestsuites[] =
  {
      cmocka_unit_test(test_nuttx_listen_call),
      cmocka_unit_test(test_nuttx_ims_listen),
      cmocka_unit_test(test_nuttx_ims_turnon),
      cmocka_unit_test(test_nuttx_sms_set_service_center_num),
      cmocka_unit_test(test_nuttx_sms_get_service_center_num),
      cmocka_unit_test(test_nuttx_sms_send_short_message_in_english),
      cmocka_unit_test(test_nuttx_sms_send_short_message_in_chinese),
      cmocka_unit_test(test_nuttx_sms_send_short_data_message_in_english),
      cmocka_unit_test(test_nuttx_sms_send_short_data_message_in_chinese),
      cmocka_unit_test(test_nuttx_sms_send_long_message_in_english),
      cmocka_unit_test(test_nuttx_sms_send_long_message_in_chinese),
      cmocka_unit_test(test_nuttx_sms_send_long_data_message_in_english),
      cmocka_unit_test(test_nuttx_sms_send_long_data_message_in_chinese),
      cmocka_unit_test(test_nuttx_sms_send_short_english_message_in_dialing),
      cmocka_unit_test(test_nuttx_sms_send_short_chinese_message_in_dialing),
      cmocka_unit_test(test_nuttx_sms_send_long_english_message_in_dialing),
      cmocka_unit_test(test_nuttx_sms_send_long_chinese_message_in_dialing),
      cmocka_unit_test
      (test_nuttx_sms_send_short_english_data_message_in_dialing),
      cmocka_unit_test
      (test_nuttx_sms_send_short_chinese_data_message_in_dialing),
      cmocka_unit_test
      (test_nuttx_sms_send_long_english_data_message_in_dialing),
      cmocka_unit_test
      (test_nuttx_sms_send_long_chinese_data_message_in_dialing),
      cmocka_unit_test(test_nuttx_send_english_message_in_voice_imscap),
      cmocka_unit_test(test_nuttx_send_chinese_message_in_voice_imscap),
      cmocka_unit_test(test_nuttx_send_long_english_message_in_voice_imscap),
      cmocka_unit_test(test_nuttx_send_long_chinese_message_in_voice_imscap),
      cmocka_unit_test(test_nuttx_send_english_data_message_in_voice_imscap),
      cmocka_unit_test(test_nuttx_send_chinese_data_message_in_voice_imscap),
      cmocka_unit_test
      (test_nuttx_send_long_english_data_message_in_voice_imscap),
      cmocka_unit_test
      (test_nuttx_send_long_chinese_data_message_in_voice_imscap),
      cmocka_unit_test(test_nuttx_ims_set_sms_cap),
      cmocka_unit_test(test_nuttx_send_english_message_in_smsimscap),
      cmocka_unit_test(test_nuttx_send_chinese_message_in_smsimscap),
      cmocka_unit_test(test_nuttx_send_long_english_message_in_smsimscap),
      cmocka_unit_test(test_nuttx_send_long_chinese_message_in_smsimscap),
      cmocka_unit_test(test_nuttx_send_english_data_message_in_smsimscap),
      cmocka_unit_test(test_nuttx_send_chinese_data_message_in_smsimscap),
      cmocka_unit_test
      (test_nuttx_send_long_english_data_message_in_smsimscap),
      cmocka_unit_test
      (test_nuttx_send_long_chinese_data_message_in_smsimscap),
      cmocka_unit_test(test_nuttx_ims_setsmsvoicecap),
      cmocka_unit_test(test_nuttx_send_english_message_insmsvoicecap),
      cmocka_unit_test(test_nuttx_send_chinese_message_insmsvoicecap),
      cmocka_unit_test(test_nuttx_send_long_english_message_insmsvoicecap),
      cmocka_unit_test(test_nuttx_send_long_chinese_message_insmsvoicecap),
      cmocka_unit_test(test_nuttx_send_english_data_message_in_smsvoicecap),
      cmocka_unit_test(test_nuttx_send_chinese_data_message_in_smsvoicecap),
      cmocka_unit_test
      (test_nuttx_send_long_english_data_message_in_smsvoicecap),
      cmocka_unit_test
      (test_nuttx_send_long_chinese_data_message_in_smsvoicecap),
      cmocka_unit_test(test_nuttx_set_sms_default_slot),
      cmocka_unit_test(test_nuttx_get_sms_default_slot),
      cmocka_unit_test(test_nuttx_sms_set_cell_broad_cast_power),
      cmocka_unit_test(test_nuttx_sms_get_cell_broad_cast_power),
      cmocka_unit_test(test_nuttx_sms_set_cell_broad_cast_topics),
      cmocka_unit_test(test_nuttx_sms_get_cell_broad_cast_topics),
      cmocka_unit_test(test_nuttx_ims_reset_ims_cap),
      cmocka_unit_test(test_nuttx_ims_turnoff),
      cmocka_unit_test(test_nuttx_unlisten_call),
  };

  const struct CMUnitTest nettestsuites[] =
  {
      cmocka_unit_test(test_nuttx_net_select_manual),
      cmocka_unit_test(test_nuttx_net_select_auto),
      cmocka_unit_test(test_nuttx_net_scan),
      cmocka_unit_test(test_nuttx_net_get_serving_cellinfos),
      cmocka_unit_test(test_nuttx_net_get_neighbouring_cellinfos),
      cmocka_unit_test(test_nuttx_net_registration_info),
      cmocka_unit_test(test_nuttx_net_get_operator_name),
      cmocka_unit_test(test_nuttx_net_query_signal_strength),

      /* cmocka_unit_test(test_nuttx_net_set_cellinfo_listrate), */

      cmocka_unit_test(test_nuttx_net_getvoiceregistered),
      cmocka_unit_test(test_nuttx_get_voicenwtype),
      cmocka_unit_test(test_nuttx_get_voiceroaming),
  };

  const struct CMUnitTest imstestsuits[] =
  {
      cmocka_unit_test(test_nuttx_ims_turnon),
      cmocka_unit_test(test_nuttx_ims_get_registration),
      cmocka_unit_test(test_nuttx_ims_getenabled),
      cmocka_unit_test(test_nuttx_ims_set_service_status),
      cmocka_unit_test(test_nuttx_ims_reset_ims_cap),
      cmocka_unit_test(test_nuttx_ims_turnoff),
      cmocka_unit_test(test_nuttx_ims_turnonoff),
  };

  const struct CMUnitTest sstestsuits[] =
  {
      cmocka_unit_test(test_nuttx_ssregister),
      cmocka_unit_test(test_nuttx_ssunregister),
      cmocka_unit_test(test_nuttx_request_call_barring),
      cmocka_unit_test(test_nuttx_set_call_barring),
      cmocka_unit_test(test_nuttx_get_call_barring),
      cmocka_unit_test(test_nuttx_change_call_barring_password),
      cmocka_unit_test(test_nuttx_reset_call_barring_password),
      cmocka_unit_test(test_nuttx_disable_all_in_coming),
      cmocka_unit_test(test_nuttx_disable_all_out_going),
      cmocka_unit_test(test_nuttx_disable_all_call_barrings),
      cmocka_unit_test(test_nuttx_set_call_forwarding_unconditional),
      cmocka_unit_test(test_nuttx_get_call_forwarding_unconditional),
      cmocka_unit_test(test_nuttx_clear_call_forwarding_unconditional),
      cmocka_unit_test(test_nuttx_set_call_forwarding_busy),
      cmocka_unit_test(test_nuttx_get_call_forwarding_busy),
      cmocka_unit_test(test_nuttx_clear_call_forwarding_busy),
      cmocka_unit_test(test_nuttx_set_call_forwarding_noreply),
      cmocka_unit_test(test_nuttx_get_call_forwarding_noreply),
      cmocka_unit_test(test_nuttx_clear_call_forwarding_noreply),
      cmocka_unit_test(test_nuttx_set_call_forwarding_not_reachable),
      cmocka_unit_test(test_nuttx_get_call_forwarding_not_reachable),
      cmocka_unit_test(test_nuttx_clear_call_forwarding_not_reachable),
      cmocka_unit_test(test_nuttx_enable_call_waiting),
      cmocka_unit_test(test_nuttx_get_enable_call_waiting),
      cmocka_unit_test(test_nuttx_disable_call_waiting),
      cmocka_unit_test(test_nuttx_get_disable_call_waiting),
      cmocka_unit_test(test_nuttx_enablefdn),
      cmocka_unit_test(test_nuttx_get_fdn_enabled),
      cmocka_unit_test(test_nuttx_disable_fdn),
      cmocka_unit_test(test_nuttx_get_fdn_disabled),
  };

  const struct CMUnitTest commontestsuites[] =
  {
      cmocka_unit_test(test_nuttx_modem_getimei),

      /* cmocka_unit_test(test_nuttx_modem_set_umts_pref_net_mode),
       * cmocka_unit_test(test_nuttx_modem_set_gsm_only_pref_net_mode),
       * cmocka_unit_test(test_nuttx_modem_set_wcdma_only_pref_netmode),
       * cmocka_unit_test(test_nuttx_modem_set_lte_only_pref_net_mode),
       * cmocka_unit_test(test_nuttx_modem_set_lte_wcdma_pref_net_mode),
       * cmocka_unit_test(test_nuttx_modem_set_lte_gsmwcdma_pref_net_mode),
       */

      cmocka_unit_test(test_nuttx_modem_get_pref_net_mode),
      cmocka_unit_test(test_nuttx_modem_register),
      cmocka_unit_test(test_nuttx_modem_unregister),
      cmocka_unit_test(test_nuttx_modem_invokeoem_shot_ril_request_raw),
      cmocka_unit_test(test_nuttx_modem_invokeoem_long_ril_request_raw),
      cmocka_unit_test(test_nuttx_modem_invokeoem_normal_ril_request_raw),
      cmocka_unit_test(test_nuttx_modem_invokeoem_seperate_ril_request_raw),
      cmocka_unit_test(test_nuttx_modem_invokeoem_ril_request_atcmdstrings),
      cmocka_unit_test
      (test_nuttx_modem_invokeoem_ril_request_notatcmdstrings),
      cmocka_unit_test(test_nuttx_modem_invokeoem_ril_request_hexstrings),
      cmocka_unit_test(test_nuttx_ims_listen),
      cmocka_unit_test(test_nuttx_get_modem_revision),
      cmocka_unit_test(test_nuttx_modem_disable),
      cmocka_unit_test(test_nuttx_get_modem_dsiable_status),
      cmocka_unit_test(test_nuttx_modem_enable_disable_repeatedly),
      cmocka_unit_test(test_nuttx_modem_enable),
      cmocka_unit_test(test_nuttx_get_modem_enable_status),
      cmocka_unit_test(test_nuttx_modem_set_radio_poweroff),
      cmocka_unit_test(test_nuttx_modem_set_radio_poweron_off_repeatedly),
      cmocka_unit_test(test_nuttx_modem_set_radio_poweron),
      cmocka_unit_test(test_nuttx_modem_disable),

      /* cmocka_unit_test_setup_teardown(
       * test_nuttx_modem_set_radio_poweron_off_repeatedly,
       * setup_normal_mode, free_mode),
       * // Airplane mode
       * cmocka_unit_test(test_nuttx_modem_set_radio_poweroff),
       * cmocka_unit_test_setup_teardown(test_nuttx_ims_servicestatus,
       * //     setup_airplane_mode, free_mode),
       * cmocka_unit_test(test_nuttx_modem_set_radio_poweron),

       * // Call dialing
       * cmocka_unit_test(test_nuttx_modem_dialcall),
       * // TODO: enable disable modem
       * cmocka_unit_test(test_nuttx_modem_set_radio_poweron),
       * // cmocka_unit_test_setup_teardown(test_nuttx_ims_servicestatus,
       * //     setup_call_dialing, free_mode),
       * cmocka_unit_test(test_nuttx_modem_hangupcall),

       * // Modem poweroff

       * cmocka_unit_test_setup_teardown(test_nuttx_ims_servicestatus,
       * //     setup_modem_poweroff, free_mode),
       * cmocka_unit_test_setup_teardown(
       * test_nuttx_modem_set_radio_poweron_off_repeatedly,
       * setup_modem_poweroff, free_mode),
       * // FIXME: Cannot enable because of RADIO_NOT_AVAILABLE
       * // cmocka_unit_test(test_nuttx_modem_enable),
       */
  };

  sleep(3);
  cmocka_run_group_tests(simtestsuites, NULL, NULL);

  cmocka_run_group_tests(calltestsuites, NULL, NULL);

  cmocka_run_group_tests(datatestsuites, NULL, NULL);

  cmocka_run_group_tests(smstestsuites, NULL, NULL);

  cmocka_run_group_tests(nettestsuites, NULL, NULL);

  cmocka_run_group_tests(imstestsuits, NULL, NULL);

  cmocka_run_group_tests(sstestsuits, NULL, NULL);

  cmocka_run_group_tests(commontestsuites, NULL, NULL);

  tapi_close(context);
  uv_async_send(&g_uv_exit);

  pthread_join(thread, NULL);
  uv_close((uv_handle_t *)&g_uv_exit, NULL);

  return 0;
}
