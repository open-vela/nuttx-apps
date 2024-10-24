/****************************************************************************
 * apps/examples/hello/hello_main.c
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
#include <sys/types.h>

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * hello_main
 ****************************************************************************/

static int hello(int argc, FAR char *argv[]) {
  printf("Hello, World!!\n");
  return 0;
}

static int breakpoint(int argc, FAR char *argv[]) {
  printf("Breakpoint\n");
  return 0;
}

static int step(int argc, FAR char *argv[]) {
  int i = 0;
  printf("Next, World!!\n");
  while (i < 10) {
    printf("Step %d\n", i);
    i++;
  }
  return 0;
}

static char *g_wild_pointer;
static int print(int argc, FAR char *argv[]) {
  static char g_buffer[64];
  int i = 0;

  g_wild_pointer = g_buffer;
  snprintf(g_buffer, sizeof(g_buffer), "%d buffer\n", i);
  while (i < 10) {
    g_buffer[0] = '0' + i % 10;
    printf("%s", g_buffer);
    fflush(stdout);
    i++;
    sleep(1);
  }
  return 0;
}

static int watchpoint(int argc, FAR char *argv[]) {
  printf("Watchpoint\n");
  if (g_wild_pointer == NULL)
    return 0;

  strcpy(g_wild_pointer, "Wild pointer write.\n");
  return 0;
}

nooptimiziation_function
static int corruption(int argc, FAR char *argv[]) {
  printf("Corruption\n");
  int *p = (int *)malloc(1024);
  for (int i = 0; i < 12; i++)
    {
      p[-i] = 0xdeadbeef;
    }

  return 0;
}

nooptimiziation_function
static int leak(int argc, FAR char *argv[]) {
  size_t size = rand() % 1024;
  int *p = (int *)malloc(size);
  printf("Leak: %p: %zu\n", p, size);
  return 0;
}

#define ADD_COMMAND(name) {#name, name}

static const struct {
  const char *cmd;
  int (*func)(int argc, FAR char *argv[]);
} g_commands[] = {
    ADD_COMMAND(hello),
    ADD_COMMAND(breakpoint),
    ADD_COMMAND(step),
    ADD_COMMAND(print),
    ADD_COMMAND(watchpoint),
    ADD_COMMAND(corruption),
    ADD_COMMAND(leak),
};

int main(int argc, FAR char *argv[]) {
  int i;
  if (argc < 2) {
    printf("Usage: %s <command>\n", argv[0]);
    return 1;
  }

  for (i = 0; i < sizeof(g_commands) / sizeof(g_commands[0]); i++) {
    if (strcmp(argv[1], g_commands[i].cmd) == 0) {
      return g_commands[i].func(argc, argv);
    }
  }

  printf("Unknown command: %s\n", argv[1]);
  return 0;
}
