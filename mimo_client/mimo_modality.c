/****************************************************************************
 * apps/mimo_client/mimo_modality.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "mimo_internal.h"

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/* Forward declarations from modality modules */

char *mimo_text_build_body(const struct mimo_req_s *req);
int   mimo_text_parse_resp(const char *json_str, struct mimo_rsp_s *rsp);

char *mimo_vision_build_body(const struct mimo_req_s *req);
int   mimo_vision_parse_resp(const char *json_str, struct mimo_rsp_s *rsp);

char *mimo_stub_build_body(const struct mimo_req_s *req);
int   mimo_stub_parse_resp(const char *json_str, struct mimo_rsp_s *rsp);

/****************************************************************************
 * Public Data
 ****************************************************************************/

const struct mimo_modality_cfg_s g_modality_table[MIMO_MODALITY_MAX] =
{
  [MIMO_TEXT]   =
  {
    .model      = "mimo-v2.5",
    .timeout_ms = 30000,
    .build_body = mimo_text_build_body,
    .parse_resp = mimo_text_parse_resp,
  },
  [MIMO_VISION] =
  {
    .model      = "mimo-v2.5",
    .timeout_ms = 60000,
    .build_body = mimo_vision_build_body,
    .parse_resp = mimo_vision_parse_resp,
  },
  [MIMO_TTS] =
  {
    .model      = "mimo-v2.5-tts",
    .timeout_ms = 60000,
    .build_body = mimo_stub_build_body,
    .parse_resp = mimo_stub_parse_resp,
  },
  [MIMO_ASR] =
  {
    .model      = "mimo-v2.5-asr",
    .timeout_ms = 60000,
    .build_body = mimo_stub_build_body,
    .parse_resp = mimo_stub_parse_resp,
  },
};
