/****************************************************************************
 * apps/testing/testsuites/kernel/telephony/telephony_data_test.c
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
#include "telephony_data_test.h"

extern struct judge_type judge_data;
static struct
{
  int data_enabled_watch_id;
  int data_connection_state_change_watch_id;
  int data_on;
  int connection_state;
} global_data;

static void data_event_response(tapi_async_result *result);
static void data_signal_change(tapi_async_result *result);

int tapi_data_listen_data_test(int slot_id)
{
  global_data.data_enabled_watch_id = -1;
  global_data.data_enabled_watch_id = tapi_data_register(
      get_tapi_ctx(), slot_id, MSG_DATA_ENABLED_CHANGE_IND, NULL,
      data_signal_change);

  if (global_data.data_enabled_watch_id < 0)
    {
      syslog(LOG_ERR, "%s, slot_id: %d, target_state: %d, watch_id: %d",
             __func__, slot_id, MSG_DATA_ENABLED_CHANGE_IND,
             global_data.data_enabled_watch_id);
      return -1;
    }

  global_data.data_connection_state_change_watch_id = -1;
  global_data.data_connection_state_change_watch_id = tapi_data_register(
      get_tapi_ctx(), slot_id, MSG_DATA_CONNECTION_STATE_CHANGE_IND,
      NULL, data_signal_change);
  if (global_data.data_connection_state_change_watch_id < 0)
    {
      syslog(LOG_DEBUG,
             "%s, slot_id: %d, target_state: %d, watch_id: %d", __func__,
             slot_id, MSG_DATA_CONNECTION_STATE_CHANGE_IND,
             global_data.data_connection_state_change_watch_id);
      return -1;
    }

  return 0;
}

int tapi_data_unlisten_data_test(void)
{
  int ret = -1;
  int res = 0;
  syslog(LOG_INFO,
         "data enabled watch id: %d, data connection state change watch "
         "id: %d",
         global_data.data_enabled_watch_id,
         global_data.data_connection_state_change_watch_id);
  ret = tapi_data_unregister(get_tapi_ctx(),
                             global_data.data_enabled_watch_id);
  if (ret)
    {
      syslog(LOG_ERR,
             "unregister data enable change fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  ret = tapi_data_unregister(
      get_tapi_ctx(), global_data.data_connection_state_change_watch_id);
  if (ret)
    {
      syslog(
          LOG_ERR,
          "unregister data connection state change fail in %s, ret: %d",
          __func__, ret);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

int tapi_data_load_apn_contexts_test(int slot_id)
{
  int ret = -1;
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_APN_LOADED_DONE;
  ret = tapi_data_load_apn_contexts(get_tapi_ctx(), slot_id,
                                    EVENT_APN_LOADED_DONE,
                                    data_event_response);

  if (ret)
    {
      syslog(
          LOG_ERR,
          "tapi_data_load_apn_contexts_test execute fail in %s, ret: %d",
          __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "data_event_response called by %s is not execute",
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

int tapi_data_save_apn_context_test(char *slot_id, char *type,
                                    char *name, char *apn, char *proto,
                                    char *auth)
{
  int ret = -1;
  int res = 0;
  tapi_data_context *apn_context;
  judge_data_init();
  judge_data.expect = EVENT_APN_ADD_DONE;
  apn_context = malloc(sizeof(tapi_data_context));
  if (apn_context == NULL)
    {
      syslog(LOG_DEBUG, "apn_contex is null in %s", __func__);
      return -EINVAL;
    }

  apn_context->type = atoi(type);
  apn_context->protocol = atoi(proto);
  apn_context->auth_method = atoi(auth);

  if (strlen(name) <= MAX_APN_DOMAIN_LENGTH)
    strcpy(apn_context->name, name);

  if (strlen(apn) <= MAX_APN_DOMAIN_LENGTH)
    strcpy(apn_context->accesspointname, apn);

  strcpy(apn_context->username, "");
  strcpy(apn_context->password, "");

  ret = tapi_data_add_apn_context(get_tapi_ctx(), atoi(slot_id),
                                  EVENT_APN_ADD_DONE, apn_context,
                                  data_event_response);
  free(apn_context);
  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_data_add_apn_context execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_DEBUG, "data_event_response is not executed in %s",
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

int tapi_data_edit_apn_context_test(char *slot_id, char *id, char *type,
                                    char *name, char *apn, char *proto,
                                    char *auth)
{
  int ret = -1;
  tapi_data_context *apn_context;
  judge_data_init();
  judge_data.expect = EVENT_APN_EDIT_DONE;
  apn_context = malloc(sizeof(tapi_data_context));
  if (apn_context == NULL)
    {
      syslog(LOG_ERR, "apn_context is null in %s", __func__);
      return -EINVAL;
    }

  apn_context->id = id;
  apn_context->type = atoi(type);
  apn_context->protocol = atoi(proto);
  apn_context->auth_method = atoi(auth);

  if (strlen(name) <= MAX_APN_DOMAIN_LENGTH)
    strcpy(apn_context->name, name);

  if (strlen(apn) <= MAX_APN_DOMAIN_LENGTH)
    strcpy(apn_context->accesspointname, apn);

  strcpy(apn_context->username, "");
  strcpy(apn_context->password, "");

  ret = tapi_data_edit_apn_context(get_tapi_ctx(), atoi(slot_id),
                                   EVENT_APN_EDIT_DONE, apn_context,
                                   data_event_response);
  free(apn_context);
  judge();

  return ret || judge_data.result ||
         (judge_data.expect != judge_data.flag);
}

int tapi_data_remove_apn_context_test(char *slot_id, char *id)
{
  int ret = -1;
  int res = 0;
  tapi_data_context *apn;
  judge_data_init();
  judge_data.expect = EVENT_APN_REMOVAL_DONE;

  apn = malloc(sizeof(tapi_data_context));
  if (apn == NULL)
    {
      syslog(LOG_ERR, "apn is null in %s", __func__);
      return ret;
    }

  apn->id = id;
  ret = tapi_data_remove_apn_context(get_tapi_ctx(), atoi(slot_id),
                                     EVENT_APN_REMOVAL_DONE, apn,
                                     data_event_response);
  free(apn);
  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_data_remove_apn_context execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "data_event_response is not executed in %s",
             __func__);
      res = -1;
      goto on_exit;
    }

  if (judge_data.result)
    {
      syslog(LOG_DEBUG, "async result is invalid in %s", __func__);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

int tapi_data_reset_apn_contexts_test(char *slot_id)
{
  int ret = -1;
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_APN_RESTORE_DONE;

  ret = tapi_data_reset_apn_contexts(get_tapi_ctx(), atoi(slot_id),
                                     EVENT_APN_RESTORE_DONE,
                                     data_event_response);
  if (ret)
    {
      syslog(LOG_ERR, "tapi_data_reset_apn_contexts execute fail in %s",
             __func__);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "data_event_response is not executed in %s",
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

int tapi_data_request_network_test(int slot_id, char *target_state)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = MSG_DATA_CONNECTION_STATE_CHANGE_IND;
  global_data.connection_state = -100;

  int ret =
      tapi_data_request_network(get_tapi_ctx(), slot_id, target_state);
  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_data_request_network execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR,
             "the callback function of request_network is not executed "
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

  if (global_data.connection_state != 1)
    {
      syslog(LOG_ERR, "connection state is error in %s", __func__);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

int tapi_data_release_network_test(int slot_id, char *target_state)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = MSG_DATA_CONNECTION_STATE_CHANGE_IND;
  global_data.connection_state = -100;
  int ret =
      tapi_data_release_network(get_tapi_ctx(), slot_id, target_state);
  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_data_release_network execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR,
             "the callback function of release_network is not executed "
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

  if (global_data.connection_state != 0)
    {
      syslog(LOG_ERR, "connection state is error in %s", __func__);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

int tapi_data_enable_test(int state)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = MSG_DATA_ENABLED_CHANGE_IND;
  global_data.data_on = -1;
  int ret = tapi_data_enable_data(get_tapi_ctx(), state);
  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_data_enable_data execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(
          LOG_ERR,
          "the callback function of data_enable is not executed in %s",
          __func__);
      res = -1;
      goto on_exit;
    }

  if (judge_data.result)
    {
      syslog(LOG_ERR, "async result in %s is invalid", __func__);
      res = -1;
      goto on_exit;
    }

  if (global_data.data_on != state)
    {
      syslog(LOG_ERR, "data status is error");
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

/* is - data - on */

