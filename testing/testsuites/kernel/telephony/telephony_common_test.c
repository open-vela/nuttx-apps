/****************************************************************************
 * apps/testing/testsuites/kernel/telephony/telephony_common_test.c
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
#include "telephony_common_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DATA_INIT_VALUE -100
#define HANDLE_REPEAT_TIME 1
#define HANDLE_REPEAT_COUNT 10

#define MAX_MODEM_WATCH_COUNT 4
#define MSG_DATA_IND_START MSG_DATA_ENABLED_CHANGE_IND
#define MSG_NETWORK_IND_START MSG_NETWORK_STATE_CHANGE_IND
#define MSG_MODEM_IND_START MSG_RADIO_STATE_CHANGE_IND

static struct
{
  int radio_state_watch_id;
  int phone_state_watch_id;
  int modem_restart_ind_watch_id;
  int oem_hook_raw_watch_id;
  int modem_state;
} modem_data;

extern struct judge_type judge_data;

static void radio_signal_change(tapi_async_result *result);

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
    case EVENT_MODEM_ACTIVITY_INFO_QUERY_DONE:
      syslog(LOG_DEBUG,
             "%s: EVENT_MODEM_ACTIVITY_INFO_QUERY_DONE status: %d\n",
             __func__, result->status);
      if (judge_data.expect == EVENT_MODEM_ACTIVITY_INFO_QUERY_DONE)
        {
          judge_data.result = status;
          judge_data.flag = EVENT_MODEM_ACTIVITY_INFO_QUERY_DONE;
        }

      break;
    case EVENT_MODEM_ENABLE_DONE:
      syslog(LOG_DEBUG, "%s: EVENT_MODEM_ENABLE_DONE status: %d\n",
             __func__, result->status);
      if (judge_data.expect == EVENT_MODEM_ENABLE_DONE)
        {
          judge_data.result = 0;
          judge_data.flag = EVENT_MODEM_ENABLE_DONE;
        }

      break;
    case EVENT_RADIO_STATE_SET_DONE:
      syslog(LOG_DEBUG, "%s: EVENT_RADIO_STATE_SET_DONE status: %d\n",
             __func__, result->status);
      if (judge_data.expect == EVENT_RADIO_STATE_SET_DONE)
        {
          judge_data.result = status;
          judge_data.flag = EVENT_RADIO_STATE_SET_DONE;
        }

      break;
    case EVENT_OEM_RIL_REQUEST_RAW_DONE:
      syslog(LOG_DEBUG,
             "%s: EVENT_OEM_RIL_REQUEST_RAW_DONE status: %d\n", __func__,
             result->status);
      if (judge_data.expect == EVENT_OEM_RIL_REQUEST_RAW_DONE)
        {
          judge_data.result = status;
          judge_data.flag = EVENT_OEM_RIL_REQUEST_RAW_DONE;
        }

      break;
    case EVENT_OEM_RIL_REQUEST_STRINGS_DONE:
      syslog(LOG_DEBUG,
             "%s: EVENT_OEM_RIL_REQUEST_STRINGS_DONE status: %d\n",
             __func__, result->status);
      if (judge_data.expect == EVENT_OEM_RIL_REQUEST_STRINGS_DONE)
        {
          judge_data.result = status;
          judge_data.flag = EVENT_OEM_RIL_REQUEST_STRINGS_DONE;
        }

      break;
    case EVENT_MODEM_STATUS_QUERY_DONE:
      syslog(LOG_DEBUG, "%s: EVENT_MODEM_STATUS_QUERY_DONE status: %d\n",
             __func__, result->status);
      if (judge_data.expect == EVENT_MODEM_STATUS_QUERY_DONE)
        {
          judge_data.result = status;
          judge_data.flag = EVENT_MODEM_STATUS_QUERY_DONE;
          modem_data.modem_state = result->arg2;
        }

      break;
    case EVENT_RAT_MODE_SET_DONE:
      syslog(LOG_DEBUG, "%s: EVENT_RAT_MODE_SET_DONE status: %d\n",
             __func__, result->status);
      if (judge_data.expect == EVENT_RAT_MODE_SET_DONE)
        {
          judge_data.result = status;
          judge_data.flag = EVENT_RAT_MODE_SET_DONE;
        }

      break;
    default:
      break;
    }
}

int tapi_get_imei_test(int slot_id)
{
  char *imei = NULL;

  int ret = tapi_get_imei(get_tapi_ctx(), slot_id, &imei);
  syslog(LOG_DEBUG, "%s, slotId : %d imei : %s \n", __func__, slot_id,
         imei);

  return ret;
}

int tapi_get_modem_revision_test(int slot_id)
{
  char *version = NULL;

  int ret = tapi_get_modem_revision(get_tapi_ctx(), slot_id, &version);
  syslog(LOG_DEBUG, "%s, slotId : %d version : %s \n", __func__, slot_id,
         version);

  return ret || (!version);
}

int tapi_get_pref_net_mode_test(int slot_id, tapi_pref_net_mode *value)
{
  int ret = tapi_get_pref_net_mode(get_tapi_ctx(), slot_id, value);
  syslog(LOG_DEBUG, "%s, slotId : %d value :%d \n", __func__, slot_id,
         *value);
  return ret;
}

int tapi_set_radio_power_test(int slot_id, bool target_state)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_RADIO_STATE_SET_DONE;

  int ret = tapi_set_radio_power(get_tapi_ctx(), slot_id,
                                 EVENT_RADIO_STATE_SET_DONE,
                                 target_state, tele_call_async_fun);

  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_set_radio_power_test execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_DEBUG,
             "tapi_set_radio_power_test is not executed in %s",
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

int tapi_get_radio_power_test(int slot_id, bool *value)
{
  int ret = tapi_get_radio_power(get_tapi_ctx(), slot_id, value);
  syslog(LOG_DEBUG, "%s, slotId : %d value : %d \n", __func__, slot_id,
         *value);
  return ret;
}

int tapi_modem_register_test(int slot_id)
{
  modem_data.radio_state_watch_id = -1;
  modem_data.radio_state_watch_id =
      tapi_register(get_tapi_ctx(), slot_id, MSG_RADIO_STATE_CHANGE_IND,
                    NULL, radio_signal_change);
  if (modem_data.radio_state_watch_id < 0)
    {
      syslog(LOG_ERR, "%s, radio state change registered fail, ret: %d",
             __func__, modem_data.radio_state_watch_id);
      return -1;
    }

  modem_data.phone_state_watch_id = -1;
  modem_data.phone_state_watch_id =
      tapi_register(get_tapi_ctx(), slot_id, MSG_PHONE_STATE_CHANGE_IND,
                    NULL, radio_signal_change);
  if (modem_data.phone_state_watch_id < 0)
    {
      syslog(LOG_ERR, "%s, phone state change registered fail, ret: %d",
             __func__, modem_data.phone_state_watch_id);
      return -1;
    }

  modem_data.modem_restart_ind_watch_id = -1;
  modem_data.modem_restart_ind_watch_id =
      tapi_register(get_tapi_ctx(), slot_id, MSG_MODEM_RESTART_IND, NULL,
                    radio_signal_change);
  if (modem_data.modem_restart_ind_watch_id < 0)
    {
      syslog(LOG_ERR, "%s, modem restart ind registered fail, ret: %d",
             __func__, modem_data.modem_restart_ind_watch_id);
      return -1;
    }

  modem_data.oem_hook_raw_watch_id = -1;
  modem_data.oem_hook_raw_watch_id =
      tapi_register(get_tapi_ctx(), slot_id, MSG_OEM_HOOK_RAW_IND, NULL,
                    radio_signal_change);
  if (modem_data.oem_hook_raw_watch_id < 0)
    {
      syslog(LOG_ERR, "%s, oem hook raw registered fail, ret: %d",
             __func__, modem_data.oem_hook_raw_watch_id);
      return -1;
    }

  return 0;
}

int tapi_modem_unregister_test(void)
{
  int ret = -1;
  int res = 0;
  ret = tapi_unregister(get_tapi_ctx(), modem_data.radio_state_watch_id);
  if (ret)
    {
      syslog(LOG_ERR,
             "unregister radio state change fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  ret = tapi_unregister(get_tapi_ctx(), modem_data.phone_state_watch_id);
  if (ret)
    {
      syslog(LOG_ERR,
             "unregister phone state change fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  ret = tapi_unregister(get_tapi_ctx(),
                        modem_data.modem_restart_ind_watch_id);
  if (ret)
    {
      syslog(LOG_ERR,
             "unregister modem restart change fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  ret =
      tapi_unregister(get_tapi_ctx(), modem_data.oem_hook_raw_watch_id);
  if (ret)
    {
      syslog(LOG_ERR, "unregister oem hook raw fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

static int hex_string_to_byte_array(char *hex_str,
                                    unsigned char *byte_arr, int arr_len)
{
  char *str;
  int len;
  int i;
  int j;

  if (hex_str == NULL)
    return -EINVAL;

  len = strlen(hex_str);
  if (!len || (len % 2) != 0 || len > arr_len * 2)
    return -EINVAL;

  str = strdup(hex_str);

  for (i = 0, j = 0; i < len; i += 2, j++)
    {
      /* uppercase char 'a' ~ 'f' */

      if (str[i] >= 'a' && str[i] <= 'f')
        str[i] = str[i] & ~0x20;

      if (str[i + 1] >= 'a' && str[i + 1] <= 'f')
        str[i + 1] = str[i + 1] & ~0x20;

      /* convert the first character to decimal. */

      if (str[i] >= 'A' && str[i] <= 'F')
        byte_arr[j] = (str[i] - 'A' + 10) << 4;
      else if (str[i] >= '0' && str[i] <= '9')
        byte_arr[j] = (str[i] & ~0x30) << 4;
      else
        return -EINVAL;

      /* convert the second character to decimal */

      /* and combine with the previous decimal. */

      if (str[i + 1] >= 'A' && str[i + 1] <= 'F')
        byte_arr[j] |= (str[i + 1] - 'A' + 10);
      else if (str[i + 1] >= '0' && str[i + 1] <= '9')
        byte_arr[j] |= (str[i + 1] & ~0x30);
      else
        return -EINVAL;
    }

  free(str);
  return 0;
}

