/****************************************************************************
 * apps/examples/mimonet/mimonet_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Staged connectivity self-test against the Xiaomi MiMo API.
 * Stages run in order and stop at the first failure so that the serial
 * output points directly at the offending layer.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <stdint.h>
#include <stdbool.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"

#include <arch/board/board_secrets.h>

#include "netutils/cJSON.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* /v1/models response measured ~400 bytes; leave headroom. */

#define MIMONET_RSP_MAX  4096

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct mimonet_ctx_s
{
  char ip[INET_ADDRSTRLEN];   /* Resolved IPv4 address, filled by stage_dns */
  int  sockfd;                /* Connected socket, filled by stage_tcp */

  bool                     tls_inited; /* mbedTLS objects need freeing */
  mbedtls_ssl_context      ssl;
  mbedtls_ssl_config       conf;
  mbedtls_entropy_context  entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bio_send / bio_recv
 *
 * Description:
 *   mbedTLS BIO callbacks bridging to a POSIX socket.  Used instead of
 *   mbedtls_net_* so that the code does not depend on MBEDTLS_NET_C.
 *
 * Input Parameters:
 *   p - Opaque pointer, here a pointer to the socket descriptor.
 *
 * Returned Value:
 *   Number of bytes transferred, or a negative MBEDTLS_ERR_* code.
 *
 ****************************************************************************/

static int bio_send(void *p, const unsigned char *buf, size_t len)
{
  int fd = *(int *)p;
  ssize_t n;

  n = send(fd, buf, len, 0);
  if (n < 0)
    {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
          return MBEDTLS_ERR_SSL_WANT_WRITE;
        }

      return MBEDTLS_ERR_NET_SEND_FAILED;
    }

  return (int)n;
}

static int bio_recv(void *p, unsigned char *buf, size_t len)
{
  int fd = *(int *)p;
  ssize_t n;

  n = recv(fd, buf, len, 0);
  if (n < 0)
    {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
          return MBEDTLS_ERR_SSL_WANT_READ;
        }

      return MBEDTLS_ERR_NET_RECV_FAILED;
    }

  return (int)n;
}

/****************************************************************************
 * Name: tls_strerror
 *
 * Description:
 *   Render an mbedTLS error code into a static buffer for printing.
 *
 ****************************************************************************/

static const char *tls_strerror(int err)
{
  static char buf[128];

  mbedtls_strerror(err, buf, sizeof(buf));
  return buf;
}

/****************************************************************************
 * Name: stage_dns
 *
 * Description:
 *   Resolve BOARD_MIMO_HOST to an IPv4 address and store it in ctx->ip.
 *
 * Returned Value:
 *   0 on success; -1 on failure.
 *
 ****************************************************************************/

static int stage_dns(struct mimonet_ctx_s *ctx)
{
  struct addrinfo hints;
  struct addrinfo *res = NULL;
  struct sockaddr_in *sin;
  int ret;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family   = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  ret = getaddrinfo(BOARD_MIMO_HOST, BOARD_MIMO_PORT, &hints, &res);
  if (ret != 0 || res == NULL)
    {
      printf("[dns ] FAIL  getaddrinfo(%s) ret=%d errno=%d\n",
             BOARD_MIMO_HOST, ret, errno);
      return -1;
    }

  sin = (struct sockaddr_in *)res->ai_addr;
  if (inet_ntop(AF_INET, &sin->sin_addr, ctx->ip, sizeof(ctx->ip)) == NULL)
    {
      printf("[dns ] FAIL  inet_ntop errno=%d\n", errno);
      freeaddrinfo(res);
      return -1;
    }

  freeaddrinfo(res);
  printf("[dns ] PASS  %s -> %s\n", BOARD_MIMO_HOST, ctx->ip);
  return 0;
}

/****************************************************************************
 * Name: stage_tcp
 *
 * Description:
 *   Open a TCP connection to ctx->ip on BOARD_MIMO_PORT and store the
 *   socket in ctx->sockfd.
 *
 * Returned Value:
 *   0 on success; -1 on failure.  On failure the socket is closed and
 *   ctx->sockfd is left at -1.
 *
 ****************************************************************************/

static int stage_tcp(struct mimonet_ctx_s *ctx)
{
  struct sockaddr_in addr;
  int fd;

  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    {
      printf("[tcp ] FAIL  socket() errno=%d\n", errno);
      return -1;
    }

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port   = htons((uint16_t)atoi(BOARD_MIMO_PORT));

  if (inet_pton(AF_INET, ctx->ip, &addr.sin_addr) != 1)
    {
      printf("[tcp ] FAIL  inet_pton(%s) errno=%d\n", ctx->ip, errno);
      close(fd);
      return -1;
    }

  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
      printf("[tcp ] FAIL  connect(%s:%s) errno=%d\n",
             ctx->ip, BOARD_MIMO_PORT, errno);
      close(fd);
      return -1;
    }

  ctx->sockfd = fd;
  printf("[tcp ] PASS  connected to %s:%s fd=%d\n",
         ctx->ip, BOARD_MIMO_PORT, fd);
  return 0;
}

