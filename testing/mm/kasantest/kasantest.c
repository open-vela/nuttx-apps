/****************************************************************************
 * apps/testing/mm/kasantest/kasantest.c
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

#include <assert.h>
#include <errno.h>
#include <malloc.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <time.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/wait.h>

#include <nuttx/fs/procfs.h>
#include <nuttx/mm/mm.h>
#include <nuttx/mm/kasan.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#if defined(CONFIG_MM_KASAN_GLOBAL) || defined(CONFIG_MM_KASAN_SIM)
#  define KASANTEST_GLOBAL
#endif

#if defined(CONFIG_MM_KASAN_SIM)
#  define KASANTEST_STACK
#endif

/****************************************************************************
 * Private Types Prototypes
 ****************************************************************************/

typedef struct testcase_s
{
  bool (*func)(FAR struct mm_heap_s *heap, size_t size);
  bool is_auto;
  bool expect_fault;
  FAR const char *name;
} testcase_t;

typedef struct run_s
{
  FAR const testcase_t *testcase;
  FAR struct mm_heap_s *heap;
  size_t size;
} run_t;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static bool test_heap_underflow(FAR struct mm_heap_s *heap, size_t size);
static bool test_heap_overflow(FAR struct mm_heap_s *heap, size_t size);
static bool test_heap_use_after_free(FAR struct mm_heap_s *heap,
                                     size_t size);
static bool test_heap_invalid_free(FAR struct mm_heap_s *heap, size_t size);
static bool test_heap_double_free(FAR struct mm_heap_s *heap, size_t size);
static bool test_heap_poison(FAR struct mm_heap_s *heap, size_t size);
static bool test_heap_unpoison(FAR struct mm_heap_s *heap, size_t size);
static bool test_heap_illegal_memchr(FAR struct mm_heap_s *heap,
                                     size_t size);
static bool test_heap_illegal_memcpy(FAR struct mm_heap_s *heap,
                                     size_t size);
static bool test_heap_illegal_memcmp(FAR struct mm_heap_s *heap,
                                     size_t size);
static bool test_heap_illegal_memmove(FAR struct mm_heap_s *heap,
                                      size_t size);
static bool test_heap_illegal_memset(FAR struct mm_heap_s *heap,
                                     size_t size);
static bool test_heap_illegal_strcmp(FAR struct mm_heap_s *heap,
                                     size_t size);
static bool test_heap_illegal_strcpy(FAR struct mm_heap_s *heap,
                                     size_t size);
static bool test_heap_illegal_strlen(FAR struct mm_heap_s *heap,
                                     size_t size);
static bool test_heap_illegal_strncpy(FAR struct mm_heap_s *heap,
                                      size_t size);
static bool test_heap_illegal_strchr(FAR struct mm_heap_s *heap,
                                     size_t size);
static bool test_heap_illegal_strncmp(FAR struct mm_heap_s *heap,
                                      size_t size);
static bool test_heap_illegal_strnlen(FAR struct mm_heap_s *heap,
                                      size_t size);
static bool test_heap_illegal_strrchr(FAR struct mm_heap_s *heap,
                                      size_t size);
static bool test_heap_legal_memchr(FAR struct mm_heap_s *heap, size_t size);
static bool test_heap_legal_memcpy(FAR struct mm_heap_s *heap, size_t size);
static bool test_heap_legal_memcmp(FAR struct mm_heap_s *heap, size_t size);
static bool test_heap_legal_memmove(FAR struct mm_heap_s *heap, size_t size);
static bool test_heap_legal_memset(FAR struct mm_heap_s *heap, size_t size);
static bool test_heap_legal_strcmp(FAR struct mm_heap_s *heap, size_t size);
static bool test_heap_legal_strcpy(FAR struct mm_heap_s *heap, size_t size);
static bool test_heap_legal_strlen(FAR struct mm_heap_s *heap, size_t size);
static bool test_heap_legal_strncpy(FAR struct mm_heap_s *heap, size_t size);
static bool test_heap_legal_strchr(FAR struct mm_heap_s *heap, size_t size);
static bool test_heap_legal_strncmp(FAR struct mm_heap_s *heap, size_t size);
static bool test_heap_legal_strnlen(FAR struct mm_heap_s *heap, size_t size);
static bool test_heap_legal_strrchr(FAR struct mm_heap_s *heap, size_t size);

