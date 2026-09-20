/****************************************************************************
 * apps/mimo_client/include/mimo_client.h
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

#ifndef __APPS_MIMO_CLIENT_INCLUDE_MIMO_CLIENT_H
#define __APPS_MIMO_CLIENT_INCLUDE_MIMO_CLIENT_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>
#include <stddef.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration:
 * - Host and port from Kconfig (non-secret defaults)
 * - API key from board_secrets.h (gitignored)
 */

#ifdef CONFIG_MIMO_CLIENT_USE_BOARD_SECRETS
#  include <arch/board/board_secrets.h>
#  define MIMO_API_KEY   BOARD_MIMO_API_KEY
#  define MIMO_HOST      BOARD_MIMO_HOST
#  define MIMO_PORT      BOARD_MIMO_PORT
#else
#  define MIMO_API_KEY   CONFIG_MIMO_CLIENT_API_KEY
#  define MIMO_HOST      CONFIG_MIMO_CLIENT_HOST
#  define MIMO_PORT      CONFIG_MIMO_CLIENT_PORT
#endif

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* Supported modalities */

enum mimo_modality_e
{
  MIMO_TEXT = 0,
  MIMO_VISION,
  MIMO_TTS,
  MIMO_ASR,
  MIMO_MODALITY_MAX
};

/* Request descriptor */

struct mimo_req_s
{
  enum mimo_modality_e modality;
  const uint8_t *jpeg_data;     /* JPEG buffer (MIMO_VISION only) */
  size_t          jpeg_len;
  const char     *prompt;       /* Text prompt or vision prompt */
  const char     *tts_text;     /* Text to speak (MIMO_TTS only) */
};

/* Response descriptor */

struct mimo_rsp_s
{
  char    *content;             /* Text/vision/ASR response text */
  uint8_t *audio_data;          /* Raw PCM (TTS only) */
  size_t   audio_len;
  int      http_status;
  int      error_code;
  char     error_msg[256];
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/* Initialize the MiMo client (TLS context, etc.). Call once at startup. */

int mimo_client_init(void);

/* Send a multimodal request and populate the response.
 * Caller must free the response with mimo_free_response().
 */

int mimo_request(const struct mimo_req_s *req, struct mimo_rsp_s *rsp);

/* Free all heap-allocated fields in a response. */

void mimo_free_response(struct mimo_rsp_s *rsp);

/* Deinitialize the MiMo client. */

void mimo_client_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __APPS_MIMO_CLIENT_INCLUDE_MIMO_CLIENT_H */