/****************************************************************************
 * Name: stage_tls
 *
 * Description:
 *   Perform a TLS handshake over ctx->sockfd.
 *
 * Returned Value:
 *   0 on success; -1 on failure.  mbedTLS objects are always initialised
 *   before the first possible failure, so tls_cleanup() is safe to call
 *   whenever ctx->tls_inited is true.
 *
 ****************************************************************************/

static int stage_tls(struct mimonet_ctx_s *ctx)
{
  static const char *pers = "mimonet";
  int ret;

  mbedtls_ssl_init(&ctx->ssl);
  mbedtls_ssl_config_init(&ctx->conf);
  mbedtls_entropy_init(&ctx->entropy);
  mbedtls_ctr_drbg_init(&ctx->ctr_drbg);
  ctx->tls_inited = true;

  ret = mbedtls_ctr_drbg_seed(&ctx->ctr_drbg, mbedtls_entropy_func,
                              &ctx->entropy,
                              (const unsigned char *)pers, strlen(pers));
  if (ret != 0)
    {
      printf("[tls ] FAIL  ctr_drbg_seed: -0x%04x %s\n",
             (unsigned)-ret, tls_strerror(ret));
      return -1;
    }

  ret = mbedtls_ssl_config_defaults(&ctx->conf, MBEDTLS_SSL_IS_CLIENT,
                                    MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT);
  if (ret != 0)
    {
      printf("[tls ] FAIL  config_defaults: -0x%04x %s\n",
             (unsigned)-ret, tls_strerror(ret));
      return -1;
    }

  /* TODO(M2): replace with MBEDTLS_SSL_VERIFY_REQUIRED plus a CA chain.
   * Leaving verification off makes this connection vulnerable to MITM.
   */

  mbedtls_ssl_conf_authmode(&ctx->conf, MBEDTLS_SSL_VERIFY_NONE);
  mbedtls_ssl_conf_rng(&ctx->conf, mbedtls_ctr_drbg_random, &ctx->ctr_drbg);

  ret = mbedtls_ssl_setup(&ctx->ssl, &ctx->conf);
  if (ret != 0)
    {
      printf("[tls ] FAIL  ssl_setup: -0x%04x %s\n",
             (unsigned)-ret, tls_strerror(ret));
      return -1;
    }

  ret = mbedtls_ssl_set_hostname(&ctx->ssl, BOARD_MIMO_HOST);
  if (ret != 0)
    {
      printf("[tls ] FAIL  set_hostname: -0x%04x %s\n",
             (unsigned)-ret, tls_strerror(ret));
      return -1;
    }

  mbedtls_ssl_set_bio(&ctx->ssl, &ctx->sockfd, bio_send, bio_recv, NULL);

  do
    {
      ret = mbedtls_ssl_handshake(&ctx->ssl);
    }
  while (ret == MBEDTLS_ERR_SSL_WANT_READ ||
         ret == MBEDTLS_ERR_SSL_WANT_WRITE);

  if (ret != 0)
    {
      printf("[tls ] FAIL  handshake: -0x%04x %s\n",
             (unsigned)-ret, tls_strerror(ret));
      return -1;
    }

  printf("[tls ] PASS  %s / %s\n",
         mbedtls_ssl_get_version(&ctx->ssl),
         mbedtls_ssl_get_ciphersuite(&ctx->ssl));
  printf("[tls ] WARN  certificate verification is DISABLED (M0 only)\n");
  return 0;
}

/****************************************************************************
 * Name: tls_cleanup
 ****************************************************************************/

static void tls_cleanup(struct mimonet_ctx_s *ctx)
{
  if (!ctx->tls_inited)
    {
      return;
    }

  mbedtls_ssl_close_notify(&ctx->ssl);
  mbedtls_ssl_free(&ctx->ssl);
  mbedtls_ssl_config_free(&ctx->conf);
  mbedtls_ctr_drbg_free(&ctx->ctr_drbg);
  mbedtls_entropy_free(&ctx->entropy);
  ctx->tls_inited = false;
}

/****************************************************************************
 * Name: stage_http
 *
 * Description:
 *   Send GET /v1/models over the established TLS session, then parse the
 *   JSON body and print every model id.
 *
 * Returned Value:
 *   0 on success; -1 on failure.
 *
 ****************************************************************************/

