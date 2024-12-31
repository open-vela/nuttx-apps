/****************************************************************************
 * apps/testing/testsuites/kernel/telephony/telephony_ss_test.c
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
#include "telephony_ss_test.h"

extern struct judge_type judge_data;

static struct
{
  int call_barring_property_change_watch_id;
  int ussd_property_change_watch_id;
  int ussd_notification_received_watch_id;
  int ussd_request_received_watch_id;
  char cf_number[64];
  int fdn_enable;
  int cf_type;
  int cw;
} global_data;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

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

int tapi_listen_ss_test(int slot_id)
{
  global_data.call_barring_property_change_watch_id = -1;
  global_data.call_barring_property_change_watch_id = tapi_ss_register(
      get_tapi_ctx(), slot_id, MSG_CALL_BARRING_PROPERTY_CHANGE_IND,
      NULL, ss_signal_change);
  if (global_data.call_barring_property_change_watch_id < 0)
    {
      syslog(LOG_ERR,
             "call barring property change register fail, ret: %d",
             global_data.call_barring_property_change_watch_id);
      return -1;
    }

  global_data.ussd_property_change_watch_id = -1;
  global_data.ussd_property_change_watch_id = tapi_ss_register(
      get_tapi_ctx(), slot_id, MSG_USSD_PROPERTY_CHANGE_IND, NULL,
      ss_signal_change);
  if (global_data.ussd_property_change_watch_id < 0)
    {
      syslog(LOG_ERR, "ussd property change register fail, ret: %d",
             global_data.ussd_property_change_watch_id);
      return -1;
    }

  global_data.ussd_notification_received_watch_id = -1;
  global_data.ussd_notification_received_watch_id = tapi_ss_register(
      get_tapi_ctx(), slot_id, MSG_USSD_NOTIFICATION_RECEIVED_IND, NULL,
      ss_signal_change);
  if (global_data.ussd_notification_received_watch_id < 0)
    {
      syslog(LOG_DEBUG,
             "ussd notification received register fail, ret: %d",
             global_data.ussd_notification_received_watch_id);
      return -1;
    }

  global_data.ussd_request_received_watch_id = -1;
  global_data.ussd_request_received_watch_id = tapi_ss_register(
      get_tapi_ctx(), slot_id, MSG_USSD_REQUEST_RECEIVED_IND, NULL,
      ss_signal_change);
  if (global_data.ussd_request_received_watch_id < 0)
    {
      syslog(LOG_ERR, "ussd request received register fail, ret: %d",
             global_data.ussd_request_received_watch_id);
      return -1;
    }

  return 0;
}

int tapi_unlisten_ss_test(void)
{
  int ret;
  if (global_data.call_barring_property_change_watch_id < 0 ||
      global_data.ussd_property_change_watch_id < 0 ||
      global_data.ussd_notification_received_watch_id < 0 ||
      global_data.ussd_request_received_watch_id < 0)
    {
      return -1;
    }

  ret = -1;
  ret = tapi_ss_unregister(
      get_tapi_ctx(), global_data.call_barring_property_change_watch_id);
  if (ret < 0)
    {
      syslog(LOG_DEBUG,
             "call barring property change unregister fail, ret: %d",
             ret);
      return -1;
    }

  ret = -1;
  ret = tapi_ss_unregister(get_tapi_ctx(),
                           global_data.ussd_property_change_watch_id);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ussd property change unregister fail, ret: %d",
             ret);
      return -1;
    }

  ret = -1;
  ret = tapi_ss_unregister(
      get_tapi_ctx(), global_data.ussd_notification_received_watch_id);
  if (ret < 0)
    {
      syslog(LOG_DEBUG,
             "ussd notification received unregister fail, ret: %d", ret);
      return -1;
    }

  ret = -1;
  ret = tapi_ss_unregister(get_tapi_ctx(),
                           global_data.ussd_request_received_watch_id);
  if (ret < 0)
    {
      syslog(LOG_ERR, "ussd request received unregister fail, ret: %d",
             ret);
      return -1;
    }

  return 0;
}

static void tele_ss_async_fun(tapi_async_result *result)
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

  if (result->msg_id == EVENT_QUERY_ALL_CALL_BARRING_DONE)
    {
      if (judge_data.expect == EVENT_QUERY_ALL_CALL_BARRING_DONE)
        {
          judge_data.result = 0;
          judge_data.flag = EVENT_QUERY_ALL_CALL_BARRING_DONE;
        }
    }

  else if (result->msg_id == EVENT_REQUEST_CALL_BARRING_DONE)
    {
      if (judge_data.expect == EVENT_REQUEST_CALL_BARRING_DONE)
        {
          judge_data.result = 0;
          judge_data.flag = EVENT_REQUEST_CALL_BARRING_DONE;
        }
    }

  else if (result->msg_id == EVENT_CALL_BARRING_PASSWD_CHANGE_DONE)
    {
      if (judge_data.expect == EVENT_CALL_BARRING_PASSWD_CHANGE_DONE)
        {
          judge_data.result = 0;
          judge_data.flag = EVENT_CALL_BARRING_PASSWD_CHANGE_DONE;
        }
    }

  else if (result->msg_id == EVENT_DISABLE_ALL_INCOMING_DONE)
    {
      if (judge_data.expect == EVENT_DISABLE_ALL_INCOMING_DONE)
        {
          judge_data.result = 0;
          judge_data.flag = EVENT_DISABLE_ALL_INCOMING_DONE;
        }
    }

  else if (result->msg_id == EVENT_DISABLE_ALL_OUTGOING_DONE)
    {
      if (judge_data.expect == EVENT_DISABLE_ALL_OUTGOING_DONE)
        {
          judge_data.result = 0;
          judge_data.flag = EVENT_DISABLE_ALL_OUTGOING_DONE;
        }
    }

  else if (result->msg_id == EVENT_DISABLE_ALL_CALL_BARRINGS_DONE)
    {
      if (judge_data.expect == EVENT_DISABLE_ALL_CALL_BARRINGS_DONE)
        {
          judge_data.result = 0;
          judge_data.flag = EVENT_DISABLE_ALL_CALL_BARRINGS_DONE;
        }
    }

  else if (result->msg_id == EVENT_REQUEST_CALL_FORWARDING_DONE)
    {
      if (judge_data.expect == EVENT_REQUEST_CALL_FORWARDING_DONE)
        {
          if (result->arg1 == global_data.cf_type && result->arg2 == 1)
            {
              judge_data.result = 0;
              judge_data.flag = EVENT_REQUEST_CALL_FORWARDING_DONE;
            }
        }
    }

  else if (result->msg_id == EVENT_QUERY_CALL_FORWARDING_DONE)
    {
      tapi_call_forward_info *cf_info = result->data;
      if (judge_data.expect == EVENT_QUERY_CALL_FORWARDING_DONE)
        {
          if (result->arg1 == global_data.cf_type && cf_info != NULL &&
              !strcmp(global_data.cf_number,
                      cf_info->phone_number.number))
            {
              judge_data.result = 0;
              judge_data.flag = EVENT_QUERY_CALL_FORWARDING_DONE;
            }
        }
    }

  else if (result->msg_id == EVENT_REQUEST_CALL_WAITING_DONE)
    {
      if (judge_data.expect == EVENT_REQUEST_CALL_WAITING_DONE)
        {
          if (result->arg2 == global_data.cw)
            {
              judge_data.result = 0;
              judge_data.flag = EVENT_REQUEST_CALL_WAITING_DONE;
            }
        }
    }

  else if (result->msg_id == EVENT_QUERY_CALL_WAITING_DONE)
    {
      if (judge_data.expect == EVENT_QUERY_CALL_WAITING_DONE)
        {
          if (result->arg2 == global_data.cw)
            {
              judge_data.result = 0;
              judge_data.flag = EVENT_QUERY_CALL_WAITING_DONE;
            }
        }
    }

  else if (result->msg_id == EVENT_ENABLE_FDN_DONE)
    {
      if (judge_data.expect == EVENT_ENABLE_FDN_DONE)
        {
          judge_data.result = 0;
          judge_data.flag = EVENT_ENABLE_FDN_DONE;
        }
    }

  else if (result->msg_id == EVENT_QUERY_FDN_DONE)
    {
      if (judge_data.expect == EVENT_QUERY_FDN_DONE)
        {
          if (result->arg2 == global_data.fdn_enable)
            {
              judge_data.result = 0;
              judge_data.flag = EVENT_QUERY_FDN_DONE;
            }
        }
    }
}

int tapi_ss_request_call_barring_test(int slot_id)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_QUERY_ALL_CALL_BARRING_DONE;
  int ret = tapi_ss_request_call_barring(
      get_tapi_ctx(), slot_id, EVENT_QUERY_ALL_CALL_BARRING_DONE,
      tele_ss_async_fun);
  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_ss_request_call_barring execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_ss_async_fun is not executed in %s",
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

int tapi_ss_set_call_barring_option_test(int slot_id, char *facility,
                                         char *pin2)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_REQUEST_CALL_BARRING_DONE;
  int ret = tapi_ss_set_call_barring_option(
      get_tapi_ctx(), slot_id, EVENT_REQUEST_CALL_BARRING_DONE, "AI",
      "1234", tele_ss_async_fun);
  if (ret)
    {
      syslog(
          LOG_ERR,
          "tapi_ss_set_call_barring_option execute fail in %s, ret: %d",
          __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_ss_async_fun is not executed in %s",
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

int tapi_ss_get_call_barring_option_test(int slot_id, char *key,
                                         char *expect)
{
  char *result = NULL;
  int ret = tapi_ss_get_call_barring_option(get_tapi_ctx(), slot_id, key,
                                            &result);
  syslog(LOG_INFO, "%s, ret: %d, result: %s", __func__, ret, result);
  return ret || result == NULL || strcmp(expect, result);
}

int tapi_ss_change_call_barring_password_test(int slot_id,
                                              char *old_passwd,
                                              char *new_passwd)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_CALL_BARRING_PASSWD_CHANGE_DONE;
  int ret = tapi_ss_change_call_barring_password(
      get_tapi_ctx(), slot_id, EVENT_CALL_BARRING_PASSWD_CHANGE_DONE,
      old_passwd, new_passwd, tele_ss_async_fun);
  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_ss_change_call_barring_password execute fail in %s, "
             "ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_ss_async_fun is not executed in %s",
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

int tapi_ss_disable_all_incoming_test(int slot_id, char *passwd)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_DISABLE_ALL_INCOMING_DONE;
  int ret = tapi_ss_disable_all_incoming(get_tapi_ctx(), slot_id,
                                         EVENT_DISABLE_ALL_INCOMING_DONE,
                                         passwd, tele_ss_async_fun);
  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_ss_disable_all_incoming execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_ss_async_fun is not executed in %s",
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

int tapi_ss_disable_all_outgoing_test(int slot_id, char *passwd)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_DISABLE_ALL_OUTGOING_DONE;
  int ret = tapi_ss_disable_all_outgoing(get_tapi_ctx(), slot_id,
                                         EVENT_DISABLE_ALL_OUTGOING_DONE,
                                         passwd, tele_ss_async_fun);
  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_ss_disable_all_outgoing execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_ss_async_fun is not executed in %s",
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

int tapi_ss_disable_all_call_barrings_test(int slot_id, char *passwd)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_DISABLE_ALL_CALL_BARRINGS_DONE;
  int ret = tapi_ss_disable_all_call_barrings(
      get_tapi_ctx(), slot_id, EVENT_DISABLE_ALL_CALL_BARRINGS_DONE,
      passwd, tele_ss_async_fun);
  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_ss_disable_all_call_barrings execute fail in %s, "
             "ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_ss_async_fun is not executed in %s",
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

int tapi_ss_set_call_forwarding_option_test(int slot_id, int cf_type,
                                            char *number)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_REQUEST_CALL_FORWARDING_DONE;
  memset(global_data.cf_number, 0, sizeof(global_data.cf_number));
  strcpy(global_data.cf_number, number);
  global_data.cf_type = cf_type;
  int ret = tapi_ss_set_call_forwarding_option(
      get_tapi_ctx(), slot_id, EVENT_REQUEST_CALL_FORWARDING_DONE,
      cf_type, BEARER_CLASS_VOICE, number, tele_ss_async_fun);

  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_ss_set_call_forwarding_option execute fail in %s, "
             "ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_ss_async_fun is not executed in %s",
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

int tapi_ss_get_call_forwarding_option_test(int slot_id, int cf_type)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_QUERY_CALL_FORWARDING_DONE;
  global_data.cf_type = cf_type;
  int ret = tapi_ss_query_call_forwarding_option(
      get_tapi_ctx(), slot_id, EVENT_QUERY_CALL_FORWARDING_DONE, cf_type,
      BEARER_CLASS_VOICE, tele_ss_async_fun);
  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_ss_query_call_forwarding_option execute fail in %s, "
             "ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_ss_async_fun is not executed in %s",
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

int tapi_ss_set_call_waiting_test(int slot_id, bool enable)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_REQUEST_CALL_WAITING_DONE;
  global_data.cw = (int)enable;
  int ret = tapi_ss_set_call_waiting(get_tapi_ctx(), slot_id,
                                     EVENT_REQUEST_CALL_WAITING_DONE,
                                     enable, tele_ss_async_fun);
  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_ss_set_call_waiting execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_ss_async_fun is not executed in %s",
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

int tapi_ss_get_call_waiting_test(int slot_id, bool expect)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_QUERY_CALL_WAITING_DONE;
  global_data.cw = (int)expect;
  int ret = tapi_ss_get_call_waiting(get_tapi_ctx(), slot_id,
                                     EVENT_QUERY_CALL_WAITING_DONE,
                                     tele_ss_async_fun);
  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_ss_get_call_waiting execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_ss_async_fun is not executed in %s",
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

int tapi_ss_enable_fdn_test(int slot_id, bool enable, char *passwd)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_ENABLE_FDN_DONE;
  int ret =
      tapi_ss_enable_fdn(get_tapi_ctx(), slot_id, EVENT_ENABLE_FDN_DONE,
                         enable, passwd, tele_ss_async_fun);
  if (ret)
    {
      syslog(LOG_ERR, "tapi_ss_enable_fdn execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_ss_async_fun is not executed in %s",
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

int tapi_ss_query_fdn_test(int slot_id, bool expect)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_QUERY_FDN_DONE;
  global_data.fdn_enable = (int)expect;
  int ret = tapi_ss_query_fdn(get_tapi_ctx(), slot_id,
                              EVENT_QUERY_FDN_DONE, tele_ss_async_fun);
  if (ret)
    {
      syslog(LOG_ERR, "tapi_ss_query_fdn execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_ss_async_fun is not executed in %s",
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
