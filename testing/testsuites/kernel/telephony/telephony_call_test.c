/****************************************************************************
 * apps/testing/testsuites/kernel/telephony/telephony_call_test.c
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
#include "telephony_call_test.h"
#include <string.h>

/****************************************************************************
 * Public Functions
 ****************************************************************************/

extern char *phone_num;
extern int sock_fd;
char modem_command_buf[512];

extern struct judge_type judge_data;

static struct
{
  int call_state_watch_id;
  int call_emergencylist_change_watch_id;
  int call_ring_back_tone_change_watch_id;
  int call_slot_change_watch_id;
  int ss_call_barring_watch_id;
  int ss_ussd_property_change_watch_id;
  int ss_ussd_notification_received_watch_id;
  int ss_ussd_request_received_watch_id;
  int default_voicecall_slot;
} global_data;

static struct
{
  char call_id[101];
  char network_name[101];
  unsigned int call_count;
  int two_call_state_sum;
  char hold_call_id[101];
  int current_call_state;
} test_case_data;

static void test_case_data_init(void)
{
  memset(&test_case_data, 0, sizeof(test_case_data));
  test_case_data.current_call_state = -1;
}

static int incoming_call(char *incoming_number)
{
  return 0;
}

static int hangup_remote_call(int call_id)
{
  return 0;
}

static void call_state_change_cb(tapi_async_result *result)
{
  tapi_call_info *call_info;

  syslog(LOG_DEBUG, "%s : %d\n", __func__, result->status);
  call_info = (tapi_call_info *)result->data;

  syslog(LOG_DEBUG, "call changed call_id : %s\n", call_info->call_id);
  syslog(LOG_DEBUG, "call state: %d \n", call_info->state);
  syslog(LOG_DEBUG, "call IncomingLine: %s \n",
         call_info->incoming_line);
  syslog(LOG_DEBUG, "call Name: %s \n", call_info->name);
  syslog(LOG_DEBUG, "call StartTime: %s \n", call_info->start_time);
  syslog(LOG_DEBUG, "call Multiparty: %d \n", call_info->multiparty);
  syslog(LOG_DEBUG, "call RemoteHeld: %d \n", call_info->remote_held);
  syslog(LOG_DEBUG, "call RemoteMultiparty: %d \n",
         call_info->remote_multiparty);
  syslog(LOG_DEBUG, "call Information: %s \n", call_info->info);
  syslog(LOG_DEBUG, "call Icon: %d \n", call_info->icon);
  syslog(LOG_DEBUG, "call Emergency: %d \n",
         call_info->is_emergency_number);
  syslog(LOG_DEBUG, "call disconnect_reason: %d \n\n",
         call_info->disconnect_reason);

  if (judge_data.expect == CALL_LOCAL_HANGUP)
    {
      if (call_info->disconnect_reason ==
          CALL_DISCONNECT_REASON_LOCAL_HANGUP)
        {
          judge_data.result = 0;
        }

      judge_data.flag = CALL_LOCAL_HANGUP;
    }

  else if (judge_data.expect == CALL_STATE_CHANGE_TO_ACTIVE)
    {
      if (call_info->state == 0)
        {
          judge_data.result = 0;
        }

      judge_data.flag = CALL_STATE_CHANGE_TO_ACTIVE;
    }

  else if (judge_data.expect == CALL_REMOTE_HANGUP)
    {
      if (call_info->disconnect_reason ==
          CALL_DISCONNECT_REASON_REMOTE_HANGUP)
        {
          judge_data.result = 0;
        }

      judge_data.flag = CALL_REMOTE_HANGUP;
    }

  else if (judge_data.expect == NEW_CALL_INCOMING ||
           judge_data.expect == NEW_CALL_WAITING ||
           judge_data.expect == INCOMING_CALL_WITH_NETWORK_NAME)
    {
      char *call_id = call_info->call_id;
      strncpy(test_case_data.call_id, call_id, strlen(call_id));
      test_case_data.call_id[strlen(call_id)] = '\0';

      if (judge_data.expect == INCOMING_CALL_WITH_NETWORK_NAME)
        {
          char *network_name = call_info->name;
          strncpy(test_case_data.network_name, network_name,
                  strlen(network_name));
          test_case_data.network_name[strlen(network_name)] = '\0';
          judge_data.result = 0;
          judge_data.flag = INCOMING_CALL_WITH_NETWORK_NAME;
        }

      else
        {
          if (judge_data.expect == NEW_CALL_INCOMING &&
              call_info->state == 4)
            {
              judge_data.result = 0;
              judge_data.flag = NEW_CALL_INCOMING;
            }

          else if (judge_data.expect == NEW_CALL_WAITING &&
                   call_info->state == 5)
            {
              judge_data.result = 0;
              judge_data.flag = NEW_CALL_WAITING;
            }
        }
    }

  else if (judge_data.expect == HANGUP_DUE_TO_NETWORK_EXCEPTION)
    {
      if (call_info->disconnect_reason ==
          CALL_DISCONNECT_REASON_NETWORK_HANGUP)
        {
          judge_data.result = 0;
        }

      judge_data.flag = judge_data.expect;
    }

  else if (judge_data.expect == CALL_STATE_CHANGE_TO_HOLD)
    {
      if (call_info->state == 1)
        {
          judge_data.result = 0;
        }

      judge_data.flag = judge_data.expect;
    }
}

static void tele_call_ecc_list_async_fun(tapi_async_result *result)
{
  int status = result->status;
  int list_length = result->arg2;
  ecc_info *ret = result->data;

  syslog(LOG_DEBUG, "%s : \n", __func__);
  syslog(LOG_DEBUG, "msg_id : %d\n", result->msg_id);
  syslog(LOG_DEBUG, "status : %d\n", status);
  syslog(LOG_DEBUG, "list length: %d\n", list_length);

  if (result->status == 0)
    {
      for (int i = 0; i < list_length; i++)
        {
          syslog(LOG_DEBUG, "ecc number : %s,%u,%u \n", ret[i].ecc_num,
                 ret[i].category, ret[i].condition);
        }
    }
}

static void tele_call_manager_call_async_fun(tapi_async_result *result)
{
  tapi_call_info *call_info;

  syslog(LOG_DEBUG, "%s : %d\n", __func__, result->status);
  if (result->status != OK)
    {
      syslog(LOG_ERR, "async result error in %s", __func__);
      return;
    }

  if (result->msg_id == MSG_CALL_ADD_MESSAGE_IND)
    {
      call_info = (tapi_call_info *)result->data;

      syslog(LOG_DEBUG, "call added call_id : %s\n", call_info->call_id);
      syslog(LOG_DEBUG, "call state: %d \n", call_info->state);
      syslog(LOG_DEBUG, "call IncomingLine: %s \n",
             call_info->incoming_line);
      syslog(LOG_DEBUG, "call Name: %s \n", call_info->name);
      syslog(LOG_DEBUG, "call StartTime: %s \n", call_info->start_time);
      syslog(LOG_DEBUG, "call Multiparty: %d \n", call_info->multiparty);
      syslog(LOG_DEBUG, "call RemoteHeld: %d \n",
             call_info->remote_held);
      syslog(LOG_DEBUG, "call RemoteMultiparty: %d \n",
             call_info->remote_multiparty);
      syslog(LOG_DEBUG, "call Information: %s \n", call_info->info);
      syslog(LOG_DEBUG, "call Icon: %d \n", call_info->icon);
      syslog(LOG_DEBUG, "call Emergency: %d \n\n",
             call_info->is_emergency_number);
    }

  else if (result->msg_id == MSG_CALL_REMOVE_MESSAGE_IND)
    {
      syslog(LOG_DEBUG, "call removed call_id : %s\n",
             (char *)result->data);
    }

  else if (result->msg_id == MSG_CALL_RING_BACK_TONE_IND)
    {
      syslog(LOG_DEBUG, "ring back tone status : %d\n", result->arg2);
    }

  else if (result->msg_id == MSG_CALL_FORWARDED_MESSAGE_IND)
    {
      syslog(LOG_DEBUG, "call Forwarded: %s\n", (char *)result->data);
    }

  else if (result->msg_id == MSG_CALL_BARRING_ACTIVE_MESSAGE_IND)
    {
      syslog(LOG_DEBUG, "call BarringActive: %s\n",
             (char *)result->data);
    }

  else if (result->msg_id == MSG_DEFAULT_VOICECALL_SLOT_CHANGE_IND)
    {
      if (judge_data.expect == MSG_DEFAULT_VOICECALL_SLOT_CHANGE_IND)
        {
          syslog(LOG_DEBUG, "default voicecall slot: %d\n",
                 result->arg2);
          judge_data.result = OK;
          global_data.default_voicecall_slot = result->arg2;
          judge_data.flag = MSG_DEFAULT_VOICECALL_SLOT_CHANGE_IND;
        }
    }
}

