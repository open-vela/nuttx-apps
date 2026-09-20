/****************************************************************************
 * apps/feishu_notify/include/feishu_notify.h
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

#ifndef __APPS_FEISHU_NOTIFY_INCLUDE_FEISHU_NOTIFY_H
#define __APPS_FEISHU_NOTIFY_INCLUDE_FEISHU_NOTIFY_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Feishu (Lark) OpenAPI host. Both the tenant_access_token and im/v1/messages
 * endpoints live on this host over HTTPS.
 */

#define FEISHU_OPENAPI_HOST "open.feishu.cn"
#define FEISHU_OPENAPI_PORT "443"

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/* Initialize the TLS context (mbedTLS + entropy + ctr_drbg). Idempotent.
 * feishu_notify_send_text() calls this implicitly, so callers that only send
 * do not need to initialize first.
 */

int feishu_notify_init(void);

/* Release the TLS context. Safe to call at shutdown. */

void feishu_notify_deinit(void);

/* Fetch and cache the tenant_access_token using the configured App ID /
 * App Secret (from board_secrets.h or Kconfig fallback). Returns 0 on
 * success, < 0 on error.
 */

int feishu_notify_get_token(void);

/* Return the cached tenant_access_token (empty string if not fetched). */

const char *feishu_notify_get_access_token(void);

/* Send a text message to a Feishu chat. chat_id is the group chat's chat_id
 * (receive_id_type=chat_id). The tenant_access_token is fetched lazily and
 * refreshed transparently on token expiry. Returns 0 on success, < 0 on
 * error.
 */

int feishu_notify_send_text(const char *chat_id, const char *text);

/* Send a text message as a direct message to a user. open_id is the user's
 * open_id (receive_id_type=open_id). Returns 0 on success, < 0 on error.
 */

int feishu_notify_send_dm(const char *open_id, const char *text);

/* Return the default target chat_id from board_secrets.h or Kconfig, or NULL
 * if none is configured. Useful for auto-push callers (e.g. campilot).
 */

const char *feishu_notify_get_default_chat_id(void);

/* Return the default DM recipient open_id from board_secrets.h or Kconfig,
 * or NULL if none is configured.
 */

const char *feishu_notify_get_default_open_id(void);

#ifdef __cplusplus
}
#endif

#endif /* __APPS_FEISHU_NOTIFY_INCLUDE_FEISHU_NOTIFY_H */