int tapi_data_get_enabled_test(bool *result)
{
  int ret = tapi_data_get_enabled(get_tapi_ctx(), result);
  bool flag = *result;
  syslog(LOG_DEBUG, "ret: %d, result: %d", ret, (int)flag);

  return ret;
}

int tapi_data_is_ps_attached_test(int slot_id)
{
  bool result = false;
  int ret = tapi_data_is_registered(get_tapi_ctx(), slot_id, &result);
  return ret || !result;
}

int tapi_data_get_network_type_test(int slot_id)
{
  tapi_network_type result = NETWORK_TYPE_UNKNOWN;
  int ret = tapi_data_get_network_type(get_tapi_ctx(), slot_id, &result);
  return ret || result != NETWORK_TYPE_LTE;
}

int tapi_data_enable_roaming_test(int state)
{
  int ret = tapi_data_enable_roaming(get_tapi_ctx(), state);
  return ret;
}

bool tapi_data_get_roaming_enabled_test(bool *result)
{
  int ret = tapi_data_get_roaming_enabled(get_tapi_ctx(), result);
  return ret;
}

int tapi_data_set_preferred_apn_test(int slot_id, char *apn_id)
{
  tapi_data_context *apn = malloc(sizeof(tapi_data_context));
  if (apn == NULL)
    {
      syslog(LOG_ERR, "apn is null in %s", __func__);
      return -1;
    }

  apn->id = apn_id;
  int ret = tapi_data_set_preferred_apn(get_tapi_ctx(), slot_id, apn);
  free(apn);
  return ret;
}

