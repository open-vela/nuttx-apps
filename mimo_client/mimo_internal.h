/****************************************************************************
 * apps/mimo_client/mimo_internal.h
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ****************************************************************************/

#ifndef __APPS_MIMO_CLIENT_MIMO_INTERNAL_H
#define __APPS_MIMO_CLIENT_MIMO_INTERNAL_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "include/mimo_client.h"

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* Modality callback: build the JSON request body for a given modality.
 * On success returns a malloc'd string (caller frees).
 */

typedef char *(*mimo_build_body_t)(const struct mimo_req_s *req);

/* Modality callback: parse the response for a given modality.
 * Called after the generic parse, for modality-specific fields (e.g. audio).
 */

typedef int (*mimo_parse_resp_t)(const char *json_str,
                                 struct mimo_rsp_s *rsp);

/* Modality configuration entry */

struct mimo_modality_cfg_s
{
  const char        *model;
  int                timeout_ms;
  mimo_build_body_t  build_body;
  mimo_parse_resp_t  parse_resp;
};

/****************************************************************************
 * Public Data
 ****************************************************************************/

/* Modality table indexed by enum mimo_modality_e */

extern const struct mimo_modality_cfg_s
  g_modality_table[MIMO_MODALITY_MAX];

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/* HTTP/TLS POST -- defined in mimo_http.c */

int mimo_http_post(const char *json_body, struct mimo_rsp_s *rsp);

/* Base64 encoder -- defined in mimo_vision.c */

int mimo_base64_encode(const uint8_t *src, size_t slen, char *dst,
                       size_t dlen);

#endif /* __APPS_MIMO_CLIENT_MIMO_INTERNAL_H */