static bool test_insert_perf(FAR struct mm_heap_s *heap, size_t size);
static bool test_algorithm_perf(FAR struct mm_heap_s *heap, size_t size);

#ifdef KASANTEST_GLOBAL
static bool test_global_underflow(FAR struct mm_heap_s *heap, size_t size);
static bool test_global_overflow(FAR struct mm_heap_s *heap, size_t size);
#endif

#ifdef KASANTEST_STACK
static bool test_stack_underflow(FAR struct mm_heap_s *heap, size_t size);
static bool test_stack_overflow(FAR struct mm_heap_s *heap, size_t size);
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

const static testcase_t g_kasan_test[] =
{
  {test_heap_underflow, true, true, "heap underflow"},
  {test_heap_overflow, true, true, "heap overflow"},
  {test_heap_use_after_free, true, true, "heap use after free"},
  {test_heap_invalid_free, true, true, "heap invalid free"},
  {test_heap_double_free, true, true, "heap double free"},
  {test_heap_poison, true, true, "heap poison"},
  {test_heap_unpoison, true, false, "heap unpoison"},
  {test_heap_illegal_memchr, true, true, "heap illegal memchr"},
  {test_heap_illegal_memcpy, true, true, "heap illegal memcpy"},
  {test_heap_illegal_memcmp, true, true, "heap illegal memcmp"},
  {test_heap_illegal_memmove, true, true, "heap illegal memmove"},
  {test_heap_illegal_memset, true, true, "heap illegal memset"},
  {test_heap_illegal_strcmp, true, true, "heap illegal strcmp"},
  {test_heap_illegal_strcpy, true, true, "heap illegal strcpy"},
  {test_heap_illegal_strlen, true, true, "heap illegal strlen"},
  {test_heap_illegal_strncpy, true, true, "heap illegal strncpy"},
  {test_heap_illegal_strchr, true, true, "heap illegal strchr"},
  {test_heap_illegal_strncmp, true, true, "heap illegal strncmp"},
  {test_heap_illegal_strnlen, true, true, "heap illegal strnlen"},
  {test_heap_illegal_strrchr, true, true, "heap illegal strrchr"},
  {test_heap_legal_memchr, true, false, "heap legal memchr"},
  {test_heap_legal_memcpy, true, false, "heap legal memcpy"},
  {test_heap_legal_memcmp, true, false, "heap legal memcmp"},
  {test_heap_legal_memmove, true, false, "heap legal memmove"},
  {test_heap_legal_memset, true, false, "heap legal memset"},
  {test_heap_legal_strcmp, true, false, "heap legal strcmp"},
  {test_heap_legal_strcpy, true, false, "heap legal strcpy"},
  {test_heap_legal_strlen, true, false, "heap legal strlen"},
  {test_heap_legal_strncpy, true, false, "heap legal strncpy"},
  {test_heap_legal_strchr, true, false, "heap legal strchr"},
  {test_heap_legal_strncmp, true, false, "heap legal strncmp"},
  {test_heap_legal_strnlen, true, false, "heap legal strnlen"},
  {test_heap_legal_strrchr, true, false, "heap legal strrchr"},
  {test_insert_perf, false, false, "Kasan insert performance"},
  {test_algorithm_perf, false, false, "Kasan algorithm performance"},
#ifdef KASANTEST_GLOBAL
  {test_global_underflow, true, true, "globals underflow"},
  {test_global_overflow, true, true, "globals overflow"},
#endif
#ifdef KASANTEST_STACK
  {test_stack_underflow, true, true, "stack underflow"},
  {test_stack_overflow, true, true, "stack overflow"},
#endif
};

