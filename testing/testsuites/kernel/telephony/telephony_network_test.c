/****************************************************************************
 * apps/testing/testsuites/kernel/telephony/telephony_network_test.c
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
#include "telephony_network_test.h"
#include <unistd.h>

extern struct judge_type judge_data;

static struct
{
  int network_count;
  int reg_state;
  int serving_cell_reg;
} global_data;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

static void network_event_callback(tapi_async_result *result)
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

  tapi_operator_info **operator_list;
  tapi_operator_info *operator_info;
  tapi_registration_info *info;
  tapi_cell_identity **cell_list;
  tapi_cell_identity *cell;
  int msg;
  int index;

  msg = result->msg_id;

  switch (msg)
    {
    case EVENT_NETWORK_SCAN_DONE:
      if (judge_data.expect == EVENT_NETWORK_SCAN_DONE)
        {
          index = result->arg2;
          operator_list = result->data;

          while (--index >= 0)
            {
              operator_info = *operator_list++;
              syslog(LOG_DEBUG,
                     "id : %s, name : %s, status : %d, mcc : %s, mnc : "
                     "%s \n",
                     operator_info->id, operator_info->name,
                     operator_info->status, operator_info->mcc,
                     operator_info->mnc);
            }

          judge_data.result = 0;
          global_data.network_count = result->arg2;
          judge_data.flag = EVENT_NETWORK_SCAN_DONE;
        }

      break;
    case EVENT_REGISTER_AUTO_DONE:
      if (judge_data.expect == EVENT_REGISTER_AUTO_DONE)
        {
          judge_data.result = 0;
          judge_data.flag = EVENT_REGISTER_AUTO_DONE;
        }

      break;
    case EVENT_REGISTER_MANUAL_DONE:
      if (judge_data.expect == EVENT_REGISTER_MANUAL_DONE)
        {
          judge_data.result = 0;
          judge_data.flag = EVENT_REGISTER_MANUAL_DONE;
        }

      break;
    case EVENT_QUERY_REGISTRATION_INFO_DONE:
      if (judge_data.expect == EVENT_QUERY_REGISTRATION_INFO_DONE)
        {
          info = result->data;
          syslog(LOG_DEBUG, "%s : \n", __func__);
          if (info == NULL)
            {
              judge_data.result = -1;
              judge_data.flag = EVENT_QUERY_REGISTRATION_INFO_DONE;
              return;
            }

          syslog(
              LOG_DEBUG,
              "reg_state = %d operator_name = %s mcc = %s mnc = %s \n",
              info->reg_state, info->operator_name, info->mcc,
              info->mnc);

          judge_data.result = 0;
          global_data.reg_state = info->reg_state;
          judge_data.flag = EVENT_QUERY_REGISTRATION_INFO_DONE;
        }

      break;
    case EVENT_QUERY_SERVING_CELL_DONE:
      if (judge_data.expect == EVENT_QUERY_SERVING_CELL_DONE)
        {
          index = result->arg2;
          cell_list = result->data;

          while (--index >= 0)
            {
              cell = *cell_list++;
              syslog(LOG_DEBUG,
                     "ci : %d, mcc : %s, mnc : %s, registered : %d, "
                     "type : %d, \n",
                     cell->ci, cell->mcc_str, cell->mnc_str,
                     cell->registered, cell->type);
              if (cell->registered == 1)
                {
                  global_data.serving_cell_reg = 1;
                }
            }

          judge_data.result = 0;
          judge_data.flag = EVENT_QUERY_SERVING_CELL_DONE;
        }

      break;
    case EVENT_QUERY_NEIGHBOURING_CELL_DONE:
      if (judge_data.expect == EVENT_QUERY_NEIGHBOURING_CELL_DONE)
        {
          index = result->arg2;
          cell_list = result->data;

          while (--index >= 0)
            {
              cell = *cell_list++;
              syslog(LOG_DEBUG,
                     "ci : %d, mcc : %s, mnc : %s, registered : %d, "
                     "type : %d, \n",
                     cell->ci, cell->mcc_str, cell->mnc_str,
                     cell->registered, cell->type);
            }

          judge_data.result = 0;
          judge_data.flag = EVENT_QUERY_NEIGHBOURING_CELL_DONE;
        }

      break;
    default:
      break;
    }
}

int tapi_net_select_auto_test(int slot_id)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_REGISTER_AUTO_DONE;
  int ret = tapi_network_select_auto(get_tapi_ctx(), slot_id,
                                     EVENT_REGISTER_AUTO_DONE,
                                     network_event_callback);
  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_network_select_auto execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "network_event_callback is not executed in %s",
             __func__);
      res = -1;
      goto on_exit;
    }

  if (judge_data.result)
    {
      syslog(LOG_ERR, "async result is error in %s", __func__);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

int tapi_net_select_manual_test(int slot_id, char *mcc, char *mnc,
                                char *tech)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_REGISTER_MANUAL_DONE;
  tapi_operator_info *network_info = malloc(sizeof(tapi_operator_info));
  if (network_info == NULL)
    {
      syslog(LOG_ERR, "tapi_operator_info is null in %s", __func__);
      return -ENOMEM;
    }

  snprintf(network_info->mcc, sizeof(network_info->mcc), "%s", mcc);
  snprintf(network_info->mnc, sizeof(network_info->mnc), "%s", mnc);
  snprintf(network_info->technology, sizeof(network_info->technology),
           "%s", tech);

  int ret = tapi_network_select_manual(
      get_tapi_ctx(), slot_id, EVENT_REGISTER_MANUAL_DONE, network_info,
      network_event_callback);
  free(network_info);
  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_network_select_manual execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "network_event_callback is not executed in %s",
             __func__);
      res = -1;
      goto on_exit;
    }

  if (judge_data.result)
    {
      syslog(LOG_ERR, "async result is error in %s", __func__);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

int tapi_net_scan_test(int slot_id)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_NETWORK_SCAN_DONE;
  global_data.network_count = -1;

  int ret =
      tapi_network_scan(get_tapi_ctx(), slot_id, EVENT_NETWORK_SCAN_DONE,
                        network_event_callback);
  if (ret)
    {
      syslog(LOG_ERR, "tapi_network_scan execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "network_event_callback is not executed in %s",
             __func__);
      res = -1;
      goto on_exit;
    }

  if (judge_data.result)
    {
      syslog(LOG_ERR, "async result is error in %s", __func__);
      res = -1;
      goto on_exit;
    }

  if (global_data.network_count <= 0)
    {
      syslog(LOG_ERR, "network scan error in %s", __func__);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

int tapi_net_registration_info_test(int slot_id)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_QUERY_REGISTRATION_INFO_DONE;
  global_data.reg_state = -1;
  int ret = tapi_network_get_registration_info(
      get_tapi_ctx(), slot_id, EVENT_QUERY_REGISTRATION_INFO_DONE,
      network_event_callback);
  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_network_get_registration_info execute fail in %s, "
             "ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "network_event_callback is not executed in %s",
             __func__);
      res = -1;
      goto on_exit;
    }

  if (judge_data.result)
    {
      syslog(LOG_ERR, "async result is error in %s", __func__);
      res = -1;
      goto on_exit;
    }

  if (global_data.reg_state != 1)
    {
      syslog(LOG_ERR, "reg state is error in %s", __func__);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

int tapi_net_get_serving_cellinfos_test(int slot_id)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_QUERY_SERVING_CELL_DONE;
  global_data.serving_cell_reg = -1;
  int ret = tapi_network_get_serving_cellinfos(
      get_tapi_ctx(), slot_id, EVENT_QUERY_SERVING_CELL_DONE,
      network_event_callback);
  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_network_get_serving_cellinfos execute fail in %s, "
             "ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "network_event_callback is not executed in %s",
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

  if (global_data.serving_cell_reg != 1)
    {
      syslog(LOG_ERR, "serving cell reg is error in %s", __func__);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

int tapi_net_get_neighbouring_cellinfos_test(int slot_id)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_QUERY_NEIGHBOURING_CELL_DONE;
  int ret = tapi_network_get_neighbouring_cellinfos(
      get_tapi_ctx(), slot_id, EVENT_QUERY_NEIGHBOURING_CELL_DONE,
      network_event_callback);
  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_network_get_neighbouring_cellinfos execute fail in "
             "%s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "network_event_callback is not executed in %s",
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

int tapi_net_get_operator_name_test(int slot_id)
{
  char *operator_name = NULL;
  int ret = tapi_network_get_display_name(get_tapi_ctx(), slot_id,
                                          &operator_name);
  syslog(LOG_DEBUG, "%s, slotId : %d value :%s \n", __func__, slot_id,
         operator_name);

  return ret || operator_name == NULL;
}

int tapi_net_query_signalstrength_test(int slot_id)
{
  tapi_signal_strength ss;
  memset(&ss, 0, sizeof(ss));
  int ret =
      tapi_network_get_signalstrength(get_tapi_ctx(), slot_id, &ss);
  syslog(LOG_DEBUG, "%s, slotId : %d RSRP value :%d \n", __func__,
         slot_id, ss.rsrp);

  return ret || !ss.rsrp;
}

int tapi_net_get_voice_registered_test(int slot_id)
{
  bool result = false;
  int ret =
      tapi_network_is_voice_registered(get_tapi_ctx(), slot_id, &result);
  syslog(LOG_DEBUG, "%s, slot_id: %d, voice_reg: %d", __func__, slot_id,
         (int)result);

  return ret || !result;
}
