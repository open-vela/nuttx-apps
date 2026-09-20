/****************************************************************************
 * apps/mimo_client/mimo_text.c
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "mimo_internal.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/**
 * Build JSON request body for MiMo text chat completions.
 * Uses direct snprintf to avoid cJSON double operations (no FPU on rv32imac).
 */

char *mimo_text_build_body(const struct mimo_req_s *req)
{
  const char *prompt = req->prompt ? req->prompt : "Hello";
  size_t prompt_len = strlen(prompt);
  size_t buf_size;
  char *body;
  char *escaped;
  size_t i;
  size_t j;
  int ret;

  printf("[mimo_text] build_body start, prompt_len=%zu\n", prompt_len);

  /* Escape JSON special chars in prompt: " \ and control chars */

  escaped = malloc(prompt_len * 2 + 2);
  if (!escaped) { printf("[mimo_text] escaped malloc failed\n"); return NULL; }

  j = 0;
  for (i = 0; i < prompt_len; i++)
    {
      char c = prompt[i];
      if (c == '"')  { escaped[j++] = '\\'; escaped[j++] = '"'; }
      else if (c == '\\') { escaped[j++] = '\\'; escaped[j++] = '\\'; }
      else if (c == '\n') { escaped[j++] = '\\'; escaped[j++] = 'n'; }
      else if (c == '\r') { escaped[j++] = '\\'; escaped[j++] = 'r'; }
      else if (c == '\t') { escaped[j++] = '\\'; escaped[j++] = 't'; }
      else { escaped[j++] = c; }
    }
  escaped[j] = '\0';

  /* Fixed JSON skeleton + escaped prompt.
   * 512 bytes for JSON structure + 2x prompt for safety */

  buf_size = 512 + j + 1;
  body = malloc(buf_size);
  if (!body) { printf("[mimo_text] body malloc(%zu) failed\n", buf_size); free(escaped); return NULL; }

  ret = snprintf(body, buf_size,
    "{"
    "\"model\":\"mimo-v2.5\","
    "\"top_p\":0.95,"
    "\"max_tokens\":4096,"
    "\"messages\":[{"
      "\"role\":\"user\","
      "\"content\":[{"
        "\"type\":\"text\","
        "\"text\":\"%s\""
      "}]"
    "}]"
    "}", escaped);

  free(escaped);

  if (ret < 0 || (size_t)ret >= buf_size)
    {
      printf("[mimo_text] snprintf truncated: ret=%d, buf=%zu\n", ret, buf_size);
      free(body);
      return NULL;
    }

  printf("[mimo_text] body built (%d bytes)\n", ret);
  return body;
}

int mimo_text_parse_resp(const char *json_str, struct mimo_rsp_s *rsp)
{
  /* Generic parsing in mimo_http_post already extracts content.
   * Text modality has no extra fields to parse.
   */

  (void)json_str;
  (void)rsp;
  return 0;
}