static char g_kasan_heap[10240] aligned_data(8);

#ifdef KASANTEST_GLOBAL
static char g_kasan_globals[32];
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void error_handler(void)
{
  int i;

  printf("Usage: kasantest [-h] [case_number]\n");
  printf("options:\n-h: show this help message\n");
  printf("case_number:\n");
  for (i = 0; i < nitems(g_kasan_test); i++)
    {
      printf("%d: %s\n", i + 1, g_kasan_test[i].name);
    }
}

static void print_heap_access(FAR const char *name, size_t requested,
                              FAR const void *base, size_t usable,
                              FAR const volatile void *target)
{
  printf("KASANTEST access: case=%s requested=%zu base=%p "
         "usable=%zu target=%p\n",
         name, requested, base, usable, (FAR const void *)target);
  fflush(stdout);
}

static void timespec_sub(struct timespec *dest,
                         struct timespec *ts1,
                         struct timespec *ts2)
{
  dest->tv_sec = ts1->tv_sec - ts2->tv_sec;
  dest->tv_nsec = ts1->tv_nsec - ts2->tv_nsec;

  if (dest->tv_nsec < 0)
    {
      dest->tv_nsec += 1000000000;
      dest->tv_sec -= 1;
    }
}

static bool test_heap_underflow(FAR struct mm_heap_s *heap, size_t size)
{
  FAR uint8_t *mem = mm_malloc(heap, size);
  FAR volatile uint8_t *target;
  size_t usable;

  if (mem == NULL)
    {
      return false;
    }

  usable = mm_malloc_size(heap, mem);
  target = mem - 1;
  print_heap_access("heap underflow", size, mem, usable, target);
  *target = 0x12;
  return false;
}

static bool test_heap_overflow(FAR struct mm_heap_s *heap, size_t size)
{
  FAR uint8_t *mem = mm_malloc(heap, size);
  FAR volatile uint8_t *target;
  size_t usable;

  if (mem == NULL)
    {
      return false;
    }

  usable = mm_malloc_size(heap, mem);
  target = mem + usable;
  print_heap_access("heap overflow", size, mem, usable, target);
  *target = 0x11;
  return false;
}

static bool test_heap_use_after_free(FAR struct mm_heap_s *heap, size_t size)
{
  FAR uint8_t *mem = mm_malloc(heap, size);
  FAR volatile uint8_t *target;
  size_t usable;

  if (mem == NULL)
    {
      return false;
    }

  usable = mm_malloc_size(heap, mem);
  target = mem;
  mm_free(heap, mem);
  print_heap_access("heap use after free", size, mem, usable, target);
  *target = 0x10;
  return false;
}

static bool test_heap_invalid_free(FAR struct mm_heap_s *heap, size_t size)
{
  int x;
  mm_free(heap, &x);
  return false;
}

static bool test_heap_double_free(FAR struct mm_heap_s *heap, size_t size)
{
  uint8_t *mem = mm_malloc(heap, size);

  mm_free(heap, mem);
  mm_free(heap, mem);
  return false;
}

static bool test_heap_poison(FAR struct mm_heap_s *heap, size_t size)
{
  FAR uint8_t *mem = mm_malloc(heap, size);
  size = mm_malloc_size(heap, mem);

  kasan_poison(mem, size);
  mem[0] = 0x10;
  return false;
}

static bool test_heap_unpoison(FAR struct mm_heap_s *heap, size_t size)
{
  FAR uint8_t *mem = mm_malloc(heap, size);
  size_t memsize = mm_malloc_size(heap, mem);

  kasan_poison(mem, memsize);
  kasan_unpoison(mem, memsize);
  mem[0] = 0x10;
  return true;
}

static bool test_heap_illegal_memchr(FAR struct mm_heap_s *heap, size_t size)
{
  FAR uint8_t *mem = mm_malloc(heap, size);
  size = mm_malloc_size(heap, mem);

  return memchr(mem, 0x00, size + 1) == NULL;
}

