/****************************************************************************
 * apps/feishu_notify/feishu_http.c
 *
 * HTTPS/TLS client for the Feishu (Lark) OpenAPI.
 * Uses raw sockets + mbedTLS (no webclient dependency), mirroring the TLS
 * pattern already proven in apps/mimo_client/mimo_http.c.
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#ifdef CONFIG_CRYPTO_MBEDTLS
#  include <mbedtls/ssl.h>
#  include <mbedtls/entropy.h>
#  include <mbedtls/ctr_drbg.h>
#endif

#include "feishu_internal.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define HTTP_RECV_BUF_SIZE  2048
#define HTTP_MAX_RESPONSE   (16 * 1024)

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

int feishu_tls_init(void)
{
  int ret;

  if (g_tls_ready) return 0;

  mbedtls_entropy_init(&g_tls_ctx.entropy);
  mbedtls_ctr_drbg_init(&g_tls_ctx.ctr_drbg);
  mbedtls_ssl_config_init(&g_tls_ctx.conf);

  ret = mbedtls_ctr_drbg_seed(&g_tls_ctx.ctr_drbg,
                              mbedtls_entropy_func,
                              &g_tls_ctx.entropy,
                              (const unsigned char *)"feishu_notify", 14);
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

void feishu_tls_deinit(void)
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

  he = gethostbyname(hostname);
  if (!he)
    {
      printf("[feishu_notify] DNS resolve failed for %s (h_errno=%d)\n",
             hostname, h_errno);
      return -EIO;
    }

  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    {
      printf("[feishu_notify] socket() failed: %d\n", errno);
      return -EIO;
    }

  portnum = atoi(port);
  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons(portnum);
  memcpy(&server.sin_addr, he->h_addr_list[0], he->h_length);

  if (connect(fd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
      printf("[feishu_notify] TCP connect failed: %d\n", errno);
      close(fd);
      return -EIO;
    }

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
          printf("[feishu_notify] TLS handshake failed: -0x%x\n", -ret);
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
 * Private Functions - HTTP helpers
 ****************************************************************************/

/* Extract HTTP status code from the response header ("HTTP/1.x NNN ..."). */

static int extract_http_status(const char *response)
{
  const char *p = response;

  if (strncmp(p, "HTTP/", 5) != 0) return 0;

  p = strchr(p, ' ');
  if (!p) return 0;

  return atoi(p + 1);
}

/* Find the body start in an HTTP response (after "\r\n\r\n"). */

static const char *find_body(const char *response)
{
  const char *p = strstr(response, "\r\n\r\n");
  if (!p) return response;  /* No header separator, treat all as body */
  return p + 4;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int feishu_notify_https_post(const char *path, const char *bearer,
                             const char *json_body, char *resp_buf,
                             size_t resp_cap)
{
#ifdef CONFIG_CRYPTO_MBEDTLS
  struct tls_conn_s *conn = NULL;
  char *request;
  size_t req_size;
  size_t body_len = json_body ? strlen(json_body) : 0;
  char recv_buf[HTTP_RECV_BUF_SIZE];
  char *response = NULL;
  size_t resp_len = 0;
  size_t resp_alloc = 0;
  const char *body_start;
  int http_status;
  int ret;
  int n;
  int hlen;

  if (feishu_tls_init() != 0)
    {
      return -1;
    }

  /* Host + path + headers + body. The Authorization header is variable. */

  req_size = 512 + strlen(FEISHU_OPENAPI_HOST) + strlen(path)
             + (bearer ? strlen(bearer) : 0) + body_len + 1;
  request = malloc(req_size);
  if (!request)
    {
      return -1;
    }

  ret = tls_connect_socket(FEISHU_OPENAPI_HOST, FEISHU_OPENAPI_PORT, &conn);
  if (ret < 0)
    {
      free(request);
      return -1;
    }

  hlen = snprintf(request, req_size,
                  "POST %s HTTP/1.1\r\n"
                  "Host: %s\r\n"
                  "Content-Type: application/json\r\n",
                  path, FEISHU_OPENAPI_HOST);

  if (bearer && bearer[0] != '\0')
    {
      hlen += snprintf(request + hlen, req_size - (size_t)hlen,
                       "Authorization: Bearer %s\r\n", bearer);
    }

  hlen += snprintf(request + hlen, req_size - (size_t)hlen,
                   "Content-Length: %zu\r\n"
                   "Connection: close\r\n"
                   "\r\n",
                   body_len);

  if (body_len > 0 && json_body)
    {
      snprintf(request + hlen, req_size - (size_t)hlen, "%s", json_body);
    }

  ret = tls_write(conn, request, strlen(request));
  free(request);

  if (ret < 0)
    {
      tls_disconnect(conn);
      return -1;
    }

  /* Read the full response. */

  while ((n = tls_read(conn, recv_buf, sizeof(recv_buf) - 1)) > 0)
    {
      if (resp_len + n + 1 > resp_alloc)
        {
          size_t newcap = resp_alloc ? resp_alloc * 2 : HTTP_RECV_BUF_SIZE;
          if (newcap > HTTP_MAX_RESPONSE)
            {
              break;  /* Too large */
            }

          char *newbuf = realloc(response, newcap);
          if (!newbuf) break;
          response = newbuf;
          resp_alloc = newcap;
        }

      memcpy(response + resp_len, recv_buf, n);
      resp_len += n;
      response[resp_len] = '\0';
    }

  tls_disconnect(conn);

  if (!response)
    {
      if (resp_buf && resp_cap > 0)
        {
          resp_buf[0] = '\0';
        }

      return -1;
    }

  http_status = extract_http_status(response);

  /* Copy the response body into the caller's buffer. */

  body_start = find_body(response);
  if (resp_buf && resp_cap > 0)
    {
      size_t blen = strlen(body_start);
      if (blen >= resp_cap) blen = resp_cap - 1;
      memcpy(resp_buf, body_start, blen);
      resp_buf[blen] = '\0';
    }

  free(response);
  return http_status;
#else
  (void)path;
  (void)bearer;
  (void)json_body;
  (void)resp_buf;
  (void)resp_cap;
  fprintf(stderr, "[feishu_notify] TLS not available\n");
  return -1;
#endif
}
