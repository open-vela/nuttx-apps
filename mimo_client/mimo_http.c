/****************************************************************************
 * apps/mimo_client/mimo_http.c
 *
 * HTTP/TLS client for MiMo API communication.
 * Uses raw sockets + mbedTLS (no webclient dependency).
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

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <netutils/cJSON.h>

#ifdef CONFIG_CRYPTO_MBEDTLS
#  include <mbedtls/ssl.h>
#  include <mbedtls/entropy.h>
#  include <mbedtls/ctr_drbg.h>
#endif

#include "include/mimo_client.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define HTTP_RECV_BUF_SIZE  2048
#define HTTP_MAX_RESPONSE   (64 * 1024)

/****************************************************************************
 * Private Types
 ****************************************************************************/

#ifdef CONFIG_CRYPTO_MBEDTLS

struct tls_ctx_s
{
  mbedtls_entropy_context  entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_ssl_config       conf;
};

struct tls_conn_s
{
  mbedtls_ssl_context  ssl;
  int                  fd;
};

#endif /* CONFIG_CRYPTO_MBEDTLS */

/****************************************************************************
 * Private Data
 ****************************************************************************/

#ifdef CONFIG_CRYPTO_MBEDTLS
static struct tls_ctx_s g_tls_ctx;
static bool g_tls_ready = false;
#endif

/****************************************************************************
 * Private Functions - TLS
 ****************************************************************************/

#ifdef CONFIG_CRYPTO_MBEDTLS

static int bio_send(void *ctx, const unsigned char *buf, size_t len)
{
  int fd = *(int *)ctx;
  ssize_t ret = send(fd, buf, len, 0);
  return (ret < 0) ? -EIO : (int)ret;
}

static int bio_recv(void *ctx, unsigned char *buf, size_t len)
{
  int fd = *(int *)ctx;
  ssize_t ret = recv(fd, buf, len, 0);
  if (ret < 0) return -EIO;
  if (ret == 0) return MBEDTLS_ERR_SSL_WANT_READ;
  return (int)ret;
}

static int tls_init(void)
{
  int ret;

  if (g_tls_ready) return 0;

  mbedtls_entropy_init(&g_tls_ctx.entropy);
  mbedtls_ctr_drbg_init(&g_tls_ctx.ctr_drbg);
  mbedtls_ssl_config_init(&g_tls_ctx.conf);

  ret = mbedtls_ctr_drbg_seed(&g_tls_ctx.ctr_drbg,
                               mbedtls_entropy_func,
                               &g_tls_ctx.entropy,
                               (const unsigned char *)"mimo_client", 11);
  if (ret != 0) return -1;

  ret = mbedtls_ssl_config_defaults(&g_tls_ctx.conf,
                                     MBEDTLS_SSL_IS_CLIENT,
                                     MBEDTLS_SSL_TRANSPORT_STREAM,
                                     MBEDTLS_SSL_PRESET_DEFAULT);
  if (ret != 0) return -1;

  mbedtls_ssl_conf_authmode(&g_tls_ctx.conf, MBEDTLS_SSL_VERIFY_NONE);
  mbedtls_ssl_conf_rng(&g_tls_ctx.conf, mbedtls_ctr_drbg_random,
                        &g_tls_ctx.ctr_drbg);

  g_tls_ready = true;
  return 0;
}

static void tls_deinit(void)
{
  if (!g_tls_ready) return;
  mbedtls_ssl_config_free(&g_tls_ctx.conf);
  mbedtls_ctr_drbg_free(&g_tls_ctx.ctr_drbg);
  mbedtls_entropy_free(&g_tls_ctx.entropy);
  g_tls_ready = false;
}

static int tls_connect_socket(const char *hostname, const char *port,
                               struct tls_conn_s **connp)
{
  struct tls_conn_s *conn;
  struct hostent *he;
  struct sockaddr_in server;
  int portnum;
  int ret;
  int fd;

  printf("[mimo_client] DNS lookup %s:%s ...\n", hostname, port);
  he = gethostbyname(hostname);
  if (!he)
    {
      printf("[mimo_client] DNS resolve failed for %s (h_errno=%d)\n",
             hostname, h_errno);
      return -EIO;
    }
  printf("[mimo_client] DNS resolved OK\n");

  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    {
      printf("[mimo_client] socket() failed: %d\n", errno);
      return -EIO;
    }

  portnum = atoi(port);
  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons(portnum);
  memcpy(&server.sin_addr, he->h_addr_list[0], he->h_length);

