/****************************************************************************
 * apps/feishu_notify/feishu_notify.c
 *
 * Minimal Feishu (Lark) notification client: fetch a tenant_access_token and
 * send text messages (to a group chat or as a direct message) via the Feishu
 * OpenAPI.
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <netutils/cJSON.h>

#include "feishu_internal.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Credentials: prefer board_secrets.h (gitignored) over Kconfig fallback.
 * This mirrors the MIMO_CLIENT_USE_BOARD_SECRETS pattern in mimo_client.
 */

#ifdef CONFIG_FEISHU_NOTIFY_USE_BOARD_SECRETS
#  include <arch/board/board_secrets.h>
#  define FEISHU_APP_ID     BOARD_FEISHU_APP_ID
#  define FEISHU_APP_SECRET  BOARD_FEISHU_APP_SECRET
#  define FEISHU_CHAT_ID    BOARD_FEISHU_CHAT_ID
#  define FEISHU_OPEN_ID    BOARD_FEISHU_OPEN_ID
#else
#  define FEISHU_APP_ID     CONFIG_FEISHU_NOTIFY_APP_ID
#  define FEISHU_APP_SECRET  CONFIG_FEISHU_NOTIFY_APP_SECRET
#  define FEISHU_CHAT_ID    CONFIG_FEISHU_NOTIFY_CHAT_ID
#  define FEISHU_OPEN_ID    CONFIG_FEISHU_NOTIFY_OPEN_ID
#endif

#define FEISHU_TOKEN_PATH \
  "/open-apis/auth/v3/tenant_access_token/internal"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static char s_access_token[512];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* Returns true if resp looks like an expired/invalid access token error. */

static bool is_token_error(const char *resp)
{
  return resp && strstr(resp, "access token") != NULL;
}

/* Send a text message to a Feishu receiver. receive_id_type is one of
 * "chat_id", "open_id", "user_id" or "email"; receive_id is the matching
 * receiver identifier (chat_id for groups, open_id for direct messages).
 */

static int feishu_notify_send_message(const char *receive_id_type,
                                      const char *receive_id,
                                      const char *text)
{
  cJSON *content;
  cJSON *body;
  char *content_str;
  char *body_str;
  char path[160];
  char resp[1024];
  int status;

  if (!receive_id_type || !receive_id || !text)
    {
      return -1;
    }

  /* Fetch a token lazily on first use. */

  if (s_access_token[0] == '\0' && feishu_notify_get_token() != 0)
    {
      printf("[feishu_notify] cannot send: token fetch failed\n");
      return -1;
    }

  /* content is itself a JSON string: {"text": "..."} */

  content = cJSON_CreateObject();
  cJSON_AddStringToObject(content, "text", text);
  content_str = cJSON_PrintUnformatted(content);
  cJSON_Delete(content);
  if (!content_str)
    {
      return -1;
    }

  body = cJSON_CreateObject();
  cJSON_AddStringToObject(body, "receive_id", receive_id);
  cJSON_AddStringToObject(body, "msg_type", "text");
  cJSON_AddStringToObject(body, "content", content_str);
  body_str = cJSON_PrintUnformatted(body);
  cJSON_Delete(body);
  free(content_str);
  if (!body_str)
    {
      return -1;
    }

  snprintf(path, sizeof(path),
           "/open-apis/im/v1/messages?receive_id_type=%s", receive_id_type);

  status = feishu_notify_https_post(path, s_access_token,
                                    body_str, resp, sizeof(resp));

  /* On an invalid token, refresh once and retry. */

  if ((status == 400 || status == 401) && is_token_error(resp) &&
      feishu_notify_get_token() == 0)
    {
      status = feishu_notify_https_post(path, s_access_token,
                                        body_str, resp, sizeof(resp));
    }

  free(body_str);

  if (status != 200)
    {
      printf("[feishu_notify] send HTTP %d: %.200s\n", status, resp);
      return -1;
    }

  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int feishu_notify_init(void)
{
  return feishu_tls_init();
}

void feishu_notify_deinit(void)
{
  feishu_tls_deinit();
}

int feishu_notify_get_token(void)
{
  char body[256];
  char resp[1024];
  cJSON *root;
  cJSON *code;
  cJSON *tok;
  int status;

  snprintf(body, sizeof(body), "{\"app_id\":\"%s\",\"app_secret\":\"%s\"}",
           FEISHU_APP_ID, FEISHU_APP_SECRET);

  status = feishu_notify_https_post(FEISHU_TOKEN_PATH, NULL, body,
                                    resp, sizeof(resp));
  if (status != 200)
    {
      printf("[feishu_notify] tenant_access_token HTTP %d: %.200s\n",
             status, resp);
      return -1;
    }

  root = cJSON_Parse(resp);
  if (!root)
    {
      printf("[feishu_notify] tenant_access_token bad JSON\n");
      return -1;
    }

  code = cJSON_GetObjectItem(root, "code");
  if (cJSON_IsNumber(code) && code->valueint != 0)
    {
      printf("[feishu_notify] tenant_access_token code=%d\n",
             (int)code->valueint);
      cJSON_Delete(root);
      return -1;
    }

  tok = cJSON_GetObjectItem(root, "tenant_access_token");
  if (!cJSON_IsString(tok) || !tok->valuestring[0])
    {
      printf("[feishu_notify] no tenant_access_token in response\n");
      cJSON_Delete(root);
      return -1;
    }

  strncpy(s_access_token, tok->valuestring, sizeof(s_access_token) - 1);
  s_access_token[sizeof(s_access_token) - 1] = '\0';
  cJSON_Delete(root);

  printf("[feishu_notify] tenant_access_token refreshed\n");
  return 0;
}

const char *feishu_notify_get_access_token(void)
{
  return s_access_token;
}

int feishu_notify_send_text(const char *chat_id, const char *text)
{
  return feishu_notify_send_message("chat_id", chat_id, text);
}

int feishu_notify_send_dm(const char *open_id, const char *text)
{
  return feishu_notify_send_message("open_id", open_id, text);
}

const char *feishu_notify_get_default_chat_id(void)
{
  return (FEISHU_CHAT_ID[0] == '\0') ? NULL : FEISHU_CHAT_ID;
}

const char *feishu_notify_get_default_open_id(void)
{
  return (FEISHU_OPEN_ID[0] == '\0') ? NULL : FEISHU_OPEN_ID;
}
