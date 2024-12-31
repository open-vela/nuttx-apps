/****************************************************************************
 * apps/testing/testsuites/kernel/telephony/telephony_sim_test.c
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
#include "telephony_sim_test.h"

extern struct judge_type judge_data;

static struct
{
  int sim_state_change_watch_id;
  int sim_uicc_app_enabled_change_watch_id;
  int current_channel_session_id;
} global_data;

static void global_data_init(void)
{
  global_data.sim_state_change_watch_id = -1;
  global_data.sim_uicc_app_enabled_change_watch_id = -1;
}

int tapi_sim_has_icc_card_test(int slot_id)
{
  bool result = false;
  int ret = tapi_sim_has_icc_card(get_tapi_ctx(), slot_id, &result);
  syslog(LOG_DEBUG, "%s, ret: %d, slot_id: %d, result: %d\n", __func__,
         ret, slot_id, result);

  return ret || (!result);
}

int tapi_sim_multi_has_icc_card_test(int slot_id)
{
  int ret = -1;
  for (int i = 0; i < TEST_COUNT; ++i)
    {
      if ((ret = tapi_sim_has_icc_card_test(slot_id)) != 0)
        {
          return ret;
        }
    }

  return ret;
}

int tapi_sim_get_sim_operator_test(int slot_id, const char *expect_res)
{
  char operator[MAX_MCC_LENGTH + MAX_MNC_LENGTH + 1];
  memset(operator, 0, sizeof(operator));
  int ret = tapi_sim_get_sim_operator(
      get_tapi_ctx(), 0,
      (MAX_MCC_LENGTH + MAX_MNC_LENGTH + 1), operator);
  syslog(LOG_DEBUG, "%s, ret: %d, slot_id: %d, operator: %s\n", __func__,
         ret, slot_id, operator);

  return ret || (operator[0] == 0) || strcmp(expect_res, operator);
}

int tapi_sim_multi_get_sim_operator(int slot_id, const char *expect_res)
{
  int ret = -1;
  for (int i = 0; i < TEST_COUNT; ++i)
    {
      if ((ret = tapi_sim_get_sim_operator_test(slot_id, expect_res)) !=
          0)
        {
          return ret;
        }
    }

  return ret;
}

int tapi_sim_get_sim_operator_name_test(int slot_id,
                                        const char *expect_res)
{
  char *spn = NULL;
  int ret =
      tapi_sim_get_sim_operator_name(get_tapi_ctx(), slot_id, &spn);
  syslog(LOG_DEBUG, "%s, ret: %d, slot_id: %d, spn: %s\n", __func__, ret,
         slot_id, spn);

  return ret || (spn == NULL) || strcmp(expect_res, spn);
}

int tapi_sim_get_sim_operator_name_numerous(int slot_id,
                                            const char *expect_res)
{
  int ret = -1;
  for (int i = 0; i < TEST_COUNT; ++i)
    {
      if ((ret = tapi_sim_get_sim_operator_name_test(slot_id,
                                                     expect_res)) != 0)
        {
          goto on_exit;
        }
    }

on_exit:
  return ret;
}

int tapi_sim_get_sim_subscriber_id_test(int slot_id,
                                        const char *expect_res)
{
  char *result = NULL;
  int ret = tapi_sim_get_subscriber_id(get_tapi_ctx(), slot_id, &result);
  syslog(LOG_DEBUG, "%s, ret: %d, slot_id: %d, subscriber_id: %s\n",
         __func__, ret, slot_id, result);

  return ret || (!result) || strcmp(result, expect_res);
}

int tapi_sim_multi_get_sim_subscriber_id_test(int slot_id,
                                              const char *expect_res)
{
  int ret = -1;
  for (int i = 0; i < TEST_COUNT; ++i)
    {
      if ((ret = tapi_sim_get_sim_subscriber_id_test(slot_id,
                                                     expect_res)) != 0)
        {
          return ret;
        }
    }

  return ret;
}

int tapi_sim_get_sim_iccid_test(int slot_id, const char *expect_res)
{
  char *iccid = NULL;
  int ret = tapi_sim_get_sim_iccid(get_tapi_ctx(), slot_id, &iccid);
  syslog(LOG_DEBUG, "%s, ret: %d, slot_id: %d, iccid: %s\n", __func__,
         ret, slot_id, iccid);

  return ret || iccid == NULL || strcmp(expect_res, iccid);
}

int tapi_sim_multi_get_sim_iccid_test(int slot_id,
                                      const char *expect_res)
{
  int ret = -1;
  for (int i = 0; i < TEST_COUNT; ++i)
    {
      if ((ret = tapi_sim_get_sim_iccid_test(slot_id, expect_res)) != 0)
        {
          return ret;
        }
    }

  return ret;
}

int tapi_sim_get_ef_msisdn_test(int slot_id, const char *expect_res)
{
  char *number = NULL;
  int ret = tapi_get_msisdn_number(get_tapi_ctx(), slot_id, &number);
  syslog(LOG_DEBUG, "%s, ret: %d, slotId : %d  number : %s \n", __func__,
         ret, slot_id, number);

  return ret || number == NULL || strcmp(expect_res, number);
}

int tapi_sim_multi_get_ef_msisdn_test(int slot_id,
                                      const char *expect_res)
{
  int ret = -1;
  for (int i = 0; i < TEST_COUNT; ++i)
    {
      if ((ret = tapi_sim_get_ef_msisdn_test(slot_id, expect_res)) != 0)
        {
          return ret;
        }
    }

  return ret;
}

int tapi_sim_get_state_test(int slot_id)
{
  int state = 0;
  int ret = tapi_sim_get_sim_state(get_tapi_ctx(), slot_id, &state);
  syslog(LOG_DEBUG, "%s, ret: %d, slotId : %d sim_state : %s \n",
         __func__, ret, slot_id,
         tapi_sim_state_to_string((tapi_sim_state)state));

  return ret || state != 3;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

static void tele_sim_async_fun(tapi_async_result *result)
{
  sim_lock_state *sim_lock = NULL;
  sim_state_result *ss;
  unsigned char *apdu_data;
  int i;

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

  if (result->msg_id == EVENT_TRANSMIT_APDU_LOGICAL_CHANNEL_DONE ||
      result->msg_id == EVENT_TRANSMIT_APDU_BASIC_CHANNEL_DONE)
    {
      apdu_data = (unsigned char *)result->data;
      for (i = 0; i < result->arg2; i++)
        syslog(LOG_DEBUG, "apdu data %d : %d ", i, apdu_data[i]);

      if (judge_data.expect == EVENT_TRANSMIT_APDU_BASIC_CHANNEL_DONE)
        {
          judge_data.result = 0;
          judge_data.flag = EVENT_TRANSMIT_APDU_BASIC_CHANNEL_DONE;
        }
      else if (judge_data.expect ==
               EVENT_TRANSMIT_APDU_LOGICAL_CHANNEL_DONE)
        {
          judge_data.result = 0;
          judge_data.flag = EVENT_TRANSMIT_APDU_LOGICAL_CHANNEL_DONE;
        }
    }
  else if (result->msg_id == EVENT_OPEN_LOGICAL_CHANNEL_DONE)
    {
      syslog(LOG_DEBUG, "open logical channel respond session id : %d\n",
             result->arg2);
      global_data.current_channel_session_id = result->arg2;

      if (judge_data.expect == EVENT_OPEN_LOGICAL_CHANNEL_DONE)
        {
          judge_data.result = 0;
          judge_data.flag = EVENT_OPEN_LOGICAL_CHANNEL_DONE;
        }
    }
  else if (result->msg_id == MSG_SIM_STATE_CHANGE_IND)
    {
      ss = result->data;
      if (ss != NULL)
        {
          syslog(LOG_DEBUG, "response strings name : %s\n", ss->name);
          if (strcmp(ss->name, "Present") == 0)
            {
              syslog(LOG_DEBUG, "response is sim present : %d\n",
                     ss->value);
            }
          else if (strcmp(ss->name, "PinRequired") == 0)
            {
              syslog(LOG_DEBUG, "response pin required type : %s\n",
                     (char *)ss->data);
            }
          else if (strcmp(ss->name, "LockedPins") == 0)
            {
              sim_lock = ss->data;
              if (sim_lock != NULL)
                {
                  for (i = 0; i < result->arg2; ++i)
                    {
                      syslog(LOG_DEBUG,
                             "response locked pins type : %s\n",
                             sim_lock->sim_pwd_type[i]);
                    }
                }
            }
          else if (strcmp(ss->name, "Retries") == 0)
            {
              sim_lock = ss->data;
              if (sim_lock != NULL)
                {
                  for (i = 0; i < result->arg2; ++i)
                    {
                      syslog(LOG_DEBUG,
                             "response locked pins type : %s\n",
                             sim_lock->sim_pwd_type[i]);
                      syslog(LOG_DEBUG,
                             "response locked pins retries : %d\n",
                             sim_lock->retry_count[i]);
                    }
                }
            }
        }
    }
  else if (result->msg_id == EVENT_CLOSE_LOGICAL_CHANNEL_DONE)
    {
      if (judge_data.expect == EVENT_CLOSE_LOGICAL_CHANNEL_DONE)
        {
          judge_data.result = 0;
          judge_data.flag = EVENT_CLOSE_LOGICAL_CHANNEL_DONE;
        }
    }
  else if (result->msg_id == EVENT_UICC_ENABLEMENT_SET_DONE)
    {
      if (judge_data.expect == EVENT_UICC_ENABLEMENT_SET_DONE)
        {
          judge_data.result = 0;
          judge_data.flag = EVENT_UICC_ENABLEMENT_SET_DONE;
        }
    }
  else if (result->msg_id == EVENT_ENTER_SIM_PIN_DONE)
    {
      if (judge_data.expect == EVENT_ENTER_SIM_PIN_DONE)
        {
          judge_data.result = 0;
          judge_data.flag = EVENT_ENTER_SIM_PIN_DONE;
        }
    }
  else if (result->msg_id == EVENT_CHANGE_SIM_PIN_DONE)
    {
      if (judge_data.expect == EVENT_CHANGE_SIM_PIN_DONE)
        {
          judge_data.result = 0;
          judge_data.flag = EVENT_CHANGE_SIM_PIN_DONE;
        }
    }
  else if (result->msg_id == EVENT_LOCK_SIM_PIN_DONE)
    {
      if (judge_data.expect == EVENT_LOCK_SIM_PIN_DONE)
        {
          judge_data.result = 0;
          judge_data.flag = EVENT_LOCK_SIM_PIN_DONE;
        }
    }
  else if (result->msg_id == EVENT_UNLOCK_SIM_PIN_DONE)
    {
      if (judge_data.expect == EVENT_UNLOCK_SIM_PIN_DONE)
        {
          judge_data.result = 0;
          judge_data.flag = EVENT_UNLOCK_SIM_PIN_DONE;
        }
    }
}

int tapi_sim_listen_sim_test(int slot_id, int event_id)
{
  global_data_init();

  if (event_id != MSG_SIM_STATE_CHANGE_IND &&
      event_id != MSG_SIM_UICC_APP_ENABLED_CHANGE_IND)
    {
      syslog(LOG_ERR, "event id error\n");
      return -1;
    }

  if (event_id == MSG_SIM_STATE_CHANGE_IND)
    {
      global_data.sim_state_change_watch_id = tapi_sim_register(
          get_tapi_ctx(), slot_id, event_id, NULL, tele_sim_async_fun);

      if (global_data.sim_state_change_watch_id < 0)
        {
          syslog(LOG_ERR,
                 "%s, slot_id: %d, MSG_SIM_STATE_CHANGE_IND, watch id < "
                 "0\n",
                 __func__, slot_id);
          return -1;
        }
    }
  else
    {
      global_data.sim_uicc_app_enabled_change_watch_id =
          tapi_sim_register(get_tapi_ctx(), slot_id, event_id, NULL,
                            tele_sim_async_fun);

      if (global_data.sim_uicc_app_enabled_change_watch_id < 0)
        {
          syslog(LOG_ERR,
                 "%s, slot_id: %d, MSG_SIM_UICC_APP_ENABLED_CHANGE_IND, "
                 "watch id < 0\n",
                 __func__, slot_id);
          return -1;
        }
    }

  return 0;
}

int tapi_sim_unlisten_sim_test(int slot_id, int watch_id)
{
  if (watch_id == -1 ||
      (watch_id != global_data.sim_state_change_watch_id &&
       watch_id != global_data.sim_uicc_app_enabled_change_watch_id))
    {
      syslog(LOG_ERR, "watch id error\n");
      return -1;
    }

  int ret = tapi_sim_unregister(get_tapi_ctx(), watch_id);

  if (ret == 0)
    {
      if (watch_id == global_data.sim_state_change_watch_id)
        {
          global_data.sim_state_change_watch_id = -1;
        }
      else
        {
          global_data.sim_uicc_app_enabled_change_watch_id = -1;
        }
    }

  return ret;
}

int tapi_open_logical_channel_test(int slot_id)
{
  judge_data_init();
  judge_data.expect = EVENT_OPEN_LOGICAL_CHANNEL_DONE;
  unsigned char aid[] = "A0000000871002FF86FFFF89FFFFFFFF";
  int res = 0;
  global_data.current_channel_session_id = -1;

  int ret = tapi_sim_open_logical_channel(
      get_tapi_ctx(), slot_id, EVENT_OPEN_LOGICAL_CHANNEL_DONE, aid, 16,
      tele_sim_async_fun);

  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_sim_open_logical_channel execute fail, ret: %d", ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_sim_async_fun was not executed in %s",
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

  if (global_data.current_channel_session_id == -1)
    {
      syslog(LOG_ERR, "session id is invalid in %s", __func__);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

int tapi_close_logical_channel_test(int slot_id)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_CLOSE_LOGICAL_CHANNEL_DONE;

  if (global_data.current_channel_session_id == -1)
    {
      syslog(LOG_ERR, "session id is invalid in %s", __func__);
      res = -1;
      goto on_exit;
    }

  int ret = tapi_sim_close_logical_channel(
      get_tapi_ctx(), slot_id, EVENT_CLOSE_LOGICAL_CHANNEL_DONE,
      global_data.current_channel_session_id, tele_sim_async_fun);

  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_sim_close_logical_channel execute fail, ret: %d",
             ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_sim_async_fun was not executed in %s",
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
  global_data.current_channel_session_id = -1;
  return res;
}

static int hex_string_to_byte_array(char *hex_str,
                                    unsigned char *byte_arr, int arr_len)
{
  char *str;
  int len;
  int i;
  int j;
  int ret = -EINVAL;

  if (hex_str == NULL)
      return ret;

  len = strlen(hex_str);
  if (!len || (len % 2) != 0 || len > arr_len * 2)
      return ret;

  str = strdup(hex_str);
  if (str == NULL)
      return ret;

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
          goto out;

      /* convert the second character to decimal
       * and combine with the previous decimal.
       */

      if (str[i + 1] >= 'A' && str[i + 1] <= 'F')
        byte_arr[j] |= (str[i + 1] - 'A' + 10);
      else if (str[i + 1] >= '0' && str[i + 1] <= '9')
        byte_arr[j] |= (str[i + 1] & ~0x30);
      else
          goto out;
    }

  ret = 0;