static bool test_heap_illegal_memcpy(FAR struct mm_heap_s *heap, size_t size)
{
  FAR uint8_t *src;
  FAR uint8_t *dst;

  size = size / 2;
  src =  mm_malloc(heap, size);
  size = mm_malloc_size(heap, src);
  dst = mm_malloc(heap, size);

  memcpy(dst, src, size);
  memcpy(dst, src, size + 4);
  return false;
}

static bool test_heap_illegal_memcmp(FAR struct mm_heap_s *heap, size_t size)
{
  FAR uint8_t *mem = mm_malloc(heap, size);
  size = mm_malloc_size(heap, mem);

  return memcmp(mem, mem + size, 1) < 0;
}

static bool test_heap_illegal_memmove(FAR struct mm_heap_s *heap,
                                      size_t size)
{
  FAR uint8_t *src;
  FAR uint8_t *dst;

  size = size / 2;
  src =  mm_malloc(heap, size);
  size = mm_malloc_size(heap, src);
  dst = mm_malloc(heap, size);

  memmove(dst, src, size);
  memmove(dst, src, size + 4);
  return false;
}

static bool test_heap_illegal_memset(FAR struct mm_heap_s *heap, size_t size)
{
  FAR uint8_t *mem = mm_malloc(heap, size);
  size = mm_malloc_size(heap, mem);

  memset(mem, 0x11, size + 1);
  return false;
}

static bool test_heap_illegal_strcmp(FAR struct mm_heap_s *heap, size_t size)
{
  FAR char *mem = mm_malloc(heap, size);
  size = mm_malloc_size(heap, mem);

  *(int *)mem = rand();
  return strcmp(mem, mem + size) == 0;
}

static bool test_heap_illegal_strcpy(FAR struct mm_heap_s *heap, size_t size)
{
  FAR char *dst = mm_malloc(heap, 16);
  FAR char *src;
  int i;
  size = mm_malloc_size(heap, dst);
  src = mm_malloc(heap, size + 16);

  for (i = 0; i < size + 16; i++)
    {
      src[i] = 'a';
    }

  strcpy(dst, src);
  return false;
}

static bool test_heap_illegal_strlen(FAR struct mm_heap_s *heap, size_t size)
{
  FAR char *mem = mm_malloc(heap, size);
  size = mm_malloc_size(heap, mem);

  return strlen(mem + size) < 0;
}

static bool test_heap_illegal_strncpy(FAR struct mm_heap_s *heap,
                                      size_t size)
{
  FAR char *dst = mm_malloc(heap, size);
  const char *src = "Hello, World!";

  size = mm_malloc_size(heap, dst);
  strncpy(dst, src, size + 1);
  return false;
}

static bool test_heap_illegal_strchr(FAR struct mm_heap_s *heap, size_t size)
{
  FAR char *mem = mm_malloc(heap, size);
  size = mm_malloc_size(heap, mem);

  return strchr(mem + size, 0x00) == NULL;
}

static bool test_heap_illegal_strncmp(FAR struct mm_heap_s *heap,
                                      size_t size)
{
  FAR char *mem1 = mm_malloc(heap, size / 2);
  FAR char *mem2 = mm_malloc(heap, size / 2);
  size = mm_malloc_size(heap, mem2);

  *(int *)mem1 = rand();
  return strncmp(mem1, mem2 + size, size) == 0;
}

static bool test_heap_illegal_strnlen(FAR struct mm_heap_s *heap,
                                      size_t size)
{
  FAR char *mem = mm_malloc(heap, size);
  size = mm_malloc_size(heap, mem);

  return strnlen(mem + size, size) < 0;
}

static bool test_heap_illegal_strrchr(FAR struct mm_heap_s *heap,
                                      size_t size)
{
  FAR char *mem = mm_malloc(heap, size);
  size = mm_malloc_size(heap, mem);

  return strrchr(mem + size, 0x00) == NULL;
}