int tapi_call_listen_call_test(int slot_id)
{
  global_data.call_state_watch_id = -1;
  global_data.call_state_watch_id = tapi_call_register_call_state_change(
      get_tapi_ctx(), slot_id, NULL, call_state_change_cb);

  if (global_data.call_state_watch_id < 0)
    {
      syslog(LOG_ERR, "%s, call state change registered fail, ret: %d",
             __func__, global_data.call_state_watch_id);
      return -1;
    }

  global_data.call_emergencylist_change_watch_id = -1;
  global_data.call_emergencylist_change_watch_id =
      tapi_call_register_emergency_list_change(
          get_tapi_ctx(), slot_id, NULL, tele_call_ecc_list_async_fun);

  if (global_data.call_emergencylist_change_watch_id < 0)
    {
      syslog(LOG_ERR,
             "%s, emergency list change registered fail, ret: %d",
             __func__, global_data.call_emergencylist_change_watch_id);
      return -1;
    }

  global_data.call_ring_back_tone_change_watch_id = -1;
  global_data.call_ring_back_tone_change_watch_id =
      tapi_call_register_ringback_tone_change(
          get_tapi_ctx(), slot_id, NULL,
          tele_call_manager_call_async_fun);

  if (global_data.call_ring_back_tone_change_watch_id < 0)
    {
      syslog(LOG_ERR, "%s, ring back change registered fail, ret: %d",
             __func__, global_data.call_ring_back_tone_change_watch_id);
      return -1;
    }

  global_data.call_slot_change_watch_id = -1;
  global_data.call_slot_change_watch_id =
      tapi_call_register_default_voicecall_slot_change(
          get_tapi_ctx(), NULL, tele_call_manager_call_async_fun);
  if (global_data.call_slot_change_watch_id < 0)
    {
      syslog(LOG_ERR,
             "%s, voicecall slot change registered fail, ret: %d",
             __func__, global_data.call_slot_change_watch_id);
      return -1;
    }

  return 0;
}

static void tele_call_async_fun(tapi_async_result *result)
{
  syslog(LOG_DEBUG, "%s : \n", __func__);
  syslog(LOG_DEBUG, "result->msg_id : %d\n", result->msg_id);
  syslog(LOG_DEBUG, "result->status : %d\n", result->status);
  syslog(LOG_DEBUG, "result->arg1 : %d\n", result->arg1);
  syslog(LOG_DEBUG, "result->arg2 : %d\n", result->arg2);

  if (result->status != OK)
    {
      syslog(LOG_DEBUG, "%s msg id : %d result err, return.\n", __func__,
             result->msg_id);
      return;
    }

  int event = result->msg_id;
  int status = result->status;

  switch (event)
    {
    case EVENT_REQUEST_DIAL_DONE:
      syslog(LOG_DEBUG, "%s: EVENT_REQUEST_DIAL_DONE status: %d\n",
             __func__, result->status);
      if (judge_data.expect == EVENT_REQUEST_DIAL_DONE)
        {
          judge_data.result = status;
          char *call_id = (char *)result->data;
          strncpy(test_case_data.call_id, call_id, strlen(call_id));
          test_case_data.call_id[strlen(call_id)] = '\0';
          judge_data.flag = EVENT_REQUEST_DIAL_DONE;
        }
      break;
    case EVENT_REQUEST_START_DTMF_DONE:
      syslog(LOG_DEBUG, "%s: EVENT_REQUEST_START_DTMF_DONE status: %d\n",
             __func__, result->status);
      if (judge_data.expect == EVENT_REQUEST_START_DTMF_DONE)
        {
          judge_data.result = status;
          judge_data.flag = EVENT_REQUEST_START_DTMF_DONE;
        }
      break;
    case EVENT_REQUEST_STOP_DTMF_DONE:
      syslog(LOG_DEBUG, "%s: EVENT_REQUEST_STOP_DTMF_DONE status: %d\n",
             __func__, result->status);
      if (judge_data.expect == EVENT_REQUEST_STOP_DTMF_DONE)
        {
          judge_data.result = status;
          judge_data.flag = EVENT_REQUEST_STOP_DTMF_DONE;
        }
      break;
    default:
      break;
    }
}

static void ss_signal_change(tapi_async_result *result)
{
  tapi_call_barring_info *cb_value;
  int signal = result->msg_id;
  int slot_id = result->arg1;

  switch (signal)
    {
    case MSG_CALL_BARRING_PROPERTY_CHANGE_IND:
      cb_value = result->data;
      syslog(LOG_DEBUG,
             "call barring service %s changed to %s in slot[%d] \n",
             cb_value->service_type, cb_value->value, slot_id);
      break;
    case MSG_USSD_PROPERTY_CHANGE_IND:
      syslog(LOG_DEBUG, "ussd state changed to %s in slot[%d] \n",
             (char *)result->data, slot_id);
      break;
    case MSG_USSD_NOTIFICATION_RECEIVED_IND:
      syslog(LOG_DEBUG,
             "ussd notification message %s received in slot[%d] \n",
             (char *)result->data, slot_id);
      break;
    case MSG_USSD_REQUEST_RECEIVED_IND:
      syslog(LOG_DEBUG,
             "ussd request message %s received in slot[%d] \n",
             (char *)result->data, slot_id);
      break;
    default:
      break;
    }
}

int tapi_ss_listen_test(int slot_id)
{
  global_data.ss_call_barring_watch_id = -1;
  global_data.ss_call_barring_watch_id = tapi_ss_register(
      get_tapi_ctx(), slot_id, 47, NULL, ss_signal_change);

  if (global_data.ss_call_barring_watch_id < 0)
    {
      syslog(LOG_ERR, "%s, slot_id: %d, watch_id < 0\n", __func__,
             slot_id);
      return -1;
    }

  global_data.ss_ussd_property_change_watch_id = -1;
  global_data.ss_ussd_property_change_watch_id = tapi_ss_register(
      get_tapi_ctx(), slot_id, 50, NULL, ss_signal_change);

  if (global_data.ss_ussd_property_change_watch_id < 0)
    {
      syslog(LOG_ERR, "%s, slot_id: %d, watch_id < 0\n", __func__,
             slot_id);
      return -1;
    }

  global_data.ss_ussd_notification_received_watch_id = -1;
  global_data.ss_ussd_notification_received_watch_id = tapi_ss_register(
      get_tapi_ctx(), slot_id, 48, NULL, ss_signal_change);

  if (global_data.ss_ussd_notification_received_watch_id < 0)
    {
      syslog(LOG_ERR, "%s, slot_id: %d, watch_id < 0\n", __func__,
             slot_id);
      return -1;
    }

  global_data.ss_ussd_request_received_watch_id = -1;
  global_data.ss_ussd_request_received_watch_id = tapi_ss_register(
      get_tapi_ctx(), slot_id, 49, NULL, ss_signal_change);

  if (global_data.ss_ussd_request_received_watch_id < 0)
    {
      syslog(LOG_ERR, "%s, slot_id: %d, watch_id < 0\n", __func__,
             slot_id);
      return -1;
    }

  return 0;
}