static int stage_http(struct mimonet_ctx_s *ctx)
{
  char req[512];
  char *rsp;
  const char *body;
  cJSON *root;
  cJSON *data;
  cJSON *item;
  int status = 0;
  int count = 0;
  size_t total = 0;
  int len;
  int ret;

  rsp = calloc(1, MIMONET_RSP_MAX);
  if (rsp == NULL)
    {
      printf("[http] FAIL  calloc(%d) failed\n", MIMONET_RSP_MAX);
      return -1;
    }

  len = snprintf(req, sizeof(req),
                 "GET /v1/models HTTP/1.1\r\n"
                 "Host: %s\r\n"
                 "Authorization: Bearer %s\r\n"
                 "Connection: close\r\n"
                 "\r\n",
                 BOARD_MIMO_HOST, BOARD_MIMO_API_KEY);
  if (len <= 0 || (size_t)len >= sizeof(req))
    {
      printf("[http] FAIL  request does not fit in %zu bytes\n", sizeof(req));
      free(rsp);
      return -1;
    }

  ret = mbedtls_ssl_write(&ctx->ssl, (const unsigned char *)req, (size_t)len);
  if (ret < 0)
    {
      printf("[http] FAIL  ssl_write: -0x%04x %s\n",
             (unsigned)-ret, tls_strerror(ret));
      free(rsp);
      return -1;
    }

  /* Read until peer closes. Request carries Connection: close, so EOF is the full response. */

  for (; ; )
    {
      ret = mbedtls_ssl_read(&ctx->ssl,
                             (unsigned char *)rsp + total,
                             MIMONET_RSP_MAX - 1 - total);

      if (ret == MBEDTLS_ERR_SSL_WANT_READ ||
          ret == MBEDTLS_ERR_SSL_WANT_WRITE)
        {
          continue;
        }

      if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || ret == 0)
        {
          break;
        }

      if (ret < 0)
        {
          printf("[http] FAIL  ssl_read: -0x%04x %s\n",
                 (unsigned)-ret, tls_strerror(ret));
          free(rsp);
          return -1;
        }

      total += (size_t)ret;
      if (total >= MIMONET_RSP_MAX - 1)
        {
          printf("[http] FAIL  response exceeds %d bytes\n", MIMONET_RSP_MAX);
          free(rsp);
          return -1;
        }
    }

  rsp[total] = '\0';

  if (sscanf(rsp, "HTTP/1.%*d %d", &status) != 1)
    {
      printf("[http] FAIL  cannot parse status line\n");
      printf("[http]       first 128 bytes: %.128s\n", rsp);
      free(rsp);
      return -1;
    }

  if (status != 200)
    {
      printf("[http] FAIL  HTTP %d\n", status);
      printf("[http]       body: %.256s\n", rsp);
      free(rsp);
      return -1;
    }

  /* Blank line separates header from body. */

  body = strstr(rsp, "\r\n\r\n");
  if (body == NULL)
    {
      printf("[http] FAIL  no header/body separator found\n");
      free(rsp);
      return -1;
    }

  body += 4;

  root = cJSON_Parse(body);
  if (root == NULL)
    {
      printf("[http] FAIL  cJSON_Parse failed\n");
      printf("[http]       body: %.256s\n", body);
      free(rsp);
      return -1;
    }

  data = cJSON_GetObjectItem(root, "data");
  if (!cJSON_IsArray(data))
    {
      printf("[http] FAIL  \"data\" is not an array\n");
      cJSON_Delete(root);
      free(rsp);
      return -1;
    }

  printf("[http] PASS  HTTP 200, %d bytes\n", (int)total);

  cJSON_ArrayForEach(item, data)
    {
      cJSON *id = cJSON_GetObjectItem(item, "id");
      if (cJSON_IsString(id))
        {
          printf("[http]       model: %s\n", id->valuestring);
          count++;
        }
    }

  cJSON_Delete(root);
  free(rsp);

  if (count == 0)
    {
      printf("[http] FAIL  no model ids in response\n");
      return -1;
    }

  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  struct mimonet_ctx_s ctx;
  int ret = EXIT_FAILURE;

  memset(&ctx, 0, sizeof(ctx));
  ctx.sockfd = -1;

  printf("mimonet: target %s:%s\n", BOARD_MIMO_HOST, BOARD_MIMO_PORT);

  if (stage_dns(&ctx) < 0)
    {
      goto cleanup;
    }

  if (stage_tcp(&ctx) < 0)
    {
      goto cleanup;
    }

  if (stage_tls(&ctx) < 0)
    {
      goto cleanup;
    }

  if (stage_http(&ctx) < 0)
    {
      goto cleanup;
    }

  printf("mimonet: all stages passed\n");
  ret = EXIT_SUCCESS;

cleanup:
  tls_cleanup(&ctx);

  if (ctx.sockfd >= 0)
    {
      close(ctx.sockfd);
    }

  return ret;
}