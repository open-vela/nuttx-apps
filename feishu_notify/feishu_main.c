/****************************************************************************
 * apps/feishu_notify/feishu_main.c
 *
 * NSH "feishu" command: push a text message to a Feishu group chat.
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
#include <string.h>

#include <feishu_notify.h>

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void print_usage(void)
{
  printf("Usage:\n");
  printf("  feishu send <chat_id> <text...>  -- send text to a Feishu chat\n");
  printf("  feishu dm <open_id> <text...>     -- send a direct message to a user\n");
  printf("  feishu test [chat_id]             -- send a test message\n");
  printf("  feishu token                      -- fetch & print tenant token\n");
}

/* Concatenate argv[start..argc-1] into a single space-joined string. */

static void join_args(char *out, size_t out_cap, int argc, char *argv[],
                      int start)
{
  int i;

  out[0] = '\0';
  for (i = start; i < argc; i++)
    {
      if (i > start)
        {
          strlcat(out, " ", out_cap);
        }

      strlcat(out, argv[i], out_cap);
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, char *argv[])
{
  int ret;

  if (argc < 2)
    {
      print_usage();
      return -1;
    }

  if (feishu_notify_init() < 0)
    {
      printf("feishu: TLS init failed\n");
      return -1;
    }

  ret = -1;

  if (strcmp(argv[1], "token") == 0)
    {
      if (feishu_notify_get_token() < 0)
        {
          printf("feishu: token fetch failed\n");
        }
      else
        {
          printf("tenant_access_token: %s\n",
                 feishu_notify_get_access_token());
          ret = 0;
        }
    }
  else if (strcmp(argv[1], "send") == 0)
    {
      char text[1024];

      if (argc < 4)
        {
          printf("Usage: feishu send <chat_id> <text>\n");
        }
      else
        {
          join_args(text, sizeof(text), argc, argv, 3);
          ret = feishu_notify_send_text(argv[2], text);
        }
    }
  else if (strcmp(argv[1], "dm") == 0)
    {
      char text[1024];

      if (argc < 4)
        {
          printf("Usage: feishu dm <open_id> <text>\n");
        }
      else
        {
          join_args(text, sizeof(text), argc, argv, 3);
          ret = feishu_notify_send_dm(argv[2], text);
        }
    }
  else if (strcmp(argv[1], "test") == 0)
    {
      const char *chat_id = (argc >= 3) ? argv[2]
                                        : feishu_notify_get_default_chat_id();

      if (!chat_id || chat_id[0] == '\0')
        {
          printf("feishu: no chat_id configured\n");
        }
      else
        {
          ret = feishu_notify_send_text(chat_id,
                                        "hello from openvela (feishu test)");
        }
    }
  else
    {
      print_usage();
    }

  feishu_notify_deinit();
  return ret;
}
