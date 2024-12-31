/****************************************************************************
 * apps/testing/testsuites/kernel/telephony/telephony_ims_test.c
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
#include "telephony_ims_test.h"

static void tele_ims_async_fun(tapi_async_result *result);
extern struct judge_type judge_data;

static struct
{
  int ims_enabled;
  int ims_reg_watch_id;
} global_data;

int tapi_ims_listen_ims_test(int slot_id)
{
  global_data.ims_reg_watch_id = -1;
  global_data.ims_reg_watch_id = tapi_ims_register_registration_change(
      get_tapi_ctx(), slot_id, NULL, tele_ims_async_fun);

  if (global_data.ims_reg_watch_id < 0)
    {
      syslog(LOG_ERR, "%s, slot_id: %d, watch_id < 0\n", __func__,
             slot_id);
      return -1;
    }

  return 0;
}

int tapi_ims_turn_on_test(int slot_id)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = IMS_REG;
  global_data.ims_enabled = -1;
  int ret = tapi_ims_turn_on(get_tapi_ctx(), slot_id);
  if (ret)
    {
      syslog(LOG_ERR, "tapi_ims_turn_on execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "the callback function is not executed in %s",
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

  if (global_data.ims_enabled != 1)
    {
      syslog(LOG_ERR, "ims enabled is error in %s", __func__);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

int tapi_ims_turn_off_test(int slot_id)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = IMS_REG;
  global_data.ims_enabled = -1;
  int ret = tapi_ims_turn_off(get_tapi_ctx(), slot_id);
  if (ret)
    {
      syslog(LOG_ERR, "tapi_ims_turn_off execute fail in %s", __func__);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "the callback function is not executed in %s",
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

  if (global_data.ims_enabled != 0)
    {
      syslog(LOG_ERR, "ims enabled is error in %s", __func__);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

int tapi_ims_get_registration_test(int slot_id)
{
  tapi_ims_registration_info info;
  memset(&info, 0, sizeof(info));
  int ret = tapi_ims_get_registration(get_tapi_ctx(), slot_id, &info);
  syslog(LOG_ERR, "%s, slot_id: %d, reg_info: %d", __func__, slot_id,
         info.reg_info);

  return ret || !info.reg_info;
}

int tapi_ims_set_service_status_test(int slot_id, int status)
{
  int res = 0;
  int ret = tapi_ims_set_service_status(get_tapi_ctx(), slot_id, status);
  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_ims_set_service_status execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

static void tele_ims_async_fun(tapi_async_result *result)
{
  syslog(LOG_DEBUG, "%s : \n", __func__);
  syslog(LOG_DEBUG, "result->msg_id : %d\n", result->msg_id);
  syslog(LOG_DEBUG, "result->status : %d\n", result->status);
  syslog(LOG_DEBUG, "result->arg1 : %d\n", result->arg1);
  syslog(LOG_DEBUG, "result->arg2 : %d\n", result->arg2);

  switch (result->arg1)
    {
    case IMS_REG:
      if (judge_data.expect == IMS_REG)
        {
          syslog(LOG_DEBUG, "IMS_REG change : %d", result->arg2);
          judge_data.result = 0;
          global_data.ims_enabled = result->arg2;
          judge_data.flag = IMS_REG;
        }
      break;
      break;
    default:
      break;
    }
}

int tapi_ims_get_enabled_test(int slot_id)
{
  bool result = false;
  int ret = tapi_ims_get_enabled(get_tapi_ctx(), slot_id, &result);
  syslog(LOG_DEBUG, "%s, slot_id: %d, ims_enable: %d", __func__, slot_id,
         result);

  return ret || !result;
}