static bool test_heap_legal_memchr(FAR struct mm_heap_s *heap, size_t size)
{
  FAR char *mem = mm_malloc(heap, size);
  FAR volatile char *target;
  size_t usable;

  if (mem == NULL)
    {
      return false;
    }

  usable = mm_malloc_size(heap, mem);
  target = mem + usable - 1;
  print_heap_access("heap legal memchr", size, mem, usable, target);
  memset(mem, 0, usable);
  *target = 0x01;

  return memchr(mem, 0x01, usable) == (FAR void *)target;
}

static bool test_heap_legal_memcpy(FAR struct mm_heap_s *heap, size_t size)
{
  FAR char *des = mm_malloc(heap, size / 2);
  FAR char *src = mm_malloc(heap, size / 2);
  size_t des_size = mm_malloc_size(heap, des);
  size_t src_size = mm_malloc_size(heap, src);

  return memcpy(des, src, des_size > src_size ? src_size : des_size);
}

static bool test_heap_legal_memcmp(FAR struct mm_heap_s *heap, size_t size)
{
  FAR char *des = mm_malloc(heap, size / 2);
  FAR char *src = mm_malloc(heap, size / 2);
  size_t des_size = mm_malloc_size(heap, des);
  size_t src_size = mm_malloc_size(heap, src);

  des[des_size - 1] = 0x01;
  src[src_size - 1] = 0x02;

  return memcmp(des, src, des_size > src_size ? src_size : des_size);
}

static bool test_heap_legal_memmove(FAR struct mm_heap_s *heap, size_t size)
{
  FAR char *des = mm_malloc(heap, size / 2);
  FAR char *src = mm_malloc(heap, size / 2);
  size_t des_size = mm_malloc_size(heap, des);
  size_t src_size = mm_malloc_size(heap, src);

  return memmove(des, src, des_size > src_size ? src_size : des_size);
}

static bool test_heap_legal_memset(FAR struct mm_heap_s *heap, size_t size)
{
  FAR char *des = mm_malloc(heap, size / 2);
  size = mm_malloc_size(heap, des);

  return memset(des, 0xef, size);
}

static bool test_heap_legal_strcmp(FAR struct mm_heap_s *heap, size_t size)
{
  FAR char *mem = mm_malloc(heap, size);
  FAR char *str = "hello world";

  size = mm_malloc_size(heap, mem);
  strcpy(mem, str);
  return !strcmp(mem, str);
}

static bool test_heap_legal_strcpy(FAR struct mm_heap_s *heap, size_t size)
{
  FAR char *mem = mm_malloc(heap, size);
  FAR char *str = "hello world";

  size = mm_malloc_size(heap, mem);
  return strcpy(mem, str);
}

static bool test_heap_legal_strlen(FAR struct mm_heap_s *heap, size_t size)
{
  FAR char *mem = mm_malloc(heap, size);

  return strlen(mem);
}

static bool test_heap_legal_strncpy(FAR struct mm_heap_s *heap, size_t size)
{
  FAR char *dst = mm_malloc(heap, size);
  const char *src = "Hello, World!";

  size = mm_malloc_size(heap, dst);
  return strncpy(dst, src, size);
}

static bool test_heap_legal_strchr(FAR struct mm_heap_s *heap, size_t size)
{
  FAR char *mem = mm_malloc(heap, size);

  memset(mem, 0xff, size);
  mem[size / 2 - 1] = 0x01;

  return strchr(mem, 0x01);
}

static bool test_heap_legal_strncmp(FAR struct mm_heap_s *heap, size_t size)
{
  FAR char *mem1 = mm_malloc(heap, size / 2);
  FAR char *mem2 = mm_malloc(heap, size / 2);

  memset(mem1, 0xff, size / 2 - 1);
  memset(mem2, 0xff, size / 2 - 1);
  mem1[size / 2 - 2] = 0x01;
  mem2[size / 2 - 2] = 0x02;

  return strncmp(mem1, mem2, size) != 0;
}

