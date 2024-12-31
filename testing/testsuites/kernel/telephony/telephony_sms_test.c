/****************************************************************************
 * apps/testing/testsuites/kernel/telephony/telephony_sms_test.c
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
#include "telephony_sms_test.h"
#include "telephony_call_test.h"
#include "telephony_common_test.h"
#include "telephony_ims_test.h"
#include <uv.h>

extern struct judge_type judge_data;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

static void tele_sms_event_response(tapi_async_result *result)
{
  syslog(LOG_DEBUG, "%s : \n", __func__);
  syslog(LOG_DEBUG, "result->msg_id : %d\n", result->msg_id);
  syslog(LOG_DEBUG, "result->status : %d\n", result->status);
  syslog(LOG_DEBUG, "result->arg1 : %d\n", result->arg1);
  syslog(LOG_DEBUG, "result->arg2 : %d\n", result->arg2);

  if (result->status != OK)
    {
      syslog(LOG_DEBUG, "%s msg id: %d result err, return.\n", __func__,
             result->msg_id);
      return;
    }

  judge_data.result = 0;

  if (result->msg_id == EVENT_SEND_MESSAGE_DONE)
    {
      syslog(LOG_DEBUG, "send message successed, uuid : %s\n",
             (char *)result->data);
      judge_data.flag = EVENT_SEND_MESSAGE_DONE;
    }
  else if (result->msg_id == EVENT_SEND_DATA_MESSAGE_DONE)
    {
      syslog(LOG_DEBUG, "send data message successed");
      judge_data.flag = EVENT_SEND_DATA_MESSAGE_DONE;
    }
}

int sms_send_message_test(tapi_context context, int slot_id,
                          char *number, char *text)
{
  if (context == NULL || number == NULL || text == NULL)
    {
      syslog(LOG_ERR, "%s, number: %s, text: %s", __func__, number,
             text);
      return -EINVAL;
    }

  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_SEND_MESSAGE_DONE;

  int ret = tapi_sms_send_message(context, slot_id, 0, number, text,
                                  EVENT_SEND_MESSAGE_DONE,
                                  tele_sms_event_response);
  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_sms_send_message execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_sms_event_response is not executed in %s",
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

int sms_set_service_center_number_test(int slot_id)
{
  int ret = tapi_sms_set_service_center_address(get_tapi_ctx(), slot_id,
                                                "10086");
  syslog(LOG_DEBUG, "%s, slot_id: %d, ret: %d", __func__, slot_id, ret);

  return ret;
}

int sms_check_service_center_number_test(int slot_id)
{
  char *smsc_addr_rtn = NULL;
  int ret = tapi_sms_get_service_center_address(get_tapi_ctx(), slot_id,
                                                &smsc_addr_rtn);
  syslog(LOG_DEBUG, "%s, slot_id: %d, ret: %d, smsc_addr_rtn: %s",
         __func__, slot_id, ret, smsc_addr_rtn);

  return ret || strcmp(smsc_addr_rtn, "10086") != 0;
}

int sms_send_data_message_test(int slot_id, char *to, int port,
                               char *text)
{
  if (to == NULL)
    {
      syslog(LOG_ERR, "to is null in %s", __func__);
      return -EINVAL;
    }

  if (text == NULL)
    {
      syslog(LOG_ERR, "text is null in %s", __func__);
      return -EINVAL;
    }

  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_SEND_DATA_MESSAGE_DONE;
  int ret = tapi_sms_send_data_message(
      get_tapi_ctx(), slot_id, 0, to, port, text,
      EVENT_SEND_DATA_MESSAGE_DONE, tele_sms_event_response);
  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_sms_send_data_message execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_sms_event_response is not executed in %s",
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

int sms_send_message_in_dialing(int slot_id, char *to, char *text)
{
  int ret = -1;
  int res = 0;

  ret = tapi_call_dial_test(slot_id, to, 0);
  if (ret)
    {
      syslog(LOG_ERR, "tapi_call_dial_test execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  ret = sms_send_message_test(get_tapi_ctx(), slot_id, to, text);
  if (ret)
    {
      syslog(LOG_ERR,
             "sms_send_message_test execute fail in %s, ret: %d",
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

int sms_send_data_message_in_dialing(int slot_id, char *to, char *text,
                                     int port)
{
  int ret = -1;
  int res = 0;

  ret = tapi_call_dial_test(slot_id, to, 0);
  if (ret)
    {
      syslog(LOG_ERR, "tapi_call_dial_test execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  ret = sms_send_data_message_test(slot_id, to, port, text);
  if (ret)
    {
      syslog(LOG_ERR,
             "sms_send_data_message_test execute fail in %s, ret: %d",
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
