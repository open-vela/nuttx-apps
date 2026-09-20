/****************************************************************************
 * apps/feishu_notify/feishu_internal.h
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

#ifndef __APPS_FEISHU_NOTIFY_FEISHU_INTERNAL_H
#define __APPS_FEISHU_NOTIFY_FEISHU_INTERNAL_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "include/feishu_notify.h"

#include <stddef.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/* TLS lifecycle (defined in feishu_http.c). feishu_tls_init() is idempotent. */

int feishu_tls_init(void);
void feishu_tls_deinit(void);

/* Perform an HTTPS POST to open.feishu.cn and collect the response body.
 *
 *   path       URL path starting with '/' (e.g. "/open-apis/im/v1/messages")
 *   bearer     optional "Bearer" access token, or NULL for unauthenticated
 *              requests (e.g. tenant_access_token)
 *   json_body  request body, or NULL for an empty body
 *   resp_buf   caller buffer to receive the response body (NUL-terminated)
 *   resp_cap   capacity of resp_buf in bytes
 *
 * Returns the HTTP status code (200, 400, ...) on success, or -1 on a
 * transport/TLS error. The response body is always NUL-terminated.
 */

int feishu_notify_https_post(const char *path, const char *bearer,
                             const char *json_body, char *resp_buf,
                             size_t resp_cap);

#ifdef __cplusplus
}
#endif

#endif /* __APPS_FEISHU_NOTIFY_FEISHU_INTERNAL_H */