static bool test_heap_legal_strnlen(FAR struct mm_heap_s *heap, size_t size)
{
  FAR char *mem = mm_malloc(heap, size);
  size = mm_malloc_size(heap, mem);

  mem[size - 1] = 0x00;
  return strnlen(mem, size);
}

static bool test_heap_legal_strrchr(FAR struct mm_heap_s *heap, size_t size)
{
  FAR char *mem = mm_malloc(heap, size);
  size = mm_malloc_size(heap, mem);

  mem[size - 1] = 0;
  return strrchr(mem, 0x00);
}

static bool test_insert_perf(FAR struct mm_heap_s *heap, size_t size)
{
  int num = 0;
  char value;
  char *p;
  int i;

  p = (char *)malloc(CONFIG_TESTING_KASAN_PERF_HEAP_SIZE);
  if (!p)
    {
      printf("Failed to allocate memory for performance testing\n");
      return false;
    }

  do
    {
      value = num % INT8_MAX;
      for (i = 0; i < CONFIG_TESTING_KASAN_PERF_HEAP_SIZE; i++)
        {
          p[i] = value;
        }
    }
  while (num++ < CONFIG_TESTING_KASAN_PERF_CYCLES);

  free(p);
  return true;
}

static bool test_algorithm_perf(FAR struct mm_heap_s *heap, size_t size)
{
  int num = 0;
  char *p;

  p = (char *)malloc(CONFIG_TESTING_KASAN_PERF_HEAP_SIZE);
  if (!p)
    {
      printf("Failed to allocate memory for performance testing\n");
      return false;
    }

  do
    {
      memset(p, num % INT8_MAX, CONFIG_TESTING_KASAN_PERF_HEAP_SIZE);
    }
  while (num++ < CONFIG_TESTING_KASAN_PERF_CYCLES);

  free(p);
  return true;
}

static bool test_status_passed(FAR const testcase_t *test, int status)
{
  return !test->expect_fault && WIFEXITED(status) &&
         WEXITSTATUS(status) == EXIT_SUCCESS;
}

#ifdef KASANTEST_GLOBAL
static bool test_global_underflow(FAR struct mm_heap_s *heap, size_t size)
{
  memset(g_kasan_globals - 1, 0x12, sizeof(g_kasan_globals));
  return false;
}

static bool test_global_overflow(FAR struct mm_heap_s *heap, size_t size)
{
  memset(g_kasan_globals + sizeof(g_kasan_globals), 0xef, 1);
  return false;
}
#endif

#ifdef KASANTEST_STACK
static bool test_stack_underflow(FAR struct mm_heap_s *heap, size_t size)
{
  char stack[0];
  memset(stack - 16, 0xef, 16);
  return false;
}

static bool test_stack_overflow(FAR struct mm_heap_s *heap, size_t size)
{
  char stack[0];
  memset(stack, 0xef, 16);
  return false;
}
#endif