  printf("[mimo_client] TCP connect to %s:%d ...\n",
         inet_ntoa(server.sin_addr), portnum);
  if (connect(fd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
      printf("[mimo_client] TCP connect failed: %d\n", errno);
      close(fd);
      return -EIO;
    }

  printf("[mimo_client] TCP connected, TLS handshake ...\n");
  conn = calloc(1, sizeof(*conn));
  if (!conn)
    {
      close(fd);
      return -ENOMEM;
    }

  conn->fd = fd;
  mbedtls_ssl_init(&conn->ssl);

  ret = mbedtls_ssl_setup(&conn->ssl, &g_tls_ctx.conf);
  if (ret != 0) goto err;

  ret = mbedtls_ssl_set_hostname(&conn->ssl, hostname);
  if (ret != 0) goto err;

  mbedtls_ssl_set_bio(&conn->ssl, &conn->fd, bio_send, bio_recv, NULL);

  while ((ret = mbedtls_ssl_handshake(&conn->ssl)) != 0)
    {
      if (ret != MBEDTLS_ERR_SSL_WANT_READ &&
          ret != MBEDTLS_ERR_SSL_WANT_WRITE)
        {
          printf("[mimo_client] TLS handshake failed: -0x%x\n", -ret);
          goto err;
        }
    }

  *connp = conn;
  return 0;

err:
  mbedtls_ssl_free(&conn->ssl);
  close(conn->fd);
  free(conn);
  return -EIO;
}

static void tls_disconnect(struct tls_conn_s *conn)
{
  if (!conn) return;
  mbedtls_ssl_close_notify(&conn->ssl);
  mbedtls_ssl_free(&conn->ssl);
  close(conn->fd);
  free(conn);
}

static int tls_write(struct tls_conn_s *conn, const void *buf, size_t len)
{
  int ret = mbedtls_ssl_write(&conn->ssl, buf, len);
  if (ret < 0)
    {
      return (ret == MBEDTLS_ERR_SSL_WANT_WRITE) ? 0 : -EIO;
    }

  return ret;
}

static int tls_read(struct tls_conn_s *conn, void *buf, size_t len)
{
  int ret = mbedtls_ssl_read(&conn->ssl, buf, len);
  if (ret < 0)
    {
      if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY ||
          ret == MBEDTLS_ERR_SSL_WANT_READ)
        {
          return 0;
        }

      return -EIO;
    }

  return ret;
}

#endif /* CONFIG_CRYPTO_MBEDTLS */

/****************************************************************************
 * Private Functions - HTTP Response Parser
 ****************************************************************************/

static int parse_chat_response(const char *json_str,
                               struct mimo_rsp_s *rsp)
{
  cJSON *root;
  cJSON *choices;
  cJSON *choice;
  cJSON *message;
  cJSON *content;
  cJSON *audio;
  cJSON *audio_data_item;

  root = cJSON_Parse(json_str);
  if (!root)
    {
      snprintf(rsp->error_msg, sizeof(rsp->error_msg),
               "JSON parse error");
      rsp->error_code = -1;
      return -1;
    }

  cJSON *error = cJSON_GetObjectItem(root, "error");
  if (error)
    {
      cJSON *errmsg = cJSON_GetObjectItem(error, "message");
      snprintf(rsp->error_msg, sizeof(rsp->error_msg), "API: %s",
               errmsg ? errmsg->valuestring : "unknown");
      rsp->error_code = -2;
      cJSON_Delete(root);
      return -1;
    }

  choices = cJSON_GetObjectItem(root, "choices");
  if (!choices || cJSON_GetArraySize(choices) == 0)
    {
      snprintf(rsp->error_msg, sizeof(rsp->error_msg),
               "No choices in response");
      rsp->error_code = -3;
      cJSON_Delete(root);
      return -1;
    }

  choice = cJSON_GetArrayItem(choices, 0);
  message = cJSON_GetObjectItem(choice, "message");
  if (!message)
    {
      snprintf(rsp->error_msg, sizeof(rsp->error_msg),
               "No message in choice");
      rsp->error_code = -4;
      cJSON_Delete(root);
      return -1;
    }

  content = cJSON_GetObjectItem(message, "content");
  if (content && content->valuestring)
    {
      rsp->content = strdup(content->valuestring);
    }

  audio = cJSON_GetObjectItem(message, "audio");
  if (audio)
    {
      audio_data_item = cJSON_GetObjectItem(audio, "data");
      if (audio_data_item && audio_data_item->valuestring)
        {
          rsp->audio_data = (uint8_t *)strdup(audio_data_item->valuestring);
          rsp->audio_len = strlen((char *)rsp->audio_data);
        }
    }

  cJSON_Delete(root);
  return 0;
}

/****************************************************************************
 * Private Functions - HTTP POST
 ****************************************************************************/

/**
 * Extract HTTP status code from response header.
 * Returns 0 if not found.
 */

static int extract_http_status(const char *response)
{
  const char *p = response;

  /* Expect "HTTP/1.x NNN" */

  if (strncmp(p, "HTTP/", 5) != 0) return 0;

  p = strchr(p, ' ');
  if (!p) return 0;

  return atoi(p + 1);
}

/**
 * Find the body start in an HTTP response (after \r\n\r\n).
 */

