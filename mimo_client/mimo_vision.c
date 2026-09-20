/****************************************************************************
 * apps/mimo_client/mimo_vision.c
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
#include <netutils/cJSON.h>
#include "mimo_internal.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const char g_base64_table[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mimo_base64_encode
 *
 * Description:
 *   Encode binary data to base64. The caller must provide a buffer
 *   large enough: ((slen + 2) / 3) * 4 + 1.
 *   Returns the length of the encoded string (excluding null terminator).
 *
 ****************************************************************************/

int mimo_base64_encode(const uint8_t *src, size_t slen, char *dst,
                       size_t dlen)
{
  size_t i;
  int out = 0;

  for (i = 0; i < slen; i += 3)
    {
      uint32_t val = (uint32_t)src[i] << 16;
      if (i + 1 < slen) val |= (uint32_t)src[i + 1] << 8;
      if (i + 2 < slen) val |= (uint32_t)src[i + 2];

      dst[out++] = g_base64_table[(val >> 18) & 0x3f];
      dst[out++] = g_base64_table[(val >> 12) & 0x3f];
      dst[out++] = (i + 1 < slen) ?
        g_base64_table[(val >> 6) & 0x3f] : '=';
      dst[out++] = (i + 2 < slen) ?
        g_base64_table[val & 0x3f] : '=';

      if ((size_t)out >= dlen - 1) break;
    }

  dst[out] = '\0';
  return out;
}

/****************************************************************************
 * Name: mimo_vision_build_body
 *
 * Description:
 *   Build the JSON request body for vision modality.
 *   Encodes JPEG data as base64 and wraps in OpenAI vision format:
 *   {
 *     "model": "mimo-v2.5",
 *     "messages": [{
 *       "role": "user",
 *       "content": [
 *         {"type": "text", "text": "<prompt>"},
 *         {"type": "image_url", "image_url": {
 *           "url": "data:image/jpeg;base64,<b64>"
 *         }}
 *       ]
 *     }]
 *   }
 *
 ****************************************************************************/

char *mimo_vision_build_body(const struct mimo_req_s *req)
{
  cJSON *root;
  cJSON *messages;
  cJSON *msg;
  cJSON *content_array;
  cJSON *text_part;
  cJSON *image_part;
  cJSON *image_url;
  char *b64;
  size_t b64_max;
  char data_url[128];
  char *body;
  int b64_len;

  /* Base64 encode the JPEG data */

  b64_max = ((req->jpeg_len + 2) / 3) * 4 + 1;
  b64 = malloc(b64_max + 256);  /* +256 for "data:image/jpeg;base64," */
  if (!b64) return NULL;

  b64_len = mimo_base64_encode(req->jpeg_data, req->jpeg_len,
                                b64, b64_max);

  /* Build the data URL: "data:image/jpeg;base64,<base64>" */

  snprintf(data_url, sizeof(data_url), "data:image/jpeg;base64,");
  memmove(b64 + strlen(data_url), b64, b64_len + 1);
  memcpy(b64, data_url, strlen(data_url));
  b64_len += strlen(data_url);

  /* Build JSON */

  root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "model", "mimo-v2.5");
  cJSON_AddNumberToObject(root, "top_p", 0.95);
  cJSON_AddNumberToObject(root, "max_tokens", 4096);

  messages = cJSON_AddArrayToObject(root, "messages");
  msg = cJSON_CreateObject();
  cJSON_AddStringToObject(msg, "role", "user");

  content_array = cJSON_AddArrayToObject(msg, "content");

  /* Text part */

  text_part = cJSON_CreateObject();
  cJSON_AddStringToObject(text_part, "type", "text");
  cJSON_AddStringToObject(text_part, "text",
                          req->prompt ? req->prompt :
                          "Describe this image in detail.");
  cJSON_AddItemToArray(content_array, text_part);

  /* Image part */

  image_part = cJSON_CreateObject();
  cJSON_AddStringToObject(image_part, "type", "image_url");
  image_url = cJSON_CreateObject();
  cJSON_AddStringToObject(image_url, "url", b64);
  cJSON_AddItemToObject(image_part, "image_url", image_url);
  cJSON_AddItemToArray(content_array, image_part);

  cJSON_AddItemToArray(messages, msg);

  body = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  free(b64);

  return body;
}

/****************************************************************************
 * Name: mimo_vision_parse_resp
 *
 * Description:
 *   Vision modality uses the same response format as text.
 *   No extra parsing needed.
 *
 ****************************************************************************/

int mimo_vision_parse_resp(const char *json_str, struct mimo_rsp_s *rsp)
{
  (void)json_str;
  (void)rsp;
  return 0;
}
