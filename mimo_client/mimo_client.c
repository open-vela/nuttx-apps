/****************************************************************************
 * apps/mimo_client/mimo_client.c
 *
 * Main entry point for the MiMo client library.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mimo_internal.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mimo_request
 *
 * Description:
 *   Send a multimodal request to the MiMo API.
 *   Dispatches to the modality-specific body builder, POSTs via TLS,
 *   and parses the response.
 *
 ****************************************************************************/

int mimo_request(const struct mimo_req_s *req, struct mimo_rsp_s *rsp)
{
  const struct mimo_modality_cfg_s *cfg;
  char *body;
  int ret;

  printf("[mimo] mimo_request() entered, modality=%d\n",
         req ? req->modality : -1);
  if (!req || !rsp)
    {
      printf("[mimo] mimo_request: NULL args\n");
      return -1;
    }

  memset(rsp, 0, sizeof(*rsp));

  if (req->modality >= MIMO_MODALITY_MAX)
    {
      snprintf(rsp->error_msg, sizeof(rsp->error_msg),
               "Invalid modality: %d", req->modality);
      rsp->error_code = -1;
      return -1;
    }

  cfg = &g_modality_table[req->modality];

  /* Build the JSON request body */

  body = cfg->build_body(req);
  if (!body)
    {
      printf("[mimo] build_body returned NULL\n");
      snprintf(rsp->error_msg, sizeof(rsp->error_msg),
               "Failed to build request body");
      rsp->error_code = -2;
      return -1;
    }

  printf("[mimo] body built (%zu bytes), calling mimo_http_post...\n",
         strlen(body));

  /* POST to the MiMo API */

  ret = mimo_http_post(body, rsp);
  printf("[mimo] mimo_http_post returned %d\n", ret);
  free(body);

  if (ret < 0)
    {
      /* Error already set by mimo_http_post */
      return ret;
    }

  /* Modality-specific response parsing */

  ret = cfg->parse_resp(NULL, rsp);

  return ret;
}