static const char *find_body(const char *response)
{
  const char *p = strstr(response, "\r\n\r\n");
  if (!p) return response;  /* No header separator, treat all as body */
  return p + 4;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int mimo_client_init(void)
{
#ifdef CONFIG_CRYPTO_MBEDTLS
  return tls_init();
#else
  fprintf(stderr, "[mimo_client] TLS not available\n");
  return -1;
#endif
}

void mimo_client_deinit(void)
{
#ifdef CONFIG_CRYPTO_MBEDTLS
  tls_deinit();
#endif
}

/****************************************************************************
 * Name: mimo_http_post
 *
 * Description:
 *   POST a JSON body to the MiMo API endpoint via raw socket + TLS.
 *   Returns 0 on success, <0 on error. rsp is populated with parsed result.
 *
 ****************************************************************************/

int mimo_http_post(const char *json_body, struct mimo_rsp_s *rsp)
{
#ifdef CONFIG_CRYPTO_MBEDTLS
  struct tls_conn_s *conn = NULL;
  char *request;
  size_t req_size;
  char *response = NULL;
  size_t resp_len = 0;
  size_t resp_cap = 0;
  char recv_buf[HTTP_RECV_BUF_SIZE];
  const char *body_start;
  int http_status;
  int ret;
  int n;

  printf("[mimo] mimo_http_post() entered\n");
  memset(rsp, 0, sizeof(*rsp));

  /* Allocate request buffer on heap (avoid stack overflow).
   * Headers ~200 bytes + body + API key + hostname + null */

  req_size = 256 + strlen(MIMO_HOST) + strlen(MIMO_API_KEY)
             + strlen(json_body) + 1;
  request = malloc(req_size);
  if (!request)
    {
      snprintf(rsp->error_msg, sizeof(rsp->error_msg),
               "OOM: request buffer");
      rsp->error_code = -1;
      return -1;
    }

  /* Connect via TLS */

  ret = tls_connect_socket(MIMO_HOST, MIMO_PORT, &conn);
  if (ret < 0)
    {
      snprintf(rsp->error_msg, sizeof(rsp->error_msg),
               "TLS connect failed");
      rsp->error_code = ret;
      free(request);
      return -1;
    }

  /* Build HTTP POST request */

  snprintf(request, req_size,
           "POST /v1/chat/completions HTTP/1.1\r\n"
           "Host: %s\r\n"
           "Authorization: Bearer %s\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: %zu\r\n"
           "Connection: close\r\n"
           "\r\n"
           "%s",
           MIMO_HOST, MIMO_API_KEY, strlen(json_body), json_body);

  /* Send request */

  ret = tls_write(conn, request, strlen(request));
  free(request);

  if (ret < 0)
    {
      snprintf(rsp->error_msg, sizeof(rsp->error_msg),
               "TLS send failed");
      rsp->error_code = ret;
      tls_disconnect(conn);
      return -1;
    }

  /* Read response */

  while ((n = tls_read(conn, recv_buf, sizeof(recv_buf) - 1)) > 0)
    {
      if (resp_len + n + 1 > resp_cap)
        {
          size_t newcap = resp_cap ? resp_cap * 2 : HTTP_RECV_BUF_SIZE;
          if (newcap > HTTP_MAX_RESPONSE)
            {
              break;  /* Too large */
            }

          char *newbuf = realloc(response, newcap);
          if (!newbuf) break;
          response = newbuf;
          resp_cap = newcap;
        }

      memcpy(response + resp_len, recv_buf, n);
      resp_len += n;
      response[resp_len] = '\0';
    }

  tls_disconnect(conn);

  if (!response)
    {
      snprintf(rsp->error_msg, sizeof(rsp->error_msg),
               "Empty response");
      rsp->error_code = -5;
      return -1;
    }

  /* Parse HTTP status */

  http_status = extract_http_status(response);
  rsp->http_status = http_status;

  if (http_status != 200)
    {
      body_start = find_body(response);
      snprintf(rsp->error_msg, sizeof(rsp->error_msg),
               "HTTP %d: %.200s", http_status,
               body_start ? body_start : "(empty)");
      rsp->error_code = http_status;
      free(response);
      return -1;
    }

  /* Find and parse JSON body */

  body_start = find_body(response);
  if (!body_start || strlen(body_start) == 0)
    {
      snprintf(rsp->error_msg, sizeof(rsp->error_msg),
               "Empty response body");
      rsp->error_code = -6;
      free(response);
      return -1;
    }

  ret = parse_chat_response(body_start, rsp);
  free(response);
  return ret;
#else
  (void)json_body;
  snprintf(rsp->error_msg, sizeof(rsp->error_msg),
           "TLS not available");
  rsp->error_code = -100;
  return -1;
#endif
}

void mimo_free_response(struct mimo_rsp_s *rsp)
{
  if (rsp->content)
    {
      free(rsp->content);
      rsp->content = NULL;
    }

  if (rsp->audio_data)
    {
      free(rsp->audio_data);
      rsp->audio_data = NULL;
      rsp->audio_len = 0;
    }
}