int tapi_data_get_preferred_apn_test(int slot_id)
{
  sleep(2);
  char *apn = NULL;
  int ret = tapi_data_get_preferred_apn(get_tapi_ctx(), slot_id, &apn);
  syslog(LOG_DEBUG, "%s: %s", __func__, apn);

  return ret || strcmp(apn, "/ril_0/context1") != 0;
}

int tapi_data_set_default_data_slot_test(int slot_id)
{
  int ret = tapi_data_set_default_slot(get_tapi_ctx(), slot_id);
  return ret;
}

int tapi_data_get_default_data_slot_test(void)
{
  int result = -1;
  int ret = tapi_data_get_default_slot(get_tapi_ctx(), &result);
  return ret || result != 0;
}

int tapi_data_set_data_allow_test(int slot_id)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_DATA_ALLOWED_DONE;
  int ret = tapi_data_set_data_allow(get_tapi_ctx(), slot_id,
                                     EVENT_DATA_ALLOWED_DONE, 1,
                                     data_event_response);
  if (ret)
    {
      syslog(LOG_ERR,
             "tapi_data_set_data_allow execute fail in %s, ret: %d",
             __func__, ret);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "data_event_response is not executed in %s",
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

int tapi_data_send_screen_stat_test(int slot_id)
{
  int ret = tapi_set_fast_dormancy(get_tapi_ctx(), slot_id,
                                   EVENT_REQUEST_SCREEN_STATE_DONE, 1,
                                   data_event_response);
  sleep(4);

  return ret;
}

int tapi_data_get_data_call_list_test(int slot_id)
{
  int res = 0;
  judge_data_init();
  judge_data.expect = EVENT_DATA_CALL_LIST_QUERY_DONE;

  int ret = tapi_data_get_data_connection_list(
      get_tapi_ctx(), slot_id, EVENT_DATA_CALL_LIST_QUERY_DONE,
      data_event_response);
  if (ret)
    {
      syslog(LOG_DEBUG,
             "tapi_data_get_data_connection_list execute fail in %s",
             __func__);
      res = -1;
      goto on_exit;
    }

  if (judge())
    {
      syslog(LOG_ERR, "data_event_response in %s is not executed",
             __func__);
      res = -1;
      goto on_exit;
    }

  if (judge_data.result)
    {
      syslog(LOG_DEBUG, "async result is invalid in %s", __func__);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

static void data_event_response(tapi_async_result *result)
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
    case EVENT_APN_LOADED_DONE:
      if (judge_data.expect == EVENT_APN_LOADED_DONE)
        {
          judge_data.result = status;
          judge_data.flag = EVENT_APN_LOADED_DONE;
        }

      break;
    case EVENT_APN_ADD_DONE:
      if (judge_data.expect == EVENT_APN_ADD_DONE)
        {
          judge_data.result = status;
          judge_data.flag = EVENT_APN_ADD_DONE;
        }

      break;
    case EVENT_APN_EDIT_DONE:
      if (judge_data.expect == EVENT_APN_EDIT_DONE)
        {
          judge_data.result = status;
          judge_data.flag = EVENT_APN_EDIT_DONE;
        }

      break;
    case EVENT_APN_REMOVAL_DONE:
      if (judge_data.expect == EVENT_APN_REMOVAL_DONE)
        {
          judge_data.result = status;
          judge_data.flag = EVENT_APN_REMOVAL_DONE;
        }

      break;
    case EVENT_APN_RESTORE_DONE:
      if (judge_data.expect == EVENT_APN_RESTORE_DONE)
        {
          judge_data.result = status;
          judge_data.flag = EVENT_APN_RESTORE_DONE;
        }

      break;
    case EVENT_DATA_ALLOWED_DONE:
      if (judge_data.expect == EVENT_DATA_ALLOWED_DONE)
        {
          judge_data.result = status;
          judge_data.flag = EVENT_DATA_ALLOWED_DONE;
        }

    case EVENT_REQUEST_SCREEN_STATE_DONE:
      if (judge_data.expect == EVENT_REQUEST_SCREEN_STATE_DONE)
        {
          judge_data.result = status;
          judge_data.flag = EVENT_REQUEST_SCREEN_STATE_DONE;
        }

      break;
    case EVENT_DATA_CALL_LIST_QUERY_DONE:
      if (judge_data.expect == EVENT_DATA_CALL_LIST_QUERY_DONE)
        {
          judge_data.result = status;
          judge_data.flag = EVENT_DATA_CALL_LIST_QUERY_DONE;
        }

    default:
      break;
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

static void data_signal_change(tapi_async_result *result)
{
  tapi_data_context *dc;
  tapi_ipv4_settings *ipv4;
  tapi_ipv6_settings *ipv6;
  int signal = result->msg_id;
  int slot_id = result->arg1;
  int param = result->arg2;

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

  switch (signal)
    {
    case MSG_DATA_REGISTRATION_STATE_CHANGE_IND:
      syslog(LOG_DEBUG,
             "data registration state changed to %d in slot[%d] \n",
             param, slot_id);
      if (judge_data.expect == MSG_DATA_REGISTRATION_STATE_CHANGE_IND)
        {
          judge_data.result = OK;
          judge_data.flag = MSG_DATA_REGISTRATION_STATE_CHANGE_IND;
        }

      break;
    case MSG_DATA_NETWORK_TYPE_CHANGE_IND:
      syslog(LOG_DEBUG, "data network type changed to %d in slot[%d] \n",
             param, slot_id);
      if (judge_data.expect == MSG_DATA_NETWORK_TYPE_CHANGE_IND)
        {
          judge_data.result = OK;
          judge_data.flag = MSG_DATA_NETWORK_TYPE_CHANGE_IND;
        }

      break;
    case MSG_DEFAULT_DATA_SLOT_CHANGE_IND:
      syslog(LOG_DEBUG, "data slot changed to slot[%d] \n", param);
      if (judge_data.expect == MSG_DEFAULT_DATA_SLOT_CHANGE_IND)
        {
          judge_data.result = OK;
          judge_data.flag = MSG_DEFAULT_DATA_SLOT_CHANGE_IND;
        }

      break;
    case MSG_DATA_ENABLED_CHANGE_IND:
      if (judge_data.expect == MSG_DATA_ENABLED_CHANGE_IND)
        {
          syslog(LOG_DEBUG, "data switch changed to %d in slot[%d] \n",
                 param, slot_id);
          judge_data.result = 0;
          global_data.data_on = param;
          judge_data.flag = MSG_DATA_ENABLED_CHANGE_IND;
        }

      break;
    case MSG_DATA_CONNECTION_STATE_CHANGE_IND:
      if (judge_data.expect == MSG_DATA_CONNECTION_STATE_CHANGE_IND)
        {
          dc = result->data;
          if (dc != NULL)
            {
              syslog(LOG_DEBUG, "id (apn_path) = %s \n", dc->id);
              syslog(LOG_DEBUG, "type = %s \n",
                     tapi_utils_apn_type_to_string(dc->type));
              syslog(LOG_DEBUG, "active = %d \n", dc->active);
              global_data.connection_state = dc->active;
              if (dc->ip_settings != NULL)
                {
                  ipv4 = dc->ip_settings->ipv4;
                  if (ipv4 != NULL)
                    {
                      syslog(LOG_DEBUG,
                             "ipv4-interface = %s; ip = %s; gateway = "
                             "%s; dns[0] = %s; \n",
                             ipv4->interface, ipv4->ip, ipv4->gateway,
                             ipv4->dns[0]);
                    }

                  ipv6 = dc->ip_settings->ipv6;
                  if (ipv6 != NULL)
                    {
                      syslog(LOG_DEBUG,
                             "ipv6-interface = %s; ip = %s; gateway = "
                             "%s; dns[0] = %s; \n",
                             ipv6->interface, ipv6->ip, ipv6->gateway,
                             ipv6->dns[0]);
                    }
                }
            }

          judge_data.result = 0;
          judge_data.flag = MSG_DATA_CONNECTION_STATE_CHANGE_IND;
        }

      break;
    default:
      break;
    }
}

int data_enabled_test(int slot_id)
{
  int ret = tapi_data_enable_test(1);
  syslog(LOG_DEBUG, "%s, ret: %d", __func__, ret);

  return ret;
}

int data_disabled_test(int slot_id)
{
  int ret = tapi_data_enable_test(0);
  syslog(LOG_DEBUG, "%s, ret: %d", __func__, ret);

  return ret;
}

int data_release_network_test(int slot_id)
{
  int ret = tapi_data_release_network_test(slot_id, "internet");
  syslog(LOG_DEBUG, "%s, slot_id: %d, ret: %d", __func__, slot_id, ret);

  return ret;
}

int data_request_network_test(int slot_id)
{
  int ret = tapi_data_request_network_test(slot_id, "internet");
  syslog(LOG_DEBUG, "%s, slot_id: %d, ret: %d", __func__, slot_id, ret);

  return ret;
}

int data_request_ims(int slot_id)
{
  int ret = tapi_data_request_network_test(slot_id, "ims");
  syslog(LOG_DEBUG, "%s, slot_id: %d, ret: %d", __func__, slot_id, ret);

  return ret;
}

int data_release_ims(int slot_id)
{
  int ret = tapi_data_release_network_test(slot_id, "ims");
  syslog(LOG_DEBUG, "%s, slot_id: %d, ret: %d", __func__, slot_id, ret);

  return ret;
}

int data_get_call_list(int slot_id)
{
  int ret = -1;
  int res = 0;

  ret = tapi_data_enable_test(1);
  if (ret)
    {
      syslog(LOG_ERR, "enable data failed in %s, ret: %d", __func__,
             ret);
      res = -1;
      goto on_exit;
    }

  ret = tapi_data_get_data_call_list_test(slot_id);
  if (ret)
    {
      syslog(LOG_ERR, "get data call list failed in %s", __func__);
      res = -1;
      goto on_exit;
    }

  ret = tapi_data_enable_test(0);
  if (ret)
    {
      syslog(LOG_ERR, "disable data failed in %s", __func__);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

int data_enable_roaming_test(void)
{
  bool result = false;
  int ret = -1;
  int res = 0;
  ret = tapi_data_enable_roaming_test(true);
  if (ret)
    {
      syslog(LOG_ERR, "enable data roaming failed");
      res = -1;
      goto on_exit;
    }

  sleep(3);
  ret = tapi_data_get_roaming_enabled_test(&result);
  if (ret)
    {
      syslog(LOG_ERR, "get data roaming failed in %s", __func__);
      res = -1;
      goto on_exit;
    }

  if (!result)
    {
      syslog(LOG_ERR, "data roaming state is error in %s, state: %d",
             __func__, (int)result);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}

int data_disable_roaming_test(void)
{
  bool result = true;
  int ret = -1;
  int res = 0;
  ret = tapi_data_enable_roaming_test(false);
  if (ret)
    {
      syslog(LOG_ERR, "disable data roaming failed in %s", __func__);
      res = -1;
      goto on_exit;
    }

  sleep(3);
  ret = tapi_data_get_roaming_enabled_test(&result);
  if (ret)
    {
      syslog(LOG_ERR, "get data roaming failed in %s", __func__);
      res = -1;
      goto on_exit;
    }

  if (result)
    {
      syslog(LOG_ERR, "data roaming state is error in %s, state: %d",
             __func__, (int)result);
      res = -1;
      goto on_exit;
    }

on_exit:
  return res;
}