int tapi_ss_unlisten_test(void)
{
  if (global_data.ss_call_barring_watch_id < 0 ||
      global_data.ss_ussd_notification_received_watch_id < 0 ||
      global_data.ss_ussd_property_change_watch_id < 0 ||
      global_data.ss_ussd_request_received_watch_id < 0)
      return -1;

  int ret = -1;
  int res = 0;
  ret = tapi_ss_unregister(get_tapi_ctx(),
                           global_data.ss_call_barring_watch_id);
  if (ret)
    {
      syslog(LOG_ERR,
             "unregister ss call barring change fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  ret = tapi_ss_unregister(
      get_tapi_ctx(),
      global_data.ss_ussd_notification_received_watch_id);
  if (ret)
    {
      syslog(LOG_ERR,
             "unregister ss ussd notification received change fail in "
             "%s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  ret = tapi_ss_unregister(get_tapi_ctx(),
                           global_data.ss_ussd_property_change_watch_id);
  if (ret)
    {
      syslog(LOG_ERR,
             "unregister ss ussd property change fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  ret = tapi_ss_unregister(
      get_tapi_ctx(), global_data.ss_ussd_request_received_watch_id);
  if (ret)
    {
      syslog(LOG_ERR,
             "unregister ss ussd request received change fail in %s, "
             "ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

int tapi_ss_listen_error_code_test(int slot_id)
{
  int ret1 = tapi_ss_register(get_tapi_ctx(), slot_id, 46, NULL,
                              ss_signal_change);

  int ret2 = tapi_ss_register(get_tapi_ctx(), slot_id, 52, NULL,
                              ss_signal_change);

  return ret1 >= 0 || ret2 >= 0;
}

int tapi_call_dial_test(int slot_id, char *phone_number,
                        int hide_caller_id)
{
  int res = 0;
  test_case_data_init();
  judge_data_init();
  judge_data.expect = EVENT_REQUEST_DIAL_DONE;
  int ret = tapi_call_dial(get_tapi_ctx(), slot_id, phone_number,
                           hide_caller_id, EVENT_REQUEST_DIAL_DONE,
                           tele_call_async_fun);

  if (ret)
    {
      syslog(LOG_ERR, "tapi_call_dial_test execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_DEBUG, "tapi_call_dial_test is not executed in %s",
             __func__);
      res = -1;
      goto on_exit;
    }

  if (judge_data.result)
    {
      syslog(LOG_ERR, "async result is invalid in %s", __func__);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

int tapi_call_hanup_current_call_test(int slot_id)
{
  syslog(LOG_DEBUG, "%s called, current call id: %s\n", __func__,
         test_case_data.call_id);

  if (test_case_data.call_id[0] == 0)
    {
      syslog(LOG_DEBUG, "no call to hanup\n");
      return -1;
    }

  int res = 0;
  judge_data_init();
  judge_data.expect = CALL_LOCAL_HANGUP;
  int ret = tapi_call_hangup_by_id(get_tapi_ctx(), slot_id,
                                   test_case_data.call_id);

  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_call_hanup_current_call_test execute fail in %s, "
             "ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_DEBUG,
             "tapi_call_hanup_current_call_test is not executed in %s",
             __func__);
      res = -1;
      goto on_exit;
    }

  if (judge_data.result)
    {
      syslog(LOG_ERR, "async result is invalid in %s", __func__);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

int tapi_call_unlisten_call_test(void)
{
  int ret = -1;
  int res = 0;
  ret = tapi_unregister(get_tapi_ctx(), global_data.call_state_watch_id);
  if (ret)
    {
      syslog(LOG_ERR, "unregister call state change fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  ret = tapi_unregister(get_tapi_ctx(),
                        global_data.call_emergencylist_change_watch_id);
  if (ret)
    {
      syslog(LOG_ERR,
             "unregister emergency list change fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  ret = tapi_unregister(get_tapi_ctx(),
                        global_data.call_ring_back_tone_change_watch_id);
  if (ret)
    {
      syslog(LOG_ERR,
             "unregister ring back tone change fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

static void call_list_query_complete(tapi_async_result *result)
{
  tapi_call_info *call_info;

  syslog(LOG_DEBUG, "%s : \n", __func__);
  if (result->status != OK)
    return;

  syslog(LOG_DEBUG, "call count: %d \n\n", result->arg2);
  if (judge_data.expect == GET_ALL_CALLS)
    {
      judge_data.result = 0;
      test_case_data.call_count = result->arg2;
      judge_data.flag = judge_data.expect;
    }
  else if (judge_data.expect == GET_TWO_CALL_STATES)
    {
      judge_data.result = 0;
      test_case_data.two_call_state_sum = 0;
      judge_data.flag = judge_data.expect;
    }

  call_info = result->data;

  for (int i = 0; i < result->arg2; i++)
    {
      test_case_data.two_call_state_sum += call_info[i].state;

      if (judge_data.expect == GET_HOLD_CALL_ID)
        {
          if (call_info[i].state == 1)
            {
              char *hold_call_id = call_info[i].call_id;
              strncpy(test_case_data.hold_call_id, hold_call_id,
                      strlen(hold_call_id));
              test_case_data.hold_call_id[strlen(hold_call_id)] = '\0';
              judge_data.result = 0;
              judge_data.flag = judge_data.expect;
            }
        }

      if (judge_data.expect == GET_CURRENT_CALL_STATE)
        {
          test_case_data.current_call_state = call_info[i].state;
          judge_data.result = 0;
          judge_data.flag = judge_data.expect;
        }

      syslog(LOG_DEBUG, "call id: %s \n", call_info[i].call_id);
      syslog(LOG_DEBUG, "call state: %d \n", call_info[i].state);
      syslog(LOG_DEBUG, "call IncomingLine: %s \n",
             call_info[i].incoming_line);
      syslog(LOG_DEBUG, "call Name: %s \n", call_info[i].name);
      syslog(LOG_DEBUG, "call StartTime: %s \n",
             call_info[i].start_time);
      syslog(LOG_DEBUG, "call Multiparty: %d \n",
             call_info[i].multiparty);
      syslog(LOG_DEBUG, "call RemoteHeld: %d \n",
             call_info[i].remote_held);
      syslog(LOG_DEBUG, "call RemoteMultiparty: %d \n",
             call_info[i].remote_multiparty);
      syslog(LOG_DEBUG, "call Information: %s \n", call_info[i].info);
      syslog(LOG_DEBUG, "call Icon: %d \n", call_info[i].icon);
      syslog(LOG_DEBUG, "call Emergency: %d \n\n",
             call_info[i].is_emergency_number);
    }
}

int tapi_get_call_count(int slot_id)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = GET_ALL_CALLS;
  test_case_data.call_count = 0;
  int ret = tapi_call_get_all_calls(get_tapi_ctx(), slot_id, 0,
                                    call_list_query_complete);

  if (ret)
    {
      syslog(LOG_ERR, "tapi_get_call_count execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_DEBUG, "tapi_get_call_count is not executed in %s",
             __func__);
      res = -1;
      goto on_exit;
    }

  if (judge_data.result)
    {
      syslog(LOG_ERR, "async result is invalid in %s", __func__);
      res = -1;
      goto on_exit;
    }

  return test_case_data.call_count;

on_exit:
  return res;
}

static int tapi_get_two_call_state(int slot_id)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = GET_TWO_CALL_STATES;
  test_case_data.two_call_state_sum = -22;
  int ret = tapi_call_get_all_calls(get_tapi_ctx(), slot_id, 0,
                                    call_list_query_complete);

  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_get_two_call_state execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_DEBUG, "tapi_get_two_call_state is not executed in %s",
             __func__);
      res = -1;
      goto on_exit;
    }

  if (judge_data.result)
    {
      syslog(LOG_ERR, "async result is invalid in %s", __func__);
      res = -1;
      goto on_exit;
    }

  return test_case_data.two_call_state_sum;

on_exit:
  return res;
}

static char *get_hold_call_id(int slot_id)
{
  judge_data_init();
  judge_data.expect = GET_HOLD_CALL_ID;
  int ret = tapi_call_get_all_calls(get_tapi_ctx(), slot_id, 0,
                                    call_list_query_complete);

  if (ret)
    {
      syslog(LOG_ERR, "get_hold_call_id execute fail in %s, ret: %d",
             __func__, ret);
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_DEBUG, "get_hold_call_id is not executed in %s",
             __func__);
      goto on_exit;
    }

  if (judge_data.result)
    {
      syslog(LOG_ERR, "async result is invalid in %s", __func__);
      goto on_exit;
    }

  return test_case_data.hold_call_id;

on_exit:
  return NULL;
}

static int get_current_call_state_test(int slot_id)
{
  int res = 0;
  test_case_data.current_call_state = -2;
  judge_data_init();
  judge_data.expect = GET_CURRENT_CALL_STATE;
  int ret = tapi_call_get_all_calls(get_tapi_ctx(), slot_id, 0,
                                    call_list_query_complete);

  if (ret)
    {
      syslog(LOG_ERR,
             "get_current_call_state_test execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_DEBUG,
             "get_current_call_state_test is not executed in %s",
             __func__);
      res = -1;
      goto on_exit;
    }

  if (judge_data.result)
    {
      syslog(LOG_ERR, "async result is invalid in %s", __func__);
      res = -1;
      goto on_exit;
    }

  return test_case_data.current_call_state;

on_exit:
  return res;
}

int tapi_call_hangup_all_test(int slot_id)
{
  int ret = tapi_call_hangup_all_calls(get_tapi_ctx(), slot_id);
  syslog(LOG_DEBUG, "%s, slotId : %d ret : %d \n", __func__, slot_id,
         ret);

  return ret;
}

int tapi_call_dial_using_phone_number_with_area_code_test(int slot_id)
{
  int ret = tapi_call_dial_test(slot_id, "02510086", 0);
  syslog(LOG_DEBUG, "%s, slotId : %d ret : %d \n", __func__, slot_id,
         ret);
  return ret;
}

int tapi_call_dial_using_phone_number_with_pause_code_test(int slot_id)
{
  int ret = tapi_call_dial_test(slot_id, "10086,001", 0);
  return ret;
}

int tapi_call_dial_using_phone_number_with_wait_code_test(int slot_id)
{
  int ret = tapi_call_dial_test(slot_id, "10086;001", 0);
  return ret;
}

int tapi_call_dial_using_phone_number_with_numerous_code_test(
    int slot_id)
{
  int ret = tapi_call_dial_test(slot_id, "10086,001;001", 0);
  return ret;
}

int tapi_call_load_ecc_list_test(int slot_id)
{
  ecc_info out[MAX_ECC_LIST_SIZE];
  int size = tapi_call_get_ecc_list(get_tapi_ctx(), slot_id, out);

  if (size <= 0)
      return -1;

  int ret = 0;
  syslog(LOG_INFO, "ecc list: \n");

  for (int i = 0; i < size; i++)
    {
      syslog(LOG_DEBUG, "ecc number : %s,%u,%u \n", out[i].ecc_num,
             out[i].category, out[i].condition);
    }

  char *correct_ecc_list_with_sim_card[] =
  {
    "110", "119", "120", "118", "999", "000", "08", "911", "112"
  };

  char *correct_ecc_list_without_sim_card[] =
  {
    "119", "118", "999", "110", "08",  "000"
  };

  char *error_ecc_list[] =
  {
    "0", "07", "234"
  };

  bool has_sim_card = false;
  tapi_sim_has_icc_card(get_tapi_ctx(), 0, &has_sim_card);

  if (has_sim_card)
    {
      for (int i = 0;
           i < sizeof(correct_ecc_list_with_sim_card) / sizeof(char *);
           i++)
        {
          if (tapi_call_is_emergency_number(
                  get_tapi_ctx(), correct_ecc_list_with_sim_card[i]) ==
              -1)
            {
              syslog(LOG_ERR, "%s is not in ecc list\n",
                     correct_ecc_list_with_sim_card[i]);
              ret |= -1;
            }
        }
    }
  else
    {
      for (int i = 0; i < sizeof(correct_ecc_list_without_sim_card) /
                              sizeof(char *);
           i++)
        {
          if (tapi_call_is_emergency_number(
                  get_tapi_ctx(),
                  correct_ecc_list_without_sim_card[i]) == -1)
            {
              syslog(LOG_ERR, "%s is not in ecc list\n",
                     correct_ecc_list_without_sim_card[i]);
              ret |= -1;
            }
        }
    }

  for (int i = 0; i < sizeof(error_ecc_list) / sizeof(char *); i++)
    {
      if (tapi_call_is_emergency_number(get_tapi_ctx(),
                                        error_ecc_list[i]) != -1)
        {
          syslog(LOG_ERR, "%s is not emergency number\n",
                 error_ecc_list[i]);
          ret |= -1;
        }
    }

  return ret;
}

int tapi_call_set_default_voicecall_slot_test(int slot_id)
{
  int res = 0;
  judge_data_init();
  global_data.default_voicecall_slot = -100;
  judge_data.expect = MSG_DEFAULT_VOICECALL_SLOT_CHANGE_IND;

  int ret = tapi_call_set_default_slot(get_tapi_ctx(), slot_id);
  if (ret)
    {
      syslog(LOG_ERR, "tapi_call_set_default_slot execute fail, ret: %d",
             ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR,
             "tele_call_manager_call_async_fun was not executed in %s",
             __func__);
      res = -1;
      goto on_exit;
    }

  if (global_data.default_voicecall_slot != slot_id)
    {
      syslog(LOG_ERR, "async result is invalid in %s", __func__);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

int tapi_call_get_default_voicecall_slot_test(int expect_res)
{
  int result = -1;
  int ret = tapi_call_get_default_slot(get_tapi_ctx(), &result);
  syslog(LOG_DEBUG, "%s, ret: %d, voicecall_slot: %d", __func__, ret,
         result);

  return ret || expect_res != result;
}

int tapi_call_answer_call_test(int slot_id, char *call_id)
{
  if (test_case_data.call_id[0] == 0)
    {
      syslog(LOG_ERR, "current call id is NULL\n");
      return -1;
    }

  int res = 0;
  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_ACTIVE;
  int ret = tapi_call_answer_by_id(get_tapi_ctx(), slot_id, call_id);

  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_call_answer_call_test execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_DEBUG,
             "tapi_call_answer_call_test is not executed in %s",
             __func__);
      res = -1;
      goto on_exit;
    }

  if (judge_data.result)
    {
      syslog(LOG_ERR, "async result is invalid in %s", __func__);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

int call_dial_after_caller_reject(int slot_id)
{
  int ret1;
  int ret2;
  int ret3;
  int ret4;
  int ret5;
  ret1 = ret2 = ret3 = ret4 = ret5 = -1;

  for (int i = 0; i < 5; ++i)
    {
      ret1 = tapi_call_listen_call_test(slot_id);
      ret2 = tapi_call_dial_test(slot_id, phone_num, 0);

      judge_data_init();
      judge_data.expect = CALL_REMOTE_HANGUP;

      int integer_call_id = atoi(test_case_data.call_id + 16);
      hangup_remote_call(integer_call_id);

      if (judge() || judge_data.result || ret1 || ret2)
          goto error;

      ret3 = tapi_call_dial_test(slot_id, phone_num, 0);
      ret4 = tapi_call_hanup_current_call_test(slot_id);
      ret5 = tapi_call_unlisten_call_test();

      if ((ret3 || ret4 || ret5) != 0)
        {
          goto error;
        }
    }

  return 0;

error:
  if (tapi_get_call_count(slot_id))
    {
      tapi_call_hangup_all_test(slot_id);
    }

  return -1;
}

int call_hangup_after_caller_answer(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  int ret2 = tapi_call_dial_test(slot_id, phone_num, 0);
  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_ACTIVE;

  if ((judge() || ret1 || ret2 || judge_data.result) != 0)
      goto error;

  int ret4 = tapi_call_hanup_current_call_test(slot_id);
  int ret5 = tapi_call_unlisten_call_test();

  if ((ret4 || ret5) != 0)
      goto error;

  return 0;

error:
  if (tapi_get_call_count(slot_id))
    {
      tapi_call_hangup_all_test(slot_id);
    }

  return -1;
}

int call_dial_active_and_hangup_by_caller(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  int ret2 = tapi_call_dial_test(slot_id, phone_num, 0);
  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_ACTIVE;

  if ((judge() || ret1 || ret2 || judge_data.result) != 0)
      goto error;

  judge_data_init();
  judge_data.expect = CALL_REMOTE_HANGUP;

  int integer_call_id = atoi(test_case_data.call_id + 16);
  hangup_remote_call(integer_call_id);

  if (judge())
      return -1;

  int ret3 = tapi_call_unlisten_call_test();
  if ((judge_data.result || ret3) != 0)
      goto error;

  return 0;

error:
  if (tapi_get_call_count(slot_id))
    {
      tapi_call_hangup_all_test(slot_id);
    }

  return -1;
}

int call_dial_caller_reject_and_incoming(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  int ret2 = tapi_call_dial_test(slot_id, phone_num, 0);

  judge_data_init();
  judge_data.expect = CALL_REMOTE_HANGUP;

  int integer_call_id = atoi(test_case_data.call_id + 16);
  hangup_remote_call(integer_call_id);

  if ((judge() || ret1 || ret2 || judge_data.result) != 0)
      goto error;

  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  incoming_call("10010");

  if (judge() || judge_data.result)
      goto error;

  int ret3 = tapi_call_hanup_current_call_test(slot_id);
  int ret4 = tapi_call_unlisten_call_test();

  if ((ret3 || ret4) != 0)
      goto error;

  return 0;

error:
  if (tapi_get_call_count(slot_id))
    {
      tapi_call_hangup_all_test(slot_id);
    }

  return -1;
}

int call_dial_caller_reject_and_dial_another(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  int ret2 = tapi_call_dial_test(slot_id, phone_num, 0);
  judge_data_init();
  judge_data.expect = CALL_REMOTE_HANGUP;

  int integer_call_id = atoi(test_case_data.call_id + 16);
  hangup_remote_call(integer_call_id);

  if ((judge() || ret1 || ret2 || judge_data.result) != 0)
      goto error;

  int ret3 = tapi_call_dial_test(slot_id, "10001", 0);
  int ret4 = tapi_call_hanup_current_call_test(slot_id);
  int ret5 = tapi_call_unlisten_call_test();

  if ((ret3 || ret4 || ret5) != 0)
      goto error;

  return 0;

error:
  if (tapi_get_call_count(slot_id))
    {
      tapi_call_hangup_all_test(slot_id);
    }

  return -1;
}

int call_hangup_between_dialing_and_answering(int slot_id)
{
  int res = 0;
  int ret = tapi_call_dial_test(slot_id, phone_num, 0);
  if (ret)
    {
      syslog(LOG_ERR, "tapi_call_dial_test execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  ret = tapi_call_hanup_current_call_test(slot_id);
  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_call_hanup_current_call_test execute fail in %s, "
             "ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

int call_clear_voicecall_slot_set(void)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = MSG_DEFAULT_VOICECALL_SLOT_CHANGE_IND;
  global_data.default_voicecall_slot = -100;
  int ret = tapi_call_set_default_voicecall_slot_test(-1);

  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_call_set_default_voicecall_slot_test execute fail, "
             "ret: %d",
             ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR,
             "tele_call_manager_call_async_fun was not executed in %s",
             __func__);
      res = -1;
      goto on_exit;
    }

  if (global_data.default_voicecall_slot != -1)
    {
      syslog(LOG_ERR, "async result is invalid in %s", __func__);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

/* todo */

int call_dial_to_phone_in_call(int slot_id)
{
  /* todo: caller in process */

  int ret1 = tapi_call_listen_call_test(slot_id);
  int ret2 = tapi_call_dial_test(slot_id, phone_num, 0);
  sleep(5);
  int ret3 = tapi_call_hanup_current_call_test(slot_id);
  int ret4 = tapi_call_unlisten_call_test();

  return ret1 || ret2 || ret3 || ret4;
}

/* todo */

int call_dial_to_phone_out_of_service(int slot_id)
{
  /* todo: caller out of service */

  int ret1 = tapi_call_listen_call_test(slot_id);
  int ret2 = tapi_call_dial_test(slot_id, phone_num, 0);
  sleep(5);
  int ret3 = tapi_call_hanup_current_call_test(slot_id);
  int ret4 = tapi_call_unlisten_call_test();

  return ret1 || ret2 || ret3 || ret4;
}

/* todo */

int call_dial_without_sim_card(int slot_id)
{
  /* todo: pull out the sim card */

  int ret1 = tapi_call_listen_call_test(slot_id);
  int ret2 = tapi_call_dial_test(slot_id, phone_num, 0);
  sleep(5);
  int ret3 = tapi_call_hanup_current_call_test(slot_id);
  int ret4 = tapi_call_unlisten_call_test();

  return ret1 || ret2 || ret3 || ret4;
}

/* todo */

int call_dial_ecc_number_without_sim_card(int slot_id)
{
  /* todo: pull out the sim card */

  int ret1 = tapi_call_listen_call_test(slot_id);
  int ret2 = tapi_call_dial_test(slot_id, "911", 0);
  sleep(5);
  int ret3 = tapi_call_hanup_current_call_test(slot_id);
  int ret4 = tapi_call_unlisten_call_test();

  return ret1 || ret2 || ret3 || ret4;
}

/* todo */

int call_dial_to_empty_number(int slot_id)
{
  char *empty_phone_number = "123456";

  /* todo: 123456 is empty phone number */

  int ret1 = tapi_call_listen_call_test(slot_id);

  if (ret1 != 0)
      return -1;

  int ret2 = tapi_call_dial_test(slot_id, empty_phone_number, 0);

  if (ret2 == 0)
    {
      tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();
      return -1;
    }

  return tapi_call_unlisten_call_test();
}

int tapi_dial_number(int slot_id)
{
  int ret = tapi_call_dial_test(slot_id, phone_num, 0);
  syslog(LOG_DEBUG, "%s, slotId : %d ret : %d \n", __func__, slot_id,
         ret);
  return ret;
}

int tapi_dial_ecc_number(int slot_id)
{
  int ret = tapi_call_dial_test(slot_id, "911", 0);
  syslog(LOG_DEBUG, "%s, ret: %d", __func__, ret);
  return ret;
}

int tapi_dial_with_long_phone_number(int slot_id)
{
  int ret = tapi_call_dial_test(slot_id, "167101398140", 0);
  syslog(LOG_DEBUG, "%s, slotId : %d ret : %d \n", __func__, slot_id,
         ret);
  return ret;
}

int tapi_dial_with_short_phone_number(int slot_id)
{
  int ret = tapi_call_dial_test(slot_id, "11", 0);
  syslog(LOG_DEBUG, "%s, slotId : %d ret : %d \n", __func__, slot_id,
         ret);
  return ret;
}

int tapi_dial_with_enable_hide_callerid(int slot_id)
{
  int ret = tapi_call_dial_test(slot_id, phone_num, 1);
  syslog(LOG_DEBUG, "%s, slotId : %d ret : %d \n", __func__, slot_id,
         ret);
  return ret;
}

int tapi_dial_with_disabled_hide_callerid(int slot_id)
{
  int ret = tapi_call_dial_test(slot_id, phone_num, 2);
  syslog(LOG_DEBUG, "%s, slotId : %d ret : %d \n", __func__, slot_id,
         ret);
  return ret;
}

int tapi_dial_with_default_hide_callerid(int slot_id)
{
  int ret = tapi_call_dial_test(slot_id, phone_num, 0);
  syslog(LOG_DEBUG, "%s, slotId : %d ret : %d \n", __func__, slot_id,
         ret);
  return ret;
}

int tapi_start_dtmf_test(int slot_id)
{
  int ret = tapi_call_start_dtmf(get_tapi_ctx(), slot_id, '0',
                                 EVENT_REQUEST_START_DTMF_DONE,
                                 tele_call_async_fun);
  syslog(LOG_DEBUG, "%s, ret: %d", __func__, ret);
  return ret;
}

int tapi_stop_dtmf_test(int slot_id)
{
  int ret = tapi_call_stop_dtmf(get_tapi_ctx(), slot_id,
                                EVENT_REQUEST_STOP_DTMF_DONE,
                                tele_call_async_fun);
  syslog(LOG_DEBUG, "%s, ret: %d", __func__, ret);
  return ret;
}

/* todo: unsolicited message error, need gaojiawei fix */

int call_incoming_and_hangup_by_dialer_before_answer(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  incoming_call("10010");

  if ((judge() || ret1 || judge_data.result) != 0)
      goto error;

  judge_data_init();
  judge_data.expect = CALL_REMOTE_HANGUP;

  int integer_call_id = atoi(test_case_data.call_id + 16);
  hangup_remote_call(integer_call_id);

  if (judge() != 0)
      goto error;

  int ret2 = tapi_call_unlisten_call_test();

  if ((ret2 || judge_data.result) != 0)
      goto error;

  return 0;

error:
  if (tapi_get_call_count(slot_id))
    {
      tapi_call_hangup_all_test(slot_id);
    }

  return -1;
}

int call_incoming_and_hangup_by_dialer_before_answer_numerous(
    int slot_id)
{
  for (int i = 0; i < 5; i++)
    {
      int ret1 = tapi_call_listen_call_test(slot_id);
      judge_data_init();
      test_case_data_init();
      judge_data.expect = NEW_CALL_INCOMING;

      /* todo: incoming call from different phone number */

      if (judge() != 0)
          return -1;

      if ((ret1 || judge_data.result) != 0)
          return -1;

      sleep(5);
      judge_data_init();
      judge_data.expect = CALL_REMOTE_HANGUP;

      /* todo: dialer hangup */

      if (judge() != 0)
          return -1;

      int ret2 = tapi_call_unlisten_call_test();

      if ((ret2 || judge_data.result) != 0)
          return -1;
    }

  return 0;
}

int call_incoming_answer_and_hangup(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  if (incoming_call("101") < 0)
      return -1;

  if ((judge() || ret1 || judge_data.result) != 0)
      return -1;

  sleep(2);
  int ret2 = tapi_call_answer_call_test(slot_id, test_case_data.call_id);
  sleep(2);
  int ret3 = tapi_call_hanup_current_call_test(slot_id);
  int ret4 = tapi_call_unlisten_call_test();

  return ret1 || ret2 || ret3 || ret4;
}

/* todo */

int call_incoming_answer_and_hangup_by_dialer(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if (judge() != 0)
      return -1;

  if ((ret1 || judge_data.result) != 0)
      return -1;

  sleep(10);
  int ret2 = tapi_call_answer_call_test(slot_id, test_case_data.call_id);

  judge_data_init();
  judge_data.expect = CALL_REMOTE_HANGUP;

  /* todo: dialer hangup */

  if (judge() != 0)
      return -1;

  int ret3 = tapi_call_unlisten_call_test();

  if ((ret2 || ret3 || judge_data.result) != 0)
      return -1;

  return 0;
}

/* todo */

int call_display_the_network_of_incoming_call(int slot_id,
                                              char *network_name)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = INCOMING_CALL_WITH_NETWORK_NAME;

  /* todo: incoming call with network name */

  if (judge() != 0)
      return -1;

  int ret2 = strncmp(network_name, test_case_data.network_name,
                     strlen(test_case_data.network_name) + 1);

  if ((ret1 || ret2 || judge_data.result) != 0)
      return -1;

  if (test_case_data.call_id[0] != '\0')
    {
      sleep(5);
      int ret3 = tapi_call_hanup_current_call_test(slot_id);
      int ret4 = tapi_call_unlisten_call_test();

      if ((ret3 || ret4) != 0)
          return -1;
    }

  return tapi_call_unlisten_call_test();
}

/* todo */

int call_display_the_network_of_incoming_call_in_call_process(
    int slot_id, char *network_name)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if ((judge() || ret1 || judge_data.result) != 0)
      return -1;

  sleep(10);
  int ret2 = tapi_call_answer_call_test(slot_id, test_case_data.call_id);
  sleep(10);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = INCOMING_CALL_WITH_NETWORK_NAME;

  /* todo: incoming call with network name */

  if ((judge() || ret2 || judge_data.result) != 0)
      return -1;

  int ret3 = strncmp(network_name, test_case_data.network_name,
                     strlen(test_case_data.network_name) + 1);

  if (ret3 != 0)
      return -1;

  if (tapi_get_call_count(slot_id) != 0)
    {
      sleep(5);
      int ret4 = tapi_call_hangup_all_test(slot_id);
      int ret5 = tapi_call_unlisten_call_test();

      if ((ret4 || ret5) != 0)
          return -1;
    }

  return tapi_call_unlisten_call_test();
}

/* todo */

int call_dial_active_hangup_due_to_dialer_network_exception(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  int ret2 = tapi_call_dial_test(slot_id, phone_num, 0);
  sleep(5);

  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_ACTIVE;

  /* todo: caller answer */

  if ((judge() || ret1 || ret2 || judge_data.result) != 0)
      return -1;

  sleep(5);
  judge_data_init();
  judge_data.expect = HANGUP_DUE_TO_NETWORK_EXCEPTION;

  /* todo: hangup due to dialer network exception */

  if ((judge() || judge_data.result) != 0)
    {
      if (tapi_get_call_count(slot_id) != 0)
        tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();

      return -1;
    }

  return tapi_call_unlisten_call_test();
}

/* todo */

int call_dial_active_hangup_due_to_caller_network_exception(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  int ret2 = tapi_call_dial_test(slot_id, phone_num, 0);
  sleep(5);

  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_ACTIVE;

  /* todo: caller answer */

  if ((judge() || ret1 || ret2 || judge_data.result) != 0)
      return -1;

  sleep(5);
  judge_data_init();
  judge_data.expect = HANGUP_DUE_TO_NETWORK_EXCEPTION;

  /* todo: hangup due to caller network exception */

  if ((judge() || judge_data.result) != 0)
    {
      if (tapi_get_call_count(slot_id) != 0)
        tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();

      return -1;
    }

  return tapi_call_unlisten_call_test();
}

/* todo 30 */

int call_check_status_in_dialing(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  int ret2 = tapi_call_dial_test(slot_id, phone_num, 0);
  sleep(5);

  int call_state = get_current_call_state_test(slot_id);

  if ((ret1 || ret2) != 0 || call_state != CALL_STATUS_DIALING)
    {
      tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();
      return -1;
    }

  int ret3 = tapi_call_hangup_all_test(slot_id);
  int ret4 = tapi_call_unlisten_call_test();

  return ret3 || ret4;
}

/* todo 31 */

int call_dial_and_check_status_in_call_process(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  int ret2 = tapi_call_dial_test(slot_id, phone_num, 0);
  sleep(5);
  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_ACTIVE;

  /* todo: caller answer */

  if ((ret1 || ret2 || judge() || judge_data.result) != 0)
      return -1;

  sleep(5);
  int call_state = get_current_call_state_test(slot_id);

  if (call_state != CALL_STATUS_ACTIVE)
    {
      tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();
      return -1;
    }

  int ret3 = tapi_call_hangup_all_test(slot_id);
  int ret4 = tapi_call_unlisten_call_test();

  return ret3 || ret4;
}

/* todo 32 */

int call_incoming_answer_and_check_status_in_call_process(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if ((ret1 || judge() || judge_data.result) != 0)
      return -1;

  sleep(10);
  int ret2 = tapi_call_answer_call_test(slot_id, test_case_data.call_id);
  sleep(10);
  int call_state = get_current_call_state_test(slot_id);

  if (ret2 || call_state != CALL_STATUS_ACTIVE)
    {
      tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();
      return -1;
    }

  int ret3 = tapi_call_hangup_all_test(slot_id);
  int ret4 = tapi_call_unlisten_call_test();

  return ret3 || ret4;
}

/* todo 33 */

int call_check_status_in_incoming(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if ((ret1 || judge() || judge_data.result) != 0)
      return -1;

  sleep(10);
  int call_state = get_current_call_state_test(slot_id);

  if (call_state != CALL_STATUS_INCOMING)
    {
      tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();
      return -1;
    }

  int ret2 = tapi_call_hangup_all_test(slot_id);
  int ret3 = tapi_call_unlisten_call_test();

  return ret2 || ret3;
}

/* todo 34 */

int call_incoming_answer_caller_hold_and_check_status(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if ((ret1 || judge() || judge_data.result) != 0)
      return -1;
  sleep(10);
  int ret2 = tapi_call_answer_call_test(slot_id, test_case_data.call_id);
  sleep(10);
  int ret3 = tapi_call_hold_call(get_tapi_ctx(), slot_id);
  sleep(10);
  int call_state = get_current_call_state_test(slot_id);

  if ((ret2 || ret3) != 0 || call_state != CALL_STATUS_HELD)
    {
      tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();
      return -1;
    }

  int ret4 = tapi_call_hangup_all_test(slot_id);
  int ret5 = tapi_call_unlisten_call_test();

  return ret4 || ret5;
}

/* todo: 35 */

int call_incoming_answer_dialer_hold_and_check_status(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if ((ret1 || judge() || judge_data.result) != 0)
      return -1;
  sleep(10);
  int ret2 = tapi_call_answer_call_test(slot_id, test_case_data.call_id);
  sleep(10);
  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_HOLD;

  /* todo: dialer hold */

  sleep(10);
  int call_state = get_current_call_state_test(slot_id);

  if (ret2 != 0 || call_state != CALL_STATUS_HELD)
    {
      tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();
      return -1;
    }

  int ret3 = tapi_call_hangup_all_test(slot_id);
  int ret4 = tapi_call_unlisten_call_test();

  return ret3 || ret4;
}

/* todo: 36 */

int call_check_call_status_in_multi_call(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if ((ret1 || judge() || judge_data.result) != 0)
      return -1;
  sleep(5);
  int ret2 = tapi_call_answer_call_test(slot_id, test_case_data.call_id);
  sleep(10);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_WAITING;

  /* todo: another incoming call */

  if ((judge() || judge_data.result || ret2) != 0)
      return -1;

  sleep(10);
  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_ACTIVE;
  int ret3 = tapi_call_hold_and_answer(get_tapi_ctx(), slot_id);

  if ((judge() || ret3 || judge_data.result) != 0)
    {
      tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();
      return -1;
    }

  if (tapi_get_call_count(slot_id) != 2 ||
      tapi_get_two_call_state(slot_id) != 1)
    {
      tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();
      return -1;
    }

  int ret4 = tapi_call_hangup_all_test(slot_id);
  int ret5 = tapi_call_unlisten_call_test();

  return ret4 || ret5;
}

/* todo: 37 */

int call_check_call_status_in_call_waiting_with_multi_call(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if ((ret1 || judge() || judge_data.result) != 0)
      return -1;
  sleep(5);
  int ret2 = tapi_call_answer_call_test(slot_id, test_case_data.call_id);
  sleep(10);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_WAITING;

  /* todo: another incoming call */

  if ((ret2 || judge() || judge_data.result) != 0)
      return -1;

  sleep(10);

  if (tapi_get_call_count(slot_id) != 2 ||
      tapi_get_two_call_state(slot_id) != 5)
    {
      tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();
      return -1;
    }

  int ret3 = tapi_call_hangup_all_test(slot_id);
  int ret4 = tapi_call_unlisten_call_test();

  return ret3 || ret4;
}

/* todo: 38 */

int call_check_call_status_in_dialing_with_multi_call(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if ((ret1 || judge() || judge_data.result) != 0)
      return -1;
  sleep(5);
  int ret2 = tapi_call_answer_call_test(slot_id, test_case_data.call_id);
  sleep(10);
  int ret3 = tapi_call_dial_test(slot_id, phone_num, 0);

  if ((ret2 || ret3 || judge()) != 0)
      return -1;

  sleep(5);

  if (tapi_get_call_count(slot_id) != 2 ||
      tapi_get_two_call_state(slot_id) != 3)
    {
      tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();
      return -1;
    }

  int ret4 = tapi_call_hangup_all_test(slot_id);
  int ret5 = tapi_call_unlisten_call_test();

  return ret4 || ret5;
}

/* todo: 39 */

int call_hold_old_call_incoming_new_call_and_check_status(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if ((ret1 || judge() || judge_data.result) != 0)
      return -1;
  sleep(5);
  int ret2 = tapi_call_answer_call_test(slot_id, test_case_data.call_id);
  sleep(10);

  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_HOLD;
  int ret3 = tapi_call_hold_call(get_tapi_ctx(), slot_id);

  if ((ret2 || ret3 || judge() || judge_data.result) != 0)
      return -1;

  judge_data_init();
  judge_data.expect = NEW_CALL_WAITING;

  /* todo: another incoming call */

  if ((judge() || judge_data.result) != 0)
      return -1;

  sleep(10);
  if (tapi_get_call_count(slot_id) != 2 ||
      tapi_get_two_call_state(slot_id) != 5)
    {
      tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();
      return -1;
    }

  int ret4 = tapi_call_hangup_all_test(slot_id);
  int ret5 = tapi_call_unlisten_call_test();

  return ret4 || ret5;
}

/* todo: 42 */

int call_incoming_hangup_first_answer_second(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if ((judge() || ret1 || judge_data.result) != 0)
      return -1;

  sleep(5);
  if (tapi_call_answer_call_test(slot_id, test_case_data.call_id) != 0)
      return -1;

  char old_call_id[101];
  strncpy(old_call_id, test_case_data.call_id,
          strlen(test_case_data.call_id) + 1);

  sleep(5);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_WAITING;

  /* todo: incoming another call */

  if ((judge() || judge_data.result) != 0)
      return -1;

  char new_call_id[101];
  strncpy(new_call_id, test_case_data.call_id,
          strlen(test_case_data.call_id) + 1);

  judge_data_init();
  judge_data.expect = CALL_LOCAL_HANGUP;
  int ret2 =
      tapi_call_hangup_by_id(get_tapi_ctx(), slot_id, old_call_id);

  if ((judge() || ret2 || judge_data.result) != 0)
      return -1;

  sleep(5);
  if (tapi_call_answer_call_test(slot_id, new_call_id) != 0)
      return -1;

  sleep(5);
  int ret3 = tapi_call_hangup_all_test(slot_id);
  int ret4 = tapi_call_unlisten_call_test();

  return ret3 || ret4;
}

/* todo: 43 */

int call_release_and_answer(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if (judge() != 0 || judge_data.result != 0 || ret1)
      return -1;

  sleep(5);
  if (tapi_call_answer_call_test(slot_id, test_case_data.call_id) != 0)
      return -1;

  sleep(5);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_WAITING;

  /* todo: incoming another call */

  if (judge() != 0 || judge_data.result != 0)
      return -1;

  sleep(5);
  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_ACTIVE;
  int ret2 = tapi_call_release_and_answer(get_tapi_ctx(), slot_id);

  if ((judge() || judge_data.result || ret2) != 0)
      return -1;

  sleep(5);
  int ret3 = tapi_call_hangup_all_test(slot_id);
  int ret4 = tapi_call_unlisten_call_test();

  return ret3 || ret4;
}

/* todo: 44 */

int call_incoming_hold_and_recover_by_dialer(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  sleep(5);

  if ((judge() || ret1 || judge_data.result) != 0)
      return -1;

  sleep(5);
  int ret2 = tapi_call_answer_call_test(slot_id, test_case_data.call_id);
  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_HOLD;

  /* todo: dialer hold */

  if ((ret2 || judge() || judge_data.result) != 0)
      return -1;

  sleep(5);
  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_ACTIVE;

  /* todo: dialer active */

  if (judge() != 0 || judge_data.result != 0)
      return -1;

  sleep(5);
  int ret3 = tapi_call_hangup_all_test(slot_id);
  int ret4 = tapi_call_unlisten_call_test();

  return ret3 || ret4;
}

/* todo: 45 */

int call_dialer_hold_recover_and_hold_by_caller(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  int ret2 = tapi_call_dial_test(slot_id, phone_num, 0);

  sleep(5);
  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_ACTIVE;

  /* todo: caller answer */

  if ((judge() || ret1 || ret2 || judge_data.result) != 0)
      return -1;

  sleep(10);
  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_HOLD;
  int ret3 = tapi_call_hold_call(get_tapi_ctx(), slot_id);

  if ((ret3 || judge() || judge_data.result) != 0)
      return -1;

  sleep(10);
  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_ACTIVE;
  int ret4 = tapi_call_unhold_call(get_tapi_ctx(), slot_id);

  if ((ret4 || judge() || judge_data.result) != 0)
      return -1;

  sleep(10);
  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_HOLD;

  /* todo: caller hold */

  if ((judge() || judge_data.result) != 0)
      return -1;

  int ret5 = tapi_call_hangup_all_test(slot_id);
  int ret6 = tapi_call_unlisten_call_test();

  return ret5 || ret6;
}

/* todo: 47 */

int call_incoming_hold_and_recover_by_caller(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if ((ret1 || judge() || judge_data.result) != 0)
      return -1;

  sleep(5);
  if (tapi_call_answer_call_test(slot_id, test_case_data.call_id))
      return -1;
  sleep(5);

  for (int i = 0; i < 3; i++)
    {
      sleep(5);
      judge_data_init();
      judge_data.expect = CALL_STATE_CHANGE_TO_HOLD;
      int ret2 = tapi_call_hold_call(get_tapi_ctx(), slot_id);

      if ((ret2 || judge() || judge_data.result) != 0)
          return -1;

      sleep(5);
      judge_data_init();
      judge_data.expect = CALL_STATE_CHANGE_TO_ACTIVE;
      int ret3 = tapi_call_unhold_call(get_tapi_ctx(), slot_id);

      if ((ret3 || judge() || judge_data.result) != 0)
          return -1;
    }

  int ret4 = tapi_call_hangup_all_test(slot_id);
  int ret5 = tapi_call_unlisten_call_test();

  return ret4 || ret5;
}

/* todo: 48 */

int call_hold_and_hangup(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if (judge() != 0 || judge_data.result != 0 || ret1)
      return -1;

  sleep(5);
  if (tapi_call_answer_call_test(slot_id, test_case_data.call_id) != 0)
      return -1;

  sleep(5);
  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_HOLD;
  int ret2 = tapi_call_hold_call(get_tapi_ctx(), slot_id);

  if ((judge() || judge_data.result || ret2) != 0)
      return -1;

  sleep(5);
  int ret3 = tapi_call_hanup_current_call_test(slot_id);
  int ret4 = tapi_call_unlisten_call_test();

  return ret3 || ret4;
}

/* todo: 49 */

int call_swap_dial_reject_swap(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if (judge() != 0 || judge_data.result != 0 || ret1)
      return -1;

  sleep(5);
  if (tapi_call_answer_call_test(slot_id, test_case_data.call_id) != 0)
      return -1;

  sleep(5);
  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_HOLD;
  int ret2 = tapi_call_hold_call(get_tapi_ctx(), slot_id);

  if ((judge() || judge_data.result || ret2) != 0)
      return -1;

  sleep(5);
  if (tapi_call_dial_test(slot_id, phone_num, 0))
      return -1;

  sleep(5);
  judge_data_init();
  judge_data.expect = CALL_REMOTE_HANGUP;

  /* todo: caller reject */

  if ((judge() || judge_data.result) != 0)
      return -1;

  sleep(5);
  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_ACTIVE;
  int ret3 = tapi_call_unhold_call(get_tapi_ctx(), slot_id);

  if ((judge() || judge_data.result || ret3) != 0)
      return -1;

  int ret4 = tapi_call_hangup_all_test(slot_id);
  int ret5 = tapi_call_unlisten_call_test();

  return ret4 || ret5;
}

/* todo: 50 */

int call_hold_incoming_answer_hangup_second_recover_first(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if (judge() != 0 || judge_data.result != 0 || ret1)
      return -1;

  sleep(5);
  if (tapi_call_answer_call_test(slot_id, test_case_data.call_id) != 0)
      return -1;

  sleep(5);
  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_HOLD;
  int ret2 = tapi_call_hold_call(get_tapi_ctx(), slot_id);

  if ((judge() || judge_data.result || ret2) != 0)
      return -1;

  sleep(5);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_WAITING;

  /* todo: incoming another call */

  if (judge() != 0 || judge_data.result != 0)
      return -1;

  sleep(5);
  if (tapi_call_answer_call_test(slot_id, test_case_data.call_id) != 0)
      return -1;

  sleep(10);

  if (tapi_call_hanup_current_call_test(slot_id) != 0)
      return -1;

  sleep(5);
  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_ACTIVE;
  int ret3 = tapi_call_unhold_call(get_tapi_ctx(), slot_id);

  if ((judge() || judge_data.result || ret3) != 0)
      return -1;

  sleep(5);
  int ret4 = tapi_call_hangup_all_test(slot_id);
  int ret5 = tapi_call_unlisten_call_test();

  return ret4 || ret5;
}

/* todo: 51 */

int call_incoming_second_call_swap_answer_hangup_swap(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if (judge() != 0 || judge_data.result != 0 || ret1)
      return -1;

  sleep(10);

  if (tapi_call_answer_call_test(slot_id, test_case_data.call_id) != 0)
      return -1;

  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_WAITING;

  /* todo: incoming another call */

  if (judge() != 0 || judge_data.result != 0)
      return -1;

  sleep(5);
  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_HOLD;
  int ret2 = tapi_call_hold_call(get_tapi_ctx(), slot_id);

  if ((judge() || judge_data.result || ret2) != 0)
      return -1;

  sleep(5);
  if (tapi_call_answer_call_test(slot_id, test_case_data.call_id) != 0)
      return -1;

  sleep(10);

  if (tapi_call_hanup_current_call_test(slot_id) != 0)
      return -1;

  sleep(5);
  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_ACTIVE;
  int ret3 = tapi_call_unhold_call(get_tapi_ctx(), slot_id);

  if ((judge() || judge_data.result || ret3) != 0)
      return -1;

  int ret4 = tapi_call_hangup_all_test(slot_id);
  int ret5 = tapi_call_unlisten_call_test();

  return ret4 || ret5;
}

/* todo: 52 */

int call_hold_current_call_and_reject_new_incoming(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if (judge() != 0 || judge_data.result != 0 || ret1)
      return -1;

  sleep(10);

  if (tapi_call_answer_call_test(slot_id, test_case_data.call_id) != 0)
      return -1;

  sleep(10);

  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_HOLD;
  int ret2 = tapi_call_hold_call(get_tapi_ctx(), slot_id);

  if ((judge() || judge_data.result || ret2) != 0)
      return -1;

  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_WAITING;

  /* todo: incoming another call */

  if (judge() != 0 || judge_data.result != 0)
      return -1;

  sleep(10);

  if (tapi_call_hanup_current_call_test(slot_id) != 0)
      return -1;

  sleep(5);
  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_ACTIVE;
  int ret3 = tapi_call_unhold_call(get_tapi_ctx(), slot_id);

  if ((judge() || judge_data.result || ret3) != 0)
      return -1;

  sleep(5);
  int ret4 = tapi_call_hangup_all_test(slot_id);
  int ret5 = tapi_call_unlisten_call_test();

  return ret4 || ret5;
}

/* todo: 53 */

int call_incoming_and_hangup_new_call(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if (judge() != 0 || judge_data.result != 0 || ret1)
      return -1;

  sleep(10);

  if (tapi_call_answer_call_test(slot_id, test_case_data.call_id) != 0)
      return -1;

  sleep(10);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_WAITING;

  /* todo: incoming new call */

  if (judge() != 0 || judge_data.result != 0)
      return -1;

  sleep(10);

  if (tapi_call_hanup_current_call_test(slot_id) != 0)
      return -1;

  sleep(10);

  if (tapi_get_call_count(slot_id) != 1)
      return -1;

  int ret2 = tapi_call_hangup_all_test(slot_id);
  int ret3 = tapi_call_unlisten_call_test();

  return ret2 || ret3;
}

/* todo: 54 */

int call_hold_first_call_and_answer_second_call(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if (judge() != 0 || judge_data.result != 0 || ret1)
      return -1;

  sleep(10);

  if (tapi_call_answer_call_test(slot_id, test_case_data.call_id) != 0)
      return -1;

  sleep(10);

  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_WAITING;

  /* todo: incoming new call */

  if (judge() != 0 || judge_data.result != 0)
      return -1;

  sleep(10);
  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_ACTIVE;
  int ret2 = tapi_call_hold_and_answer(get_tapi_ctx(), slot_id);
  int ret3 = tapi_get_two_call_state(slot_id);

  if ((judge() || judge_data.result || ret2 || ret3) != 0)
      return -1;

  if (tapi_get_call_count(slot_id) != 2 ||
      tapi_get_two_call_state(slot_id) != 1)
      return -1;

  int ret4 = tapi_call_hangup_all_test(slot_id);
  int ret5 = tapi_call_unlisten_call_test();

  return ret4 || ret5;
}

/* todo: 55 */

int call_dial_second_call_active_and_hangup_by_dialer(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if (judge() != 0 || judge_data.result != 0 || ret1)
      return -1;

  sleep(10);

  if (tapi_call_answer_call_test(slot_id, test_case_data.call_id) != 0)
      return -1;

  sleep(10);

  if (tapi_call_dial_test(slot_id, phone_num, 0))
      return -1;

  sleep(10);
  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_ACTIVE;

  /* todo: caller answer */

  if ((judge() || judge_data.result) != 0)
      return -1;

  sleep(10);

  if (tapi_call_hanup_current_call_test(slot_id) != 0)
      return -1;

  sleep(10);
  if (tapi_get_call_count(slot_id) != 1 ||
      get_current_call_state_test(slot_id) != CALL_STATUS_ACTIVE)
      return -1;

  int ret2 = tapi_call_hangup_all_test(slot_id);
  int ret3 = tapi_call_unlisten_call_test();

  return ret2 || ret3;
}

/* todo: 56 */

int call_dial_second_call_and_hangup_by_caller_before_answering(
    int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if (judge() != 0 || judge_data.result != 0 || ret1)
      return -1;

  sleep(10);

  if (tapi_call_answer_call_test(slot_id, test_case_data.call_id) != 0)
      return -1;

  sleep(10);
  if (tapi_call_dial_test(slot_id, phone_num, 0))
      return -1;

  sleep(10);
  judge_data_init();
  judge_data.expect = CALL_REMOTE_HANGUP;

  /* todo caller reject */

  if ((judge() || judge_data.result) != 0)
      return -1;

  sleep(5);

  if (tapi_get_call_count(slot_id) != 1 ||
      get_current_call_state_test(slot_id) != CALL_STATUS_ACTIVE)
      return -1;

  int ret2 = tapi_call_hangup_all_test(slot_id);
  int ret3 = tapi_call_unlisten_call_test();

  return ret2 || ret3;
}

/* todo: 57 */

int call_dial_second_call_active_and_hangup_by_caller(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if ((ret1 || judge() || judge_data.result) != 0)
      return -1;

  sleep(10);

  if (tapi_call_answer_call_test(slot_id, test_case_data.call_id) != 0)
      return -1;

  sleep(10);

  if (tapi_call_dial_test(slot_id, phone_num, 0))
      return -1;

  sleep(10);

  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_ACTIVE;

  /* todo: caller answer */

  if (judge() || judge_data.result)
      return -1;

  sleep(10);
  judge_data_init();
  judge_data.expect = CALL_REMOTE_HANGUP;

  /* todo: caller hangup */

  if (judge() || judge_data.result)
      return -1;

  sleep(10);
  if (tapi_get_call_count(slot_id) != 1)
      return -1;

  sleep(5);
  int ret2 = tapi_call_hangup_all_test(slot_id);
  int ret3 = tapi_call_unlisten_call_test();

  return ret2 || ret3;
}

/* todo: 58 */

int call_hangup_hold_call_in_two_calls(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if ((ret1 || judge() || judge_data.result) != 0)
      return -1;

  sleep(10);

  if (tapi_call_answer_call_test(slot_id, test_case_data.call_id))
      return -1;

  sleep(10);
  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_HOLD;
  int ret2 = tapi_call_hold_call(get_tapi_ctx(), slot_id);

  if ((judge() || judge_data.result || ret2) != 0)
      return -1;

  sleep(5);
  if (tapi_call_dial_test(slot_id, phone_num, 0))
      return -1;
  sleep(5);
  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_ACTIVE;

  /* todo caller answer */

  if ((judge() || judge_data.result) != 0)
      return -1;

  sleep(5);
  if (tapi_get_call_count(slot_id) != 2)
      return -1;
  char *hold_call_id = get_hold_call_id(slot_id);

  if (hold_call_id == NULL)
      return -1;

  judge_data_init();
  judge_data.expect = CALL_LOCAL_HANGUP;
  int ret3 =
      tapi_call_hangup_by_id(get_tapi_ctx(), slot_id, hold_call_id);

  if ((judge() || judge_data.result || ret3) != 0)
      return -1;

  sleep(10);
  if (tapi_get_call_count(slot_id) != 1)
      return -1;

  sleep(5);
  int ret4 = tapi_call_hangup_all_test(slot_id);
  int ret5 = tapi_call_unlisten_call_test();

  return ret4 || ret5;
}

/* todo: 59 */

int call_swap_in_two_calling(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if ((ret1 || judge() || judge_data.result) != 0)
      return -1;

  sleep(10);

  if (tapi_call_answer_call_test(slot_id, test_case_data.call_id))
      return -1;

  sleep(10);
  judge_data_init();
  judge_data.expect = NEW_CALL_WAITING;

  /* todo: incoming another call */

  if ((judge() || judge_data.result) != 0)
      return -1;

  sleep(10);
  if (tapi_call_answer_call_test(slot_id, test_case_data.call_id))
      return -1;
  sleep(10);

  if (tapi_get_call_count(slot_id) != 2 ||
      tapi_get_two_call_state(slot_id) != 1)
    {
      tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();
      return -1;
    }

  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_HOLD;
  int ret2 = tapi_call_hold_call(get_tapi_ctx(), slot_id);

  if ((ret2 || judge() || judge_data.result) != 0)
    {
      tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();
      return -1;
    }

  if (tapi_get_call_count(slot_id) != 2 ||
      tapi_get_two_call_state(slot_id) != 1)
    {
      tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();
      return -1;
    }

  char old_call_id[101];
  char *call_id = get_hold_call_id(slot_id);

  if (call_id == NULL)
    {
      tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();
      return -1;
    }

  strncpy(old_call_id, call_id, strlen(call_id) + 1);

  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_ACTIVE;
  int ret3 = tapi_call_unhold_call(get_tapi_ctx(), slot_id);

  if ((ret3 || judge() || judge_data.result) != 0)
    {
      tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();
      return -1;
    }

  call_id = get_hold_call_id(slot_id);

  if (call_id == NULL)
    {
      tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();
      return -1;
    }

  if (tapi_get_call_count(slot_id) != 2 ||
      tapi_get_two_call_state(slot_id) != 1)
    {
      tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();
      return -1;
    }

  if (strncmp(old_call_id, call_id, strlen(call_id) + 1) == 0)
    {
      tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();
      return -1;
    }

  int ret4 = tapi_call_hangup_all_test(slot_id);
  int ret5 = tapi_call_unlisten_call_test();

  return ret4 || ret5;
}

/* todo: 60 */

int call_hangup_current_call_and_recover_hold_call(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if ((ret1 || judge() || judge_data.result) != 0)
      return -1;

  sleep(10);

  if (tapi_call_answer_call_test(slot_id, test_case_data.call_id) != 0)
      return -1;
  sleep(10);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_WAITING;

  /* todo: incoming another call */

  if ((judge() || judge_data.result) != 0)
      return -1;

  sleep(10);
  if (tapi_call_answer_call_test(slot_id, test_case_data.call_id) != 0)
      return -1;
  sleep(10);

  if (tapi_get_call_count(slot_id) != 2 ||
      tapi_get_two_call_state(slot_id) != 1)
    {
      tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();
      return -1;
    }

  int ret2 = tapi_call_hanup_current_call_test(slot_id);
  judge_data_init();
  judge_data.expect = CALL_STATE_CHANGE_TO_ACTIVE;
  int ret3 = tapi_call_unhold_call(get_tapi_ctx(), slot_id);

  if ((ret2 || ret3 || judge() || judge_data.result) != 0)
      return -1;

  int ret4 = tapi_call_hangup_all_test(slot_id);
  int ret5 = tapi_call_unlisten_call_test();

  return ret4 || ret5;
}

/* todo: 61 */

int call_hangup_all_call_in_two_calling(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if ((ret1 || judge() || judge_data.result) != 0)
      return -1;

  sleep(10);

  if (tapi_call_answer_call_test(slot_id, test_case_data.call_id) != 0)
      return -1;
  sleep(10);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_WAITING;

  /* todo: incoming another call */

  if ((judge() || judge_data.result) != 0)
      return -1;
  sleep(10);
  if (tapi_call_answer_call_test(slot_id, test_case_data.call_id) != 0)
      return -1;
  sleep(10);

  if (tapi_get_call_count(slot_id) != 2 ||
      tapi_get_two_call_state(slot_id) != 1)
    {
      tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();
      return -1;
    }

  int ret2 = tapi_call_hangup_all_test(slot_id);
  int ret3 = tapi_call_unlisten_call_test();

  return ret2 || ret3;
}

/* todo: 62 */

int incoming_new_call_in_calling_and_hangup_all_call(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if ((ret1 || judge() || judge_data.result) != 0)
      return -1;

  sleep(10);

  if (tapi_call_answer_call_test(slot_id, test_case_data.call_id) != 0)
      return -1;
  sleep(10);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_WAITING;

  /* todo: incoming another call */

  if ((judge() || judge_data.result) != 0)
      return -1;
  sleep(10);
  if (tapi_get_call_count(slot_id) != 2 ||
      tapi_get_two_call_state(slot_id) != 5)
    {
      tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();
      return -1;
    }

  int ret2 = tapi_call_hangup_all_test(slot_id);
  int ret3 = tapi_call_unlisten_call_test();

  return ret2 || ret3;
}

/* todo: 63 */

int call_dial_third_call(int slot_id)
{
  int ret1 = tapi_call_listen_call_test(slot_id);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_INCOMING;

  /* todo: incoming call */

  if ((ret1 || judge() || judge_data.result) != 0)
      return -1;

  sleep(10);

  if (tapi_call_answer_call_test(slot_id, test_case_data.call_id) != 0)
      return -1;
  sleep(10);
  judge_data_init();
  test_case_data_init();
  judge_data.expect = NEW_CALL_WAITING;

  /* todo: incoming another call */

  if ((judge() || judge_data.result) != 0)
      return -1;
  sleep(10);
  if (tapi_call_answer_call_test(slot_id, test_case_data.call_id))
      return -1;
  sleep(10);

  if (tapi_get_call_count(slot_id) != 2 ||
      tapi_get_two_call_state(slot_id) != 1)
    {
      tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();
      return -1;
    }

  int ret2 = tapi_call_dial_test(slot_id, phone_num, 0);

  if (!ret2)
    {
      tapi_call_hangup_all_test(slot_id);
      tapi_call_unlisten_call_test();
      return -1;
    }

  int ret3 = tapi_call_hangup_all_test(slot_id);
  int ret4 = tapi_call_unlisten_call_test();

  return ret3 || ret4;
}

/* 67 */

int call_listen_and_unlisten_ss(int slot_id)
{
  int ret1 = tapi_ss_listen_test(slot_id);
  int ret2 = tapi_ss_unlisten_test();

  return ret1 || ret2;
}

/* 68 */

int call_listen_error_ss_code(int slot_id)
{
  return tapi_ss_listen_error_code_test(slot_id);
}

void set_callback_data(int expect)
{
  judge_data_init();
  judge_data.expect = expect;
}

int judge_callback_result(void)
{
  if ((judge() || judge_data.result) != 0)
      return -1;

  return 0;
}

void incoming_first_call(void)
{
  test_case_data_init();
  set_callback_data(NEW_CALL_INCOMING);
}

void incoming_another_call(void)
{
  test_case_data_init();
  set_callback_data(NEW_CALL_WAITING);
}