int tapi_invoke_oem_ril_request_raw_test(int slot_id, char *oem_req,
                                         int length)
{
  int res = 0;
  unsigned char req_data[MAX_INPUT_ARGS_LEN];
  hex_string_to_byte_array(oem_req, req_data, MAX_INPUT_ARGS_LEN);
  judge_data_init();
  judge_data.expect = EVENT_OEM_RIL_REQUEST_RAW_DONE;

  int ret = tapi_invoke_oem_ril_request_raw(
      get_tapi_ctx(), slot_id, EVENT_OEM_RIL_REQUEST_RAW_DONE, req_data,
      length, tele_call_async_fun);

  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_invoke_oem_ril_request_raw_test execute fail in %s, "
             "ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(
          LOG_DEBUG,
          "tapi_invoke_oem_ril_request_raw_test is not executed in %s",
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

int tapi_invoke_oem_ril_request_strings_test(int slot_id, char *req_data,
                                             int length)
{
  char *result;
  char *ptr = NULL;

  /* FIXME: oem_req should be inited */

  char *oem_req[MAX_OEM_RIL_RESP_STRINGS_LENTH] =
  {
    0
  };

  int count = 0;
  int res = 0;
  result = strtok_r(req_data, "|", &ptr);
  while (result != NULL)
    {
      if (count < length)
        oem_req[count] = result;

      count++;
      result = strtok_r(NULL, "|", &ptr);
    }

  judge_data_init();
  judge_data.expect = EVENT_OEM_RIL_REQUEST_STRINGS_DONE;

  int ret = tapi_invoke_oem_ril_request_strings(
      get_tapi_ctx(), slot_id, EVENT_OEM_RIL_REQUEST_STRINGS_DONE,
      oem_req, length, tele_call_async_fun);

  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_invoke_oem_ril_request_strings_test execute fail in "
             "%s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_DEBUG,
             "tapi_invoke_oem_ril_request_strings_test is not executed "
             "in %s",
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

int tapi_get_modem_activity_info_test(int slot_id)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_MODEM_ACTIVITY_INFO_QUERY_DONE;

  int ret = tapi_get_modem_activity_info(
      get_tapi_ctx(), slot_id, EVENT_MODEM_ACTIVITY_INFO_QUERY_DONE,
      tele_call_async_fun);

  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_get_modem_activity_info_test execute fail in %s, "
             "ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_DEBUG,
             "tapi_get_modem_activity_info_test is not executed in %s",
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

int tapi_enable_modem_test(int slot_id, int target_state)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_MODEM_ENABLE_DONE;

  int ret =
      tapi_enable_modem(get_tapi_ctx(), slot_id, EVENT_MODEM_ENABLE_DONE,
                        target_state, tele_call_async_fun);

  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_enable_modem_test execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_DEBUG, "tapi_enable_modem_test is not executed in %s",
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

int tapi_get_modem_status_test(int slot_id, int *state)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_MODEM_STATUS_QUERY_DONE;
  modem_data.modem_state = -1;

  int ret = tapi_get_modem_status(get_tapi_ctx(), slot_id,
                                  EVENT_MODEM_STATUS_QUERY_DONE,
                                  tele_call_async_fun);

  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_get_modem_enable_status_test execute fail in %s, "
             "ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_DEBUG,
             "tapi_get_modem_enable_status_test is not executed in %s",
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

  if (modem_data.modem_state != *state)
    {
      syslog(LOG_ERR, "modem_data.modem_state is invalid in %s",
             __func__);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

int tapi_set_pref_net_mode_test(int slot_id,
                                tapi_pref_net_mode target_state)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_RAT_MODE_SET_DONE;

  int ret = tapi_set_pref_net_mode(get_tapi_ctx(), slot_id,
                                   EVENT_RAT_MODE_SET_DONE, target_state,
                                   tele_call_async_fun);

  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_set_pref_net_mode_test execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_DEBUG,
             "tapi_set_pref_net_mode_test is not executed in %s",
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

/****************************************************************************
 * Public Functions
 ****************************************************************************/

static void radio_signal_change(tapi_async_result *result)
{
  int signal = result->msg_id;
  int slot_id = result->arg1;
  int param = result->arg2;

  switch (signal)
    {
    case MSG_RADIO_STATE_CHANGE_IND:
      syslog(LOG_DEBUG, "radio state changed to %d in slot[%d] \n",
             param, slot_id);
      if (judge_data.expect == MSG_RADIO_STATE_CHANGE_IND)
        {
          judge_data.result = OK;
        }
      break;
    case MSG_PHONE_STATE_CHANGE_IND:
      syslog(LOG_DEBUG, "phone state changed to %d in slot[%d] \n",
             param, slot_id);
      if (judge_data.expect == MSG_PHONE_STATE_CHANGE_IND)
        {
          judge_data.result = OK;
        }
      break;
    case MSG_MODEM_RESTART_IND:
      syslog(LOG_DEBUG, "modem restart in slot[%d] \n", slot_id);
      if (judge_data.expect == MSG_MODEM_RESTART_IND)
        {
          judge_data.result = OK;
        }
      break;
    case MSG_OEM_HOOK_RAW_IND:
      syslog(LOG_DEBUG, "oem hook raw in slot[%d] \n", slot_id);
      if (judge_data.expect == MSG_OEM_HOOK_RAW_IND)
        {
          judge_data.result = OK;
        }
      break;
    default:
      break;
    }
}
