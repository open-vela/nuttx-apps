/****************************************************************************
 * apps/mimo_client/mimo_tts.c
 *
 * TTS modality stub. Implemented in M5 (TTS/ASR audio).
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <string.h>
#include <stdio.h>
#include <netutils/cJSON.h>
#include "mimo_internal.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

char *mimo_stub_build_body(const struct mimo_req_s *req)
{
  /* TTS/ASR: return an error body for now.
   * Full implementation in M5.
   */

  cJSON *root;
  char *body;

  (void)req;

  root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "model",
    req->modality == MIMO_TTS ? "mimo-v2.5-tts" : "mimo-v2.5-asr");
  cJSON_AddStringToObject(root, "error", "not implemented yet");

  body = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  return body;
}

int mimo_stub_parse_resp(const char *json_str, struct mimo_rsp_s *rsp)
{
  (void)json_str;
  (void)rsp;
  return 0;
}
