/****************************************************************************
 * apps/examples/uio/uio_test.c
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

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define DEV_INDEX          1
#define SIZE_INDEX         2
#define W_TEST_NUM_INDEX   3
#define INPUT_NUM          4

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  struct pollfd events;
  FAR int *access_addr;
  char mem_size[32];
  ssize_t size;
  int fd;
  int size_fd;
  int num;
  int ret;
  int i;
  int counter = 0;

  if (argc != INPUT_NUM)
    {
      printf("Usage: %s <dev> <size> <num>\n", argv[0]);
      return -EINVAL;
    }

  /* open uio related files */

  fd = open(argv[DEV_INDEX], O_RDWR);
  if (fd < 0)
    {
      fprintf(stderr, "open %s: %s\n", argv[DEV_INDEX], strerror(errno));
      return fd;
    }

  size_fd = open(argv[SIZE_INDEX], O_RDONLY);
  if (size_fd < 0)
    {
      fprintf(stderr, "open %s: %s\n",
              argv[SIZE_INDEX], strerror(errno));
      ret = size_fd;
      goto err_size;
    }

  ret = read(size_fd, mem_size, sizeof(mem_size));
  if (ret < 0)
    {
      fprintf(stderr, "read %s: %s\n",
              argv[SIZE_INDEX], strerror(errno));
      goto err;
    }

  size = strtol(mem_size, NULL, 0);

  num = atoi(argv[W_TEST_NUM_INDEX]);
  if (num > size / sizeof(int))
    {
      printf("input num exceeds shmem range\n");
      ret = -EINVAL;
      goto err;
    }

  /* mmap /dev/uio addr */

  access_addr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                     MAP_SHARED, fd, 0);
  if (access_addr == MAP_FAILED)
    {
      printf("mmap %s: %s\n", argv[DEV_INDEX], strerror(errno));
      ret = errno;
      goto err;
    }

  printf("The device (lenth %ld)\n" "logical address %p\n",
         size, access_addr);

  /* read/write uio mem */

  for (i = 0; i < num; i++)
    {
      access_addr[i] += 1;
      printf("i:%u, write %u\n", i, access_addr[i]);
    }

  memset(&events, 0 , sizeof(events));
  events.fd = fd;
  events.events = POLLIN | POLLRDNORM;

  while (1)
    {
      ret = poll(&events, 1, 1000);
      if (ret < 0)
        {
          printf("[poll] read POLL ERROR, break\n");
          break;
        }
      else if (ret == 0)
        {
          printf("[poll] read POLL timeout, continue\n");
          continue;
        }
      else
        {
          if ((events.revents & (POLLIN | POLLRDNORM)) != 0)
            {
              ret = read(fd, &counter, 4);
              if (ret < 0)
                {
                  printf("[poll] read error, error:%s, errno = %d\n",
                         strerror(errno), errno);
                  if (errno == ENODEV)
                    {
                      printf("[poll] device disconnect, break\n");
                      break;
                    }
                }
              else
                {
                  printf("Interrupt number is :%u\n", counter);
                  break;
                }
            }
          else if (((events.revents & POLLERR) != 0) ||
                   ((events.revents & POLLHUP) != 0))
            {
              printf("[poll] device error, break\n");
              break;
            }
        }
    }

  munmap(access_addr, size);

err:
  close(size_fd);
err_size:
  close(fd);

  return ret;
}