out:
  free(str);

  return ret;
}

int tapi_transmit_apdu_basic_channel_test(int slot_id)
{
  char data[] = "A0B000010473656E669000";
  int res = 0;
  unsigned char pdu[128];
  judge_data_init();
  judge_data.expect = EVENT_TRANSMIT_APDU_BASIC_CHANNEL_DONE;
  int len = strlen(data) / 2;

  if (hex_string_to_byte_array(data, pdu, 128) != 0)
    {
      syslog(LOG_ERR, "hex_string_to_byte_array execute fail");
      res = -1;
      goto on_exit;
    }

  int ret = tapi_sim_transmit_apdu_basic_channel(
      get_tapi_ctx(), slot_id, EVENT_TRANSMIT_APDU_BASIC_CHANNEL_DONE,
      pdu, len, tele_sim_async_fun);

  if (ret)
    {
      syslog(
          LOG_ERR,
          "tapi_sim_transmit_apdu_basic_channel execute fail, ret: %d",
          ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_sim_async_fun was not executed in %s",
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

int tapi_transmit_apdu_logical_channel_test(int slot_id)
{
  int res = 0;

  if (global_data.current_channel_session_id == -1)
    {
      syslog(LOG_ERR, "session is invalid in %s", __func__);
      res = -1;
      goto on_exit;
    }

  unsigned char pdu[128];
  char data[] = "FFF2000000";
  int len = strlen(data) / 2;

  if (hex_string_to_byte_array(data, pdu, 128) != 0)
    {
      syslog(LOG_ERR, "hex_string_to_byte_array execute fail in %s",
             __func__);
      res = -1;
      goto on_exit;
    }

  judge_data_init();
  judge_data.expect = EVENT_TRANSMIT_APDU_LOGICAL_CHANNEL_DONE;

  int ret = tapi_sim_transmit_apdu_logical_channel(
      get_tapi_ctx(), slot_id, EVENT_TRANSMIT_APDU_LOGICAL_CHANNEL_DONE,
      global_data.current_channel_session_id, pdu, len,
      tele_sim_async_fun);

  if (ret)
    {
      syslog(
          LOG_ERR,
          "tapi_sim_transmit_apdu_logical_channel execute fail, ret: %d",
          ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_sim_async_fun was not executed in %s",
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

int sim_open_close_logical_channel_numerous(int slot_id)
{
  for (int i = 0; i < 10; i++)
    {
      if (tapi_open_logical_channel_test(slot_id))
          return -1;

      if (tapi_close_logical_channel_test(slot_id))
          return -1;
    }

  return 0;
}

int sim_transmit_apdu_by_logical_channel(int slot_id)
{
  int ret = -1;
  int res = 0;
  ret = tapi_open_logical_channel_test(slot_id);
  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_open_logical_channel_test execute fail in %s",
             __func__);
      res = -1;
      goto on_exit;
    }

  ret = tapi_transmit_apdu_logical_channel_test(slot_id);
  if (ret)
    {
      syslog(
          LOG_ERR,
          "tapi_transmit_apdu_logical_channel_test execute fail in %s",
          __func__);
      res = -1;
      goto on_exit;
    }

  ret = tapi_close_logical_channel_test(slot_id);
  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_close_logical_channel_test execute fail in %s",
             __func__);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

int tapi_sim_set_uicc_enablement_test(int slot_id)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_UICC_ENABLEMENT_SET_DONE;

  int ret = tapi_sim_set_uicc_enablement(get_tapi_ctx(), slot_id,
                                         EVENT_UICC_ENABLEMENT_SET_DONE,
                                         1, tele_sim_async_fun);

  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_sim_set_uicc_enablement execute fail, ret: %d", ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_sim_async_fun was not executed in %s",
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

int tapi_sim_get_uicc_enablement_test(int slot_id)
{
  tapi_sim_uicc_app_state state = SIM_UICC_APP_UNKNOWN;
  int ret =
      tapi_sim_get_uicc_enablement(get_tapi_ctx(), slot_id, &state);
  syslog(LOG_DEBUG, "%s, ret: %d, state: %d", __func__, ret, (int)state);

  return ret || state != SIM_UICC_APP_ACTIVE;
}

int tapi_sim_enter_pin_test(int slot_id)
{
  judge_data_init();
  judge_data.expect = EVENT_ENTER_SIM_PIN_DONE;
  int res = 0;

  int ret = tapi_sim_enter_pin(get_tapi_ctx(), slot_id,
                               EVENT_ENTER_SIM_PIN_DONE, "pin2", "1234",
                               tele_sim_async_fun);

  if (ret)
    {
      syslog(LOG_DEBUG, "tapi_sim_enter_pin execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_sim_async_fun was not executed in %s",
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

int tapi_sim_change_pin_test(int slot_id, char *old_pin, char *new_pin)
{
  syslog(LOG_DEBUG, "%s, old_pin: %s, new_pin: %s", __func__, old_pin,
         new_pin);
  judge_data_init();
  judge_data.expect = EVENT_CHANGE_SIM_PIN_DONE;
  int res = 0;

  int ret = tapi_sim_change_pin(get_tapi_ctx(), slot_id,
                                EVENT_CHANGE_SIM_PIN_DONE, "pin2",
                                old_pin, new_pin, tele_sim_async_fun);

  if (ret)
    {
      syslog(LOG_ERR, "tapi_sim_change_pin execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_sim_async_fun was not executed in %s",
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

int sim_change_pin_test(int slot_id)
{
  int ret = -1;
  int res = 0;
  ret = tapi_sim_change_pin_test(slot_id, "1234", "2345");
  if (ret)
    {
      syslog(LOG_DEBUG, "change pin from \"1234\" to \"2345\" fail");
      res = -1;
      goto on_exit;
    }

  ret = tapi_sim_change_pin_test(slot_id, "2345", "1234");
  if (ret)
    {
      syslog(LOG_DEBUG, "change pin from \"2345\" to \"1234\" fail");
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

int tapi_sim_lock_pin_test(int slot_id)
{
  judge_data_init();
  judge_data.expect = EVENT_LOCK_SIM_PIN_DONE;
  int res = 0;

  int ret =
      tapi_sim_lock_pin(get_tapi_ctx(), slot_id, EVENT_LOCK_SIM_PIN_DONE,
                        "pin", "1234", tele_sim_async_fun);

  if (ret)
    {
      syslog(LOG_ERR, "tapi_sim_lock_pin execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_sim_async_fun was not executed in %s",
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

int tapi_sim_unlock_pin_test(int slot_id)
{
  judge_data_init();
  judge_data.expect = EVENT_UNLOCK_SIM_PIN_DONE;
  int res = 0;

  int ret = tapi_sim_unlock_pin(get_tapi_ctx(), slot_id,
                                EVENT_UNLOCK_SIM_PIN_DONE, "pin", "1234",
                                tele_sim_async_fun);

  if (ret)
    {
      syslog(LOG_ERR, "tapi_sim_unlock_pin execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_sim_async_fun was not executed in %s",
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

static void tele_phonebook_async_fun(tapi_async_result *result)
{
  syslog(LOG_DEBUG, "%s : \n", __func__);
  syslog(LOG_DEBUG, "result->msg_id : %d\n", result->msg_id);
  syslog(LOG_DEBUG, "result->status : %d\n", result->status);
  syslog(LOG_DEBUG, "result->arg1 : %d\n", result->arg1);
  syslog(LOG_DEBUG, "result->arg2 : %d\n", result->arg2);

  if (result->msg_id == EVENT_LOAD_ADN_ENTRIES_DONE)
    {
      syslog(LOG_DEBUG, "adn entries : %s\n", (char *)result->data);
      if (judge_data.expect == EVENT_LOAD_ADN_ENTRIES_DONE)
        {
          judge_data.result = 0;
          judge_data.flag = EVENT_LOAD_ADN_ENTRIES_DONE;
        }
    }
  else if (result->msg_id == EVENT_LOAD_FDN_ENTRIES_DONE)
    {
      if (judge_data.expect == EVENT_LOAD_FDN_ENTRIES_DONE)
        {
          judge_data.result = 0;
          judge_data.flag = EVENT_LOAD_FDN_ENTRIES_DONE;
        }
    }
  else if (result->msg_id == EVENT_INSERT_FDN_ENTRIES_DONE)
    {
      if (judge_data.expect == EVENT_INSERT_FDN_ENTRIES_DONE)
        {
          judge_data.result = 0;
          judge_data.flag = EVENT_INSERT_FDN_ENTRIES_DONE;
        }
    }
  else if (result->msg_id == EVENT_UPDATE_FDN_ENTRIES_DONE)
    {
      if (judge_data.expect == EVENT_UPDATE_FDN_ENTRIES_DONE)
        {
          judge_data.result = 0;
          judge_data.flag = EVENT_UPDATE_FDN_ENTRIES_DONE;
        }
    }
  else if (result->msg_id == EVENT_DELETE_FDN_ENTRIES_DONE)
    {
      if (judge_data.expect == EVENT_DELETE_FDN_ENTRIES_DONE)
        {
          judge_data.result = 0;
          judge_data.flag = EVENT_DELETE_FDN_ENTRIES_DONE;
        }
    }
}

int tapi_phonebook_load_adn_entries_test(int slot_id)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_LOAD_ADN_ENTRIES_DONE;
  int ret = tapi_phonebook_load_adn_entries(get_tapi_ctx(), slot_id,
                                            EVENT_LOAD_ADN_ENTRIES_DONE,
                                            tele_phonebook_async_fun);
  if (ret)
    {
      syslog(
          LOG_ERR,
          "tapi_phonebook_load_adn_entries execute fail in %s, ret: %d",
          __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_phonebook_async_fun is not executed in %s",
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

int tapi_phonebook_load_fdn_entries_test(int slot_id)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_LOAD_FDN_ENTRIES_DONE;
  int ret = tapi_phonebook_load_fdn_entries(get_tapi_ctx(), slot_id,
                                            EVENT_LOAD_FDN_ENTRIES_DONE,
                                            tele_phonebook_async_fun);
  if (ret)
    {
      syslog(
          LOG_DEBUG,
          "tapi_phonebook_load_fdn_entries execute fail in %s, ret: %d",
          __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_phonebook_async_fun is not executed in %s",
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

int tapi_phonebook_insert_fdn_entry_test(int slot_id)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_INSERT_FDN_ENTRIES_DONE;
  int ret = tapi_phonebook_insert_fdn_entry(
      get_tapi_ctx(), slot_id, EVENT_INSERT_FDN_ENTRIES_DONE, "cmcc",
      "10086", "1234", tele_phonebook_async_fun);
  if (ret)
    {
      syslog(
          LOG_ERR,
          "tapi_phonebook_insert_fdn_entry execute fail in %s, ret: %d",
          __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_DEBUG, "tele_phonebook_async_fun is not executed in %s",
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

int tapi_phonebook_update_fdn_entry_test(int slot_id)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_UPDATE_FDN_ENTRIES_DONE;
  int ret = tapi_phonebook_update_fdn_entry(
      get_tapi_ctx(), 0, EVENT_UPDATE_FDN_ENTRIES_DONE, 1, "cmcc",
      "1008601", "1234", tele_phonebook_async_fun);
  if (ret)
    {
      syslog(
          LOG_ERR,
          "tapi_phonebook_update_fdn_entry execute fail in %s, ret: %d",
          __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_phonebook_async_fun is not executed in %s",
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

int tapi_phonebook_delete_fdn_entry_test(int slot_id)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_DELETE_FDN_ENTRIES_DONE;
  int ret = tapi_phonebook_delete_fdn_entry(
      get_tapi_ctx(), 0, EVENT_DELETE_FDN_ENTRIES_DONE, 1, "1234",
      tele_phonebook_async_fun);
  if (ret)
    {
      syslog(
          LOG_ERR,
          "tapi_phonebook_delete_fdn_entry execute fail in %s, ret: %d",
          __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "tele_phonebook_async_fun is not executed in %s",
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