static int run_child(int argc, FAR char *argv[])
{
  FAR run_t *run = (FAR run_t *)g_kasan_heap;
  struct timespec result;
  struct timespec start;
  struct timespec end;
  bool ret;

  clock_gettime(CLOCK_MONOTONIC, &start);
  ret = run->testcase->func(run->heap, run->size);
  clock_gettime(CLOCK_MONOTONIC, &end);

  timespec_sub(&result, &end, &start);
  printf("%s spending %ld.%lds\n", run->testcase->name,
         result.tv_sec, result.tv_nsec);

  if (run->testcase->expect_fault)
    {
      printf("KASANTEST expected fault was not triggered\n");
      return EXIT_SUCCESS;
    }

  return ret ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int run_test(FAR const testcase_t *test)
{
  size_t heap_size = sizeof(g_kasan_heap) - sizeof(run_t);
  FAR run_t *run;
  int status;
  int ret;
  pid_t pid;

  run = (run_t *)g_kasan_heap;
  run->testcase = test;
  run->size = rand() % (heap_size / 2) + 1;
  run->heap = mm_initialize("kasan", (struct mm_heap_s *)&run[1], heap_size);
  if (!run->heap)
    {
      return ERROR;
    }

  pid = task_create("kasantest", CONFIG_TESTING_KASAN_PRIORITY,
                    CONFIG_TESTING_KASAN_STACKSIZE, run_child, NULL);
  if (pid < 0)
    {
      printf("KASANTEST task create failed: %d\n", errno);
      mm_uninitialize(run->heap);
      return ERROR;
    }

  do
    {
      ret = waitpid(pid, &status, 0);
    }
  while (ret < 0 && errno == EINTR);

  if (ret != pid)
    {
      printf("KASANTEST wait failed: %d\n", errno);
      return ERROR;
    }

  mm_uninitialize(run->heap);
  return status;
}

static int run_testcase(int argc, FAR char *argv[])
{
  uintptr_t index = strtoul(argv[1], NULL, 0);
  int ret;

  /* Pass in the number to run the specified case,
   * and the string of the number will not be very long
   */

  if (strlen(argv[1]) <= 3)
    {
      if (memcmp(argv[1], "-h", 2) == 0
          || index <= 0 || index > nitems(g_kasan_test))
        {
          error_handler();
        }
      else
        {
          FAR const testcase_t *test = &g_kasan_test[index - 1];

          ret = run_test(test);
          if (ret == ERROR)
            {
              printf("KASANTEST result: case=%lu name=%s FAIL status=%d\n",
                     (unsigned long)index, test->name, ret);
              return EXIT_FAILURE;
            }

          if (test->expect_fault)
            {
              if (WIFEXITED(ret) && WEXITSTATUS(ret) == EXIT_SUCCESS)
                {
                  printf("KASANTEST result: case=%lu name=%s FAIL "
                         "status=%d\n",
                         (unsigned long)index, test->name, ret);
                }
              else
                {
                  printf("KASANTEST result: case=%lu name=%s FAULT "
                         "status=%d verification=required\n",
                         (unsigned long)index, test->name, ret);
                }

              return EXIT_FAILURE;
            }

          if (!test_status_passed(test, ret))
            {
              printf("KASANTEST result: case=%lu name=%s FAIL status=%d\n",
                     (unsigned long)index, test->name, ret);
              return EXIT_FAILURE;
            }

          printf("KASANTEST result: case=%lu name=%s PASS status=%d\n",
                 (unsigned long)index, test->name, ret);
        }

      return EXIT_SUCCESS;
    }

  return EXIT_FAILURE;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  int status[nitems(g_kasan_test)];
  bool passed = true;
  size_t i;

  if (argc < 2)
    {
      for (i = 0; i < nitems(g_kasan_test); i++)
        {
          if (g_kasan_test[i].is_auto)
            {
              printf("KASan test: %s\n", g_kasan_test[i].name);
              status[i] = run_test(&g_kasan_test[i]);
            }
        }

      for (i = 0; i < nitems(status); i++)
        {
          if (g_kasan_test[i].is_auto)
            {
              bool test_passed;

              test_passed = test_status_passed(&g_kasan_test[i], status[i]);
              if (g_kasan_test[i].expect_fault &&
                  !(WIFEXITED(status[i]) &&
                    WEXITSTATUS(status[i]) == EXIT_SUCCESS))
                {
                  printf("KASan Test: %s -> FAULT "
                         "(external report verification required)\n",
                         g_kasan_test[i].name);
                }
              else
                {
                  printf("KASan Test: %s -> %s\n", g_kasan_test[i].name,
                         test_passed ? "\033[32mPASS\033[0m" :
                                       "\033[31mFAIL\033[0m");
                }

              passed &= test_passed;
            }
        }
    }
  else
    {
      return run_testcase(argc, argv);
    }

  return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
