/****************************************************************************
 * apps/examples/lvgldemo/lvgldemo.c
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
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/boardctl.h>

#include <lvgl/lvgl.h>
#include <lvgl/demos/lv_demos.h>
#ifdef CONFIG_LV_USE_NUTTX_LIBUV
#include <uv.h>
#endif

extern const unsigned char lvgldemo_font_common_16_data[];
extern const unsigned int lvgldemo_font_common_16_data_len;
extern const lv_font_t lvgldemo_font_common_16_static;

#if defined(CONFIG_PIPES) && defined(CONFIG_LIBC_EXECFUNCS) && \
    defined(CONFIG_LV_FONT_UNSCII_16) && defined(CONFIG_LV_USE_KEYBOARD) && \
    defined(CONFIG_LV_USE_TEXTAREA)
#  define LVGLDEMO_DASHBOARD_TERMINAL 1
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Should we perform board-specific driver initialization? There are two
 * ways that board initialization can occur:  1) automatically via
 * board_late_initialize() during bootupif CONFIG_BOARD_LATE_INITIALIZE
 * or 2).
 * via a call to boardctl() if the interface is enabled
 * (CONFIG_BOARDCTL=y).
 * If this task is running as an NSH built-in application, then that
 * initialization has probably already been performed otherwise we do it
 * here.
 */

#undef NEED_BOARDINIT

#if defined(CONFIG_BOARDCTL) && !defined(CONFIG_NSH_ARCHINIT)
#  define NEED_BOARDINIT 1
#endif

/* P4 Outbound follows the warm engineering console palette documented in
 * ESP32-P4移植资料/ui.md.  Keep the dashboard and widgets theme unchanged. */

#define LVGLDEMO_UI_CANVAS       lv_color_hex(0xefede6)
#define LVGLDEMO_UI_PAPER        lv_color_hex(0xfbfaf6)
#define LVGLDEMO_UI_PAPER_WARM   lv_color_hex(0xf5f2ea)
#define LVGLDEMO_UI_PAPER_STRONG lv_color_hex(0xebe7dd)
#define LVGLDEMO_UI_INK          lv_color_hex(0x29302f)
#define LVGLDEMO_UI_INK_SOFT     lv_color_hex(0x4f5956)
#define LVGLDEMO_UI_MUTED        lv_color_hex(0x747c78)
#define LVGLDEMO_UI_FAINT        lv_color_hex(0x969c98)
#define LVGLDEMO_UI_LINE         lv_color_hex(0xd8d4ca)
#define LVGLDEMO_UI_LINE_SOFT    lv_color_hex(0xe7e3da)
#define LVGLDEMO_UI_STEEL        lv_color_hex(0x426879)
#define LVGLDEMO_UI_STEEL_DEEP   lv_color_hex(0x304f5d)
#define LVGLDEMO_UI_STEEL_SOFT   lv_color_hex(0xdfe8e9)
#define LVGLDEMO_UI_SAGE         lv_color_hex(0x647b70)
#define LVGLDEMO_UI_SAGE_SOFT    lv_color_hex(0xe4ebe5)
#define LVGLDEMO_UI_GREEN        lv_color_hex(0x4f8064)
#define LVGLDEMO_UI_GREEN_SOFT   lv_color_hex(0xe6efe8)
#define LVGLDEMO_UI_AMBER        lv_color_hex(0xa8752d)
#define LVGLDEMO_UI_AMBER_SOFT   lv_color_hex(0xf3ead7)
#define LVGLDEMO_UI_BRICK        lv_color_hex(0x985749)
#define LVGLDEMO_UI_BRICK_SOFT   lv_color_hex(0xf2e5e0)
#define LVGLDEMO_UI_GRAPHITE     lv_color_hex(0x202c30)

/****************************************************************************
 * Private Type Declarations
 ****************************************************************************/

enum lvgldemo_outbound_page_e
{
  LVGLDEMO_OUTBOUND_HOME = 0,
  LVGLDEMO_OUTBOUND_SCAN,
  LVGLDEMO_OUTBOUND_DISPATCH,
  LVGLDEMO_OUTBOUND_STATUS
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

#ifndef CONFIG_DISABLE_SIGNALS
static volatile sig_atomic_t g_lvgldemo_exit;
#endif

static enum lvgldemo_outbound_page_e g_outbound_page;
static int g_outbound_scanned;
static int g_outbound_vehicle = -1;
static int g_outbound_dispatch_notice;
static int g_outbound_task_state;
static int g_outbound_exception;
static int g_outbound_exception_type;
static int g_outbound_paused;
static lv_font_t *g_outbound_font;
static int g_outbound_font_dynamic;

#define LVGLDEMO_OUTBOUND_FONT_PATH    "/data/lvgldemo_zh_common_16_nocompress.bin"
#define LVGLDEMO_OUTBOUND_FONT_LV_PATH "S:/data/lvgldemo_zh_common_16_nocompress.bin"

static void lvgldemo_dashboard_create(void);
static void lvgldemo_outbound_create(void);

#ifdef LVGLDEMO_DASHBOARD_TERMINAL
#  define LVGLDEMO_READ_PIPE  0
#  define LVGLDEMO_WRITE_PIPE 1

static int g_dashboard_nsh_stdin[2] = {-1, -1};
static int g_dashboard_nsh_stdout[2] = {-1, -1};
static int g_dashboard_nsh_stderr[2] = {-1, -1};
static pid_t g_dashboard_nsh_pid = -1;
static lv_obj_t *g_dashboard_nsh_output;
static lv_obj_t *g_dashboard_nsh_input;
static lv_obj_t *g_dashboard_nsh_keyboard;
static lv_timer_t *g_dashboard_nsh_timer;

static char * const g_dashboard_nsh_argv[] =
{
  "nsh", NULL
};
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

#ifndef CONFIG_DISABLE_SIGNALS
static void lvgldemo_sigint(int signo)
{
  if (signo == SIGINT)
    {
      /* Only set a flag here.  LVGL cleanup must run in the main task. */

      g_lvgldemo_exit = 1;
    }
}
#endif

static void lvgldemo_apply_ui_font(lv_obj_t *object)
{
  if (object != NULL && g_outbound_font != NULL)
    {
      lv_obj_set_style_text_font(object, g_outbound_font,
                                 LV_PART_MAIN);
    }
}

static int lvgldemo_outbound_font_install(void)
{
  int fd;
  struct stat status;
  size_t written = 0;

  if (stat(LVGLDEMO_OUTBOUND_FONT_PATH, &status) == 0 &&
      status.st_size == lvgldemo_font_common_16_data_len)
    {
      return 0;
    }

  fd = open(LVGLDEMO_OUTBOUND_FONT_PATH,
            O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (fd < 0)
    {
      printf("lvgldemo: cannot install common font: %d\n", errno);
      return -1;
    }

  while (written < lvgldemo_font_common_16_data_len)
    {
      ssize_t result;

      result = write(fd, lvgldemo_font_common_16_data + written,
                     lvgldemo_font_common_16_data_len - written);
      if (result <= 0)
        {
          printf("lvgldemo: common font write failed: %d\n", errno);
          close(fd);
          return -1;
        }

      written += result;
    }

  close(fd);
  printf("lvgldemo: installed common font (%u bytes)\n",
         lvgldemo_font_common_16_data_len);
  return 0;
}

static void lvgldemo_outbound_font_load(void)
{
  /* Keep the POSIX asset for boards where /data is available, but do not
   * make the Chinese UI depend on SmartFS/font-file loading succeeding. */

  (void)lvgldemo_outbound_font_install();
  g_outbound_font = lv_binfont_create(LVGLDEMO_OUTBOUND_FONT_LV_PATH);
  if (g_outbound_font != NULL)
    {
      g_outbound_font_dynamic = 1;
      printf("lvgldemo: common font loaded from %s\n",
             LVGLDEMO_OUTBOUND_FONT_LV_PATH);
    }
  else
    {
      /* This font is linked into the lvgldemo image and is therefore
       * available even when the filesystem font loader is unavailable. */

      g_outbound_font = (lv_font_t *)&lvgldemo_font_common_16_static;
      g_outbound_font_dynamic = 0;
      printf("lvgldemo: using embedded common font\n");
    }
}

static void lvgldemo_outbound_font_unload(void)
{
  if (g_outbound_font != NULL && g_outbound_font_dynamic != 0)
    {
      lv_binfont_destroy(g_outbound_font);
    }

  g_outbound_font = NULL;
  g_outbound_font_dynamic = 0;
}

#ifdef LVGLDEMO_DASHBOARD_TERMINAL
static void lvgldemo_dashboard_close_fd(int *fd)
{
  if (*fd >= 0)
    {
      close(*fd);
      *fd = -1;
    }
}

static void lvgldemo_dashboard_close_pipes(void)
{
  lvgldemo_dashboard_close_fd(&g_dashboard_nsh_stdin[0]);
  lvgldemo_dashboard_close_fd(&g_dashboard_nsh_stdin[1]);
  lvgldemo_dashboard_close_fd(&g_dashboard_nsh_stdout[0]);
  lvgldemo_dashboard_close_fd(&g_dashboard_nsh_stdout[1]);
  lvgldemo_dashboard_close_fd(&g_dashboard_nsh_stderr[0]);
  lvgldemo_dashboard_close_fd(&g_dashboard_nsh_stderr[1]);
}

static void lvgldemo_dashboard_append_output(int fd)
{
  char buffer[128];
  struct pollfd descriptor;
  ssize_t length;

  if (fd < 0 || g_dashboard_nsh_output == NULL)
    {
      return;
    }

  descriptor.fd = fd;
  descriptor.events = POLLIN;
  descriptor.revents = 0;

  if (poll(&descriptor, 1, 0) <= 0 ||
      (descriptor.revents & (POLLIN | POLLHUP | POLLERR)) == 0)
    {
      return;
    }

  length = read(fd, buffer, sizeof(buffer) - 1);
  if (length > 0)
    {
      buffer[length] = '\0';
      lv_textarea_add_text(g_dashboard_nsh_output, buffer);
    }
}

static void lvgldemo_dashboard_terminal_timer(lv_timer_t *timer)
{
  int status;

  (void)timer;
  lvgldemo_dashboard_append_output(
      g_dashboard_nsh_stdout[LVGLDEMO_READ_PIPE]);
  lvgldemo_dashboard_append_output(
      g_dashboard_nsh_stderr[LVGLDEMO_READ_PIPE]);

  if (g_dashboard_nsh_pid > 0 &&
      waitpid(g_dashboard_nsh_pid, &status, WNOHANG) ==
      g_dashboard_nsh_pid)
    {
      g_dashboard_nsh_pid = -1;
      if (g_dashboard_nsh_output != NULL)
        {
          lv_textarea_add_text(g_dashboard_nsh_output,
                               "\n[NSH session exited]\n");
        }
    }
}

static void lvgldemo_dashboard_terminal_input(lv_event_t *event)
{
  lv_event_code_t code = lv_event_get_code(event);

  if (code == LV_EVENT_FOCUSED)
    {
      if (g_dashboard_nsh_keyboard != NULL)
        {
          lv_obj_remove_flag(g_dashboard_nsh_keyboard, LV_OBJ_FLAG_HIDDEN);
        }

      return;
    }

  if (code == LV_EVENT_READY && g_dashboard_nsh_input != NULL &&
      g_dashboard_nsh_pid > 0)
    {
      const char *command = lv_textarea_get_text(g_dashboard_nsh_input);

      if (command != NULL && command[0] != '\0')
        {
          size_t length = strlen(command);

          if (write(g_dashboard_nsh_stdin[LVGLDEMO_WRITE_PIPE],
                    command, length) == (ssize_t)length)
            {
              (void)write(g_dashboard_nsh_stdin[LVGLDEMO_WRITE_PIPE],
                          "\n", 1);
              lv_textarea_set_text(g_dashboard_nsh_input, "");
            }
        }

      if (g_dashboard_nsh_keyboard != NULL)
        {
          lv_obj_add_flag(g_dashboard_nsh_keyboard, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static int lvgldemo_dashboard_start_nsh(void)
{
  posix_spawn_file_actions_t actions;
  pid_t pid;
  int ret;

  if (pipe(g_dashboard_nsh_stdin) < 0 ||
      pipe(g_dashboard_nsh_stdout) < 0 ||
      pipe(g_dashboard_nsh_stderr) < 0)
    {
      ret = errno;
      lvgldemo_dashboard_close_pipes();
      return -ret;
    }

  ret = posix_spawn_file_actions_init(&actions);
  if (ret != 0)
    {
      lvgldemo_dashboard_close_pipes();
      return -ret;
    }

  ret = posix_spawn_file_actions_adddup2(
      &actions, g_dashboard_nsh_stdin[LVGLDEMO_READ_PIPE], STDIN_FILENO);
  if (ret == 0)
    {
      ret = posix_spawn_file_actions_adddup2(
          &actions, g_dashboard_nsh_stdout[LVGLDEMO_WRITE_PIPE],
          STDOUT_FILENO);
    }

  if (ret == 0)
    {
      ret = posix_spawn_file_actions_adddup2(
          &actions, g_dashboard_nsh_stderr[LVGLDEMO_WRITE_PIPE],
          STDERR_FILENO);
    }

  if (ret == 0)
    {
      ret = posix_spawn_file_actions_addclose(
          &actions, g_dashboard_nsh_stdin[LVGLDEMO_WRITE_PIPE]);
    }

  if (ret == 0)
    {
      ret = posix_spawn_file_actions_addclose(
          &actions, g_dashboard_nsh_stdout[LVGLDEMO_READ_PIPE]);
    }

  if (ret == 0)
    {
      ret = posix_spawn_file_actions_addclose(
          &actions, g_dashboard_nsh_stderr[LVGLDEMO_READ_PIPE]);
    }

  if (ret == 0)
    {
      ret = posix_spawn(&pid, "nsh", &actions, NULL,
                        g_dashboard_nsh_argv, NULL);
    }

  posix_spawn_file_actions_destroy(&actions);
  if (ret != 0)
    {
      lvgldemo_dashboard_close_pipes();
      return -ret;
    }

  g_dashboard_nsh_pid = pid;
  lvgldemo_dashboard_close_fd(
      &g_dashboard_nsh_stdin[LVGLDEMO_READ_PIPE]);
  lvgldemo_dashboard_close_fd(
      &g_dashboard_nsh_stdout[LVGLDEMO_WRITE_PIPE]);
  lvgldemo_dashboard_close_fd(
      &g_dashboard_nsh_stderr[LVGLDEMO_WRITE_PIPE]);

  g_dashboard_nsh_timer = lv_timer_create(
      lvgldemo_dashboard_terminal_timer, 100, NULL);
  return g_dashboard_nsh_timer == NULL ? -ENOMEM : 0;
}

static void lvgldemo_dashboard_stop_nsh(void)
{
  int status;

  if (g_dashboard_nsh_timer != NULL)
    {
      lv_timer_delete(g_dashboard_nsh_timer);
      g_dashboard_nsh_timer = NULL;
    }

  if (g_dashboard_nsh_pid > 0)
    {
      (void)kill(g_dashboard_nsh_pid, SIGTERM);
      (void)waitpid(g_dashboard_nsh_pid, &status, 0);
      g_dashboard_nsh_pid = -1;
    }

  lvgldemo_dashboard_close_pipes();
  g_dashboard_nsh_output = NULL;
  g_dashboard_nsh_input = NULL;
  g_dashboard_nsh_keyboard = NULL;
}

static void lvgldemo_dashboard_create_terminal(lv_obj_t *screen)
{
  lv_obj_t *panel;
  lv_obj_t *title;

  panel = lv_obj_create(screen);
  if (panel == NULL)
    {
      return;
    }

  lv_obj_set_size(panel, 968, 220);
  lv_obj_align(panel, LV_ALIGN_TOP_LEFT, 28, 352);
  lv_obj_set_style_bg_color(panel, lv_color_hex(0x111827), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(panel, lv_color_hex(0x334155), LV_PART_MAIN);
  lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(panel, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_all(panel, 8, LV_PART_MAIN);

  title = lv_label_create(panel);
  if (title != NULL)
    {
      lv_label_set_text(title, "NSH Terminal  |  tap input to open keyboard");
      lv_obj_set_style_text_color(title, lv_color_hex(0x93c5fd),
                                  LV_PART_MAIN);
      lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
    }

  g_dashboard_nsh_output = lv_textarea_create(panel);
  if (g_dashboard_nsh_output != NULL)
    {
      lv_obj_set_size(g_dashboard_nsh_output, 950, 122);
      lv_obj_align(g_dashboard_nsh_output, LV_ALIGN_TOP_LEFT, 0, 25);
      lv_obj_set_style_bg_color(g_dashboard_nsh_output, lv_color_black(),
                                LV_PART_MAIN);
      lv_obj_set_style_text_color(g_dashboard_nsh_output,
                                  lv_color_hex(0x86efac), LV_PART_MAIN);
      lv_textarea_set_text(g_dashboard_nsh_output,
                           "Starting child NSH session...\n");
    }

  g_dashboard_nsh_input = lv_textarea_create(panel);
  if (g_dashboard_nsh_input != NULL)
    {
      lv_obj_set_size(g_dashboard_nsh_input, 950, 34);
      lv_obj_align(g_dashboard_nsh_input, LV_ALIGN_TOP_LEFT, 0, 155);
      lv_obj_set_style_bg_color(g_dashboard_nsh_input, lv_color_hex(0x1e293b),
                                LV_PART_MAIN);
      lv_obj_set_style_text_color(g_dashboard_nsh_input, lv_color_white(),
                                  LV_PART_MAIN);
      lv_textarea_set_one_line(g_dashboard_nsh_input, true);
      lv_textarea_set_placeholder_text(g_dashboard_nsh_input,
                                        "enter NSH command...");
      lv_obj_add_event_cb(g_dashboard_nsh_input,
                          lvgldemo_dashboard_terminal_input, LV_EVENT_FOCUSED,
                          NULL);
      lv_obj_add_event_cb(g_dashboard_nsh_input,
                          lvgldemo_dashboard_terminal_input, LV_EVENT_READY,
                          NULL);
    }

  g_dashboard_nsh_keyboard = lv_keyboard_create(screen);
  if (g_dashboard_nsh_keyboard != NULL)
    {
      lv_obj_set_size(g_dashboard_nsh_keyboard, 1024, 232);
      lv_obj_align(g_dashboard_nsh_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
      lv_keyboard_set_textarea(g_dashboard_nsh_keyboard,
                               g_dashboard_nsh_input);
      lv_obj_add_flag(g_dashboard_nsh_keyboard, LV_OBJ_FLAG_HIDDEN);
    }

  if (g_dashboard_nsh_output == NULL || g_dashboard_nsh_input == NULL ||
      g_dashboard_nsh_keyboard == NULL || lvgldemo_dashboard_start_nsh() < 0)
    {
      if (g_dashboard_nsh_output != NULL)
        {
          lv_textarea_add_text(g_dashboard_nsh_output,
                               "[NSH terminal start failed]\n");
        }
    }
}
#else
static void lvgldemo_dashboard_stop_nsh(void)
{
}

static void lvgldemo_dashboard_create_terminal(lv_obj_t *screen)
{
  lv_obj_t *label = lv_label_create(screen);

  if (label != NULL)
    {
      lv_label_set_text(label,
                        "NSH terminal unavailable: enable pipes, exec and keyboard font");
      lv_obj_set_style_text_color(label, lv_color_hex(0xfca5a5),
                                  LV_PART_MAIN);
      lv_obj_align(label, LV_ALIGN_TOP_LEFT, 28, 390);
    }
}
#endif

static void lvgldemo_dashboard_card(lv_obj_t *screen, const char *text,
                                    lv_coord_t x, lv_coord_t y,
                                    lv_color_t status_color)
{
  lv_obj_t *card;
  lv_obj_t *label;

  card = lv_obj_create(screen);
  if (card == NULL)
    {
      return;
    }

  lv_obj_set_size(card, 468, 62);
  lv_obj_align(card, LV_ALIGN_TOP_LEFT, x, y);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x172033), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(card, status_color, LV_PART_MAIN);
  lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
  lv_obj_set_style_radius(card, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_all(card, 12, LV_PART_MAIN);

  label = lv_label_create(card);
  if (label != NULL)
    {
      lv_label_set_text(label, text);
      lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
      lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);
    }
}

static void lvgldemo_dashboard_feedback(lv_event_t *event,
                                         const char *message)
{
  lv_obj_t *feedback;

  if (lv_event_get_code(event) != LV_EVENT_CLICKED)
    {
      return;
    }

  feedback = lv_event_get_user_data(event);
  if (feedback != NULL)
    {
      lv_label_set_text(feedback, message);
    }
}

static void lvgldemo_dashboard_refresh_event(lv_event_t *event)
{
  lvgldemo_dashboard_feedback(event,
                              "Status: dashboard refreshed (hardware untouched)");
}

static void lvgldemo_dashboard_info_event(lv_event_t *event)
{
  lvgldemo_dashboard_feedback(event,
                              "Status: static system information displayed");
}

static void lvgldemo_dashboard_clear_event(lv_event_t *event)
{
  lvgldemo_dashboard_feedback(event,
                              "Status: notice cleared (no hardware action)");
}

static void lvgldemo_dashboard_button(lv_obj_t *screen, const char *text,
                                      lv_coord_t x, lv_event_cb_t callback,
                                      lv_obj_t *feedback)
{
  lv_obj_t *button;
  lv_obj_t *label;

  button = lv_button_create(screen);
  if (button == NULL)
    {
      return;
    }

  lv_obj_set_size(button, 210, 42);
  lv_obj_align(button, LV_ALIGN_TOP_LEFT, x, 300);
  lv_obj_set_style_bg_color(button, lv_color_hex(0x24527a), LV_PART_MAIN);
  lv_obj_set_style_radius(button, 8, LV_PART_MAIN);
  lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, feedback);

  label = lv_label_create(button);
  if (label != NULL)
    {
      lvgldemo_apply_ui_font(label);
      lv_label_set_text(label, text);
      lv_obj_center(label);
    }
}

static lv_obj_t *lvgldemo_outbound_label(lv_obj_t *screen, const char *text,
                                         lv_coord_t x, lv_coord_t y,
                                         lv_color_t color)
{
  lv_obj_t *label = lv_label_create(screen);

  if (label != NULL)
    {
      lvgldemo_apply_ui_font(label);
      lv_label_set_text(label, text);
      lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
      lv_obj_align(label, LV_ALIGN_TOP_LEFT, x, y);
    }

  return label;
}

static void lvgldemo_outbound_button_style(lv_obj_t *screen, const char *text,
                                           lv_coord_t x, lv_coord_t y,
                                           lv_coord_t width,
                                           lv_event_cb_t callback,
                                           void *user_data, int primary)
{
  lv_obj_t *button;
  lv_obj_t *label;

  button = lv_button_create(screen);
  if (button == NULL)
    {
      return;
    }

  lv_obj_set_size(button, width, 44);
  lv_obj_align(button, LV_ALIGN_TOP_LEFT, x, y);
  lv_obj_set_style_bg_color(button, primary ? LVGLDEMO_UI_STEEL :
                            LVGLDEMO_UI_PAPER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(button, primary ? LVGLDEMO_UI_STEEL_DEEP :
                            LVGLDEMO_UI_PAPER_STRONG,
                            LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_border_color(button, primary ? LVGLDEMO_UI_STEEL_DEEP :
                                LVGLDEMO_UI_LINE, LV_PART_MAIN);
  lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(button, 5, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(button, 6, LV_PART_MAIN);
  lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, user_data);

  label = lv_label_create(button);
  if (label != NULL)
    {
      lvgldemo_apply_ui_font(label);
      lv_label_set_text(label, text);
      lv_obj_set_style_text_color(label, primary ? lv_color_white() :
                                  LVGLDEMO_UI_INK, LV_PART_MAIN);
      lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
      lv_obj_set_width(label, width - 20);
      if (width > 500)
        {
          lv_obj_align(label, LV_ALIGN_LEFT_MID, 10, 0);
        }
      else
        {
          lv_obj_center(label);
        }
    }
}

static void lvgldemo_outbound_button(lv_obj_t *screen, const char *text,
                                     lv_coord_t x, lv_coord_t y,
                                     lv_coord_t width, lv_event_cb_t callback,
                                     void *user_data)
{
  lvgldemo_outbound_button_style(screen, text, x, y, width, callback,
                                 user_data, 1);
}

static void lvgldemo_outbound_secondary_button(lv_obj_t *screen,
                                               const char *text,
                                               lv_coord_t x, lv_coord_t y,
                                               lv_coord_t width,
                                               lv_event_cb_t callback,
                                               void *user_data)
{
  lvgldemo_outbound_button_style(screen, text, x, y, width, callback,
                                 user_data, 0);
}

static void lvgldemo_outbound_prepare_screen(lv_obj_t *screen,
                                             const char *title,
                                             const char *subtitle)
{
  if (screen == NULL)
    {
      return;
    }

  lv_obj_set_style_bg_color(screen, LVGLDEMO_UI_CANVAS, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);

  {
    lv_obj_t *header = lv_obj_create(screen);

    if (header != NULL)
      {
        lv_obj_set_size(header, 1024, 56);
        lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_set_style_bg_color(header, LVGLDEMO_UI_PAPER_WARM,
                                  LV_PART_MAIN);
        lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_color(header, LVGLDEMO_UI_LINE,
                                      LV_PART_MAIN);
        lv_obj_set_style_border_width(header, 1, LV_PART_MAIN);
        lv_obj_set_style_radius(header, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
      }
  }

  lvgldemo_outbound_label(screen, "FUNCTION-EV  /  P4 CONTROL CONSOLE",
                          24, 8, LVGLDEMO_UI_STEEL_DEEP);
  lvgldemo_outbound_label(screen, "模拟模式 · 未写入真实硬件",
                          690, 8, LVGLDEMO_UI_AMBER);
  lvgldemo_outbound_label(screen, title, 24, 68, LVGLDEMO_UI_INK);
  lvgldemo_outbound_label(screen, subtitle, 24, 99, LVGLDEMO_UI_MUTED);
}

static void lvgldemo_outbound_back_home_event(lv_event_t *event)
{
  lv_obj_t *screen;

  if (lv_event_get_code(event) != LV_EVENT_CLICKED)
    {
      return;
    }

  screen = lv_screen_active();
  lvgldemo_dashboard_stop_nsh();
  if (screen != NULL)
    {
      lv_obj_clean(screen);
    }

  lvgldemo_outbound_font_unload();
  lvgldemo_dashboard_create();
}

static void lvgldemo_outbound_show_page(
    enum lvgldemo_outbound_page_e page)
{
  lv_obj_t *screen = lv_screen_active();

  g_outbound_page = page;
  if (screen != NULL)
    {
      lv_obj_clean(screen);
    }

  lvgldemo_outbound_create();
}

static void lvgldemo_outbound_home_event(lv_event_t *event)
{
  if (lv_event_get_code(event) == LV_EVENT_CLICKED)
    {
      g_outbound_scanned = 0;
      g_outbound_vehicle = -1;
      g_outbound_dispatch_notice = 0;
      g_outbound_task_state = 0;
      g_outbound_exception = 0;
      g_outbound_paused = 0;
      lvgldemo_outbound_show_page(LVGLDEMO_OUTBOUND_HOME);
    }
}

static void lvgldemo_outbound_scan_event(lv_event_t *event)
{
  if (lv_event_get_code(event) == LV_EVENT_CLICKED)
    {
      g_outbound_scanned = 1;
      lvgldemo_outbound_show_page(LVGLDEMO_OUTBOUND_SCAN);
    }
}

static void lvgldemo_outbound_confirm_event(lv_event_t *event)
{
  if (lv_event_get_code(event) != LV_EVENT_CLICKED)
    {
      return;
    }

  if (g_outbound_scanned == 0)
    {
      lv_obj_t *feedback = lv_event_get_user_data(event);
      if (feedback != NULL)
        {
          lv_label_set_text(feedback, "请先点击重新扫码识别订单");
        }
    }
  else
    {
      g_outbound_vehicle = -1;
      g_outbound_task_state = 0;
      g_outbound_dispatch_notice = 0;
      lvgldemo_outbound_show_page(LVGLDEMO_OUTBOUND_DISPATCH);
    }
}

static void lvgldemo_outbound_select_vehicle(int vehicle)
{
  g_outbound_vehicle = vehicle;
  g_outbound_dispatch_notice = 0;
  lvgldemo_outbound_show_page(LVGLDEMO_OUTBOUND_DISPATCH);
}

static void lvgldemo_outbound_vehicle_one_event(lv_event_t *event)
{
  if (lv_event_get_code(event) == LV_EVENT_CLICKED)
    {
      lvgldemo_outbound_select_vehicle(0);
    }
}

static void lvgldemo_outbound_vehicle_two_event(lv_event_t *event)
{
  if (lv_event_get_code(event) == LV_EVENT_CLICKED)
    {
      lvgldemo_outbound_select_vehicle(1);
    }
}

static void lvgldemo_outbound_vehicle_three_event(lv_event_t *event)
{
  if (lv_event_get_code(event) == LV_EVENT_CLICKED)
    {
      g_outbound_vehicle = -1;
      g_outbound_dispatch_notice = 2;
      lvgldemo_outbound_show_page(LVGLDEMO_OUTBOUND_DISPATCH);
    }
}

static void lvgldemo_outbound_dispatch_event(lv_event_t *event)
{
  if (lv_event_get_code(event) != LV_EVENT_CLICKED)
    {
      return;
    }

  if (g_outbound_vehicle < 0)
    {
      g_outbound_dispatch_notice = 1;
      lvgldemo_outbound_show_page(LVGLDEMO_OUTBOUND_DISPATCH);
    }
  else
    {
      g_outbound_task_state = 1;
      g_outbound_exception = 0;
      g_outbound_paused = 0;
      lvgldemo_outbound_show_page(LVGLDEMO_OUTBOUND_STATUS);
    }
}

static void lvgldemo_outbound_advance_event(lv_event_t *event)
{
  if (lv_event_get_code(event) != LV_EVENT_CLICKED)
    {
      return;
    }

  if (g_outbound_exception != 0 || g_outbound_paused != 0)
    {
      return;
    }

  if (g_outbound_task_state < 4)
    {
      g_outbound_task_state++;
    }

  lvgldemo_outbound_show_page(LVGLDEMO_OUTBOUND_STATUS);
}

static void lvgldemo_outbound_exception_event(lv_event_t *event)
{
  if (lv_event_get_code(event) == LV_EVENT_CLICKED)
    {
      g_outbound_exception = 1;
      g_outbound_exception_type = (g_outbound_exception_type + 1) % 3;
      lvgldemo_outbound_show_page(LVGLDEMO_OUTBOUND_STATUS);
    }
}

static void lvgldemo_outbound_pause_event(lv_event_t *event)
{
  if (lv_event_get_code(event) == LV_EVENT_CLICKED)
    {
      g_outbound_paused = 1;
      lvgldemo_outbound_show_page(LVGLDEMO_OUTBOUND_STATUS);
    }
}

static void lvgldemo_outbound_ack_event(lv_event_t *event)
{
  if (lv_event_get_code(event) == LV_EVENT_CLICKED)
    {
      g_outbound_exception = 0;
      g_outbound_paused = 0;
      lvgldemo_outbound_show_page(LVGLDEMO_OUTBOUND_STATUS);
    }
}

static void lvgldemo_outbound_panel(lv_obj_t *screen, const char *text,
                                    lv_coord_t x, lv_coord_t y,
                                    lv_coord_t width, lv_coord_t height,
                                    lv_color_t border)
{
  lv_obj_t *panel;
  lv_obj_t *label;

  panel = lv_obj_create(screen);
  if (panel == NULL)
    {
      return;
    }

  lv_obj_set_size(panel, width, height);
  lv_obj_align(panel, LV_ALIGN_TOP_LEFT, x, y);
  lv_obj_set_style_bg_color(panel, LVGLDEMO_UI_PAPER, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(panel, LVGLDEMO_UI_LINE, LV_PART_MAIN);
  lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(panel, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_all(panel, 14, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(panel, 0, LV_PART_MAIN);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

  {
    lv_obj_t *accent = lv_obj_create(screen);

    if (accent != NULL)
      {
        lv_obj_set_size(accent, 4, height - 8);
        lv_obj_align(accent, LV_ALIGN_TOP_LEFT, x + 2, y + 4);
        lv_obj_set_style_bg_color(accent, border, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(accent, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(accent, 2, LV_PART_MAIN);
      }
  }

  label = lv_label_create(panel);
  if (label != NULL)
    {
      lvgldemo_apply_ui_font(label);
      lv_label_set_text(label, text);
      lv_obj_set_style_text_color(label, LVGLDEMO_UI_INK_SOFT, LV_PART_MAIN);
      lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
      lv_obj_set_width(label, width - 34);
      lv_obj_set_style_text_line_space(label, 2, LV_PART_MAIN);
      lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);
    }
}

static void lvgldemo_outbound_create_home(lv_obj_t *screen)
{
  lvgldemo_outbound_prepare_screen(screen, "P4 出库工作台",
                                   "当前任务 · 订单扫码、核验与车辆派发");
  lvgldemo_outbound_secondary_button(screen, "返回主界面", 846, 10, 150,
                           lvgldemo_outbound_back_home_event, NULL);

  lvgldemo_outbound_panel(screen,
                          "当前任务\n订单出库工作台\n扫码核验 > 选择车辆 > 派发任务",
                          28, 140, 620, 118, LVGLDEMO_UI_STEEL);
  lvgldemo_outbound_panel(screen,
                          "运行状态\n模拟数据 / 未连接真实硬件\n摄像头状态: BLOCKED",
                          672, 140, 316, 118, LVGLDEMO_UI_AMBER);

  lvgldemo_outbound_panel(screen, "当前时间\n12:00:00 (模拟)",
                          28, 278, 230, 72, LVGLDEMO_UI_SAGE);
  lvgldemo_outbound_panel(screen, "网络状态\n— 未连接",
                          274, 278, 230, 72, LVGLDEMO_UI_AMBER);
  lvgldemo_outbound_panel(screen, "空闲车辆\n3 辆可用",
                          520, 278, 230, 72, LVGLDEMO_UI_GREEN);
  lvgldemo_outbound_panel(screen, "数据来源\n模拟订单",
                          766, 278, 222, 72, LVGLDEMO_UI_FAINT);

  lvgldemo_outbound_panel(screen,
                          "下一步\n扫描订单二维码，完成订单核验后派发车辆",
                          28, 370, 700, 92, LVGLDEMO_UI_STEEL);
  lvgldemo_outbound_button(screen, "扫码出库", 752, 394, 236,
                           lvgldemo_outbound_scan_event, NULL);
  lvgldemo_outbound_label(screen,
                          "第一阶段 · 仅模拟订单流程 · 不控制真实摄像头和车辆",
                          28, 488, LVGLDEMO_UI_MUTED);
}

static void lvgldemo_outbound_create_scan(lv_obj_t *screen)
{
  lv_obj_t *feedback;

  lvgldemo_outbound_prepare_screen(screen, "扫码与订单核验",
                                   "当前步骤 01 / 03 · 摄像头区域和订单数据均为模拟");
  lvgldemo_outbound_secondary_button(screen, "返回主界面", 846, 10, 150,
                           lvgldemo_outbound_back_home_event, NULL);

  if (g_outbound_scanned != 0)
    {
      lvgldemo_outbound_panel(screen,
                              "扫码结果 · 核验通过 (模拟)\n订单号: ORD-20260919-001\n目标站点: STATION-07",
                              28, 140, 620, 236, LVGLDEMO_UI_GREEN);
      lvgldemo_outbound_panel(screen,
                              "核验状态\nREADY\n订单信息完整",
                              672, 140, 316, 236, LVGLDEMO_UI_GREEN);
    }
  else
    {
      lvgldemo_outbound_panel(screen,
                              "摄像头扫码区域 (模拟)\n\n等待订单码...\n当前未访问真实摄像头",
                              28, 140, 620, 236, LVGLDEMO_UI_STEEL);
      lvgldemo_outbound_panel(screen,
                              "核验状态\n待扫码\n数据来源: 模拟",
                              672, 140, 316, 236, LVGLDEMO_UI_AMBER);
    }

  feedback = lvgldemo_outbound_label(screen,
                                     g_outbound_scanned != 0 ?
                                     "订单已识别，请确认后选择车辆" :
                                     "点击重新扫码模拟识别订单",
                                     28, 402, g_outbound_scanned != 0 ?
                                     LVGLDEMO_UI_GREEN : LVGLDEMO_UI_AMBER);
  lvgldemo_outbound_button(screen, "重新扫码", 28, 468, 210,
                           lvgldemo_outbound_scan_event, NULL);
  lvgldemo_outbound_button(screen, "确认订单", 270, 468, 210,
                           lvgldemo_outbound_confirm_event, feedback);
  lvgldemo_outbound_secondary_button(screen, "取消操作", 512, 468, 210,
                           lvgldemo_outbound_home_event, NULL);
}

static void lvgldemo_outbound_create_dispatch(lv_obj_t *screen)
{
  const char *notice;
  lv_color_t notice_color;
  const char *vehicle_one;
  const char *vehicle_two;

  lvgldemo_outbound_prepare_screen(screen, "选择车辆与派发任务",
                                   "当前步骤 02 / 03 · 选择在线车辆，再派发已核验订单");
  lvgldemo_outbound_secondary_button(screen, "返回主界面", 846, 10, 150,
                           lvgldemo_outbound_back_home_event, NULL);
  lvgldemo_outbound_panel(screen,
                          "订单号: ORD-20260919-001\n核验结果: 通过 · 目标站点: STATION-07",
                          28, 140, 968, 82, LVGLDEMO_UI_GREEN);
  lvgldemo_outbound_panel(screen,
                          "车辆编号                         连接状态 / 当前任务",
                          28, 240, 968, 42, LVGLDEMO_UI_FAINT);

  vehicle_one = g_outbound_vehicle == 0 ?
                "EV-001     在线 / 空闲     [已选择]" :
                "EV-001     在线 / 空闲     点击选择";
  vehicle_two = g_outbound_vehicle == 1 ?
                "EV-002     在线 / 空闲     [已选择]" :
                "EV-002     在线 / 空闲     点击选择";
  lvgldemo_outbound_secondary_button(screen, vehicle_one, 28, 292, 968,
                                     lvgldemo_outbound_vehicle_one_event,
                                     NULL);
  lvgldemo_outbound_secondary_button(screen, vehicle_two, 28, 348, 968,
                                     lvgldemo_outbound_vehicle_two_event,
                                     NULL);
  lvgldemo_outbound_secondary_button(screen,
                                     "EV-003     离线 / 不可用     点击查看原因",
                                     28, 404, 968,
                                     lvgldemo_outbound_vehicle_three_event,
                                     NULL);

  if (g_outbound_dispatch_notice == 2)
    {
      notice = "EV-003 当前离线，请选择在线车辆";
      notice_color = LVGLDEMO_UI_BRICK;
    }
  else if (g_outbound_dispatch_notice == 1)
    {
      notice = "请先选择一辆空闲车辆，再派发任务";
      notice_color = LVGLDEMO_UI_AMBER;
    }
  else if (g_outbound_vehicle == 0)
    {
      notice = "已选择 EV-001，可以派发任务";
      notice_color = LVGLDEMO_UI_GREEN;
    }
  else if (g_outbound_vehicle == 1)
    {
      notice = "已选择 EV-002，可以派发任务";
      notice_color = LVGLDEMO_UI_GREEN;
    }
  else
    {
      notice = "请选择在线车辆，然后派发任务";
      notice_color = LVGLDEMO_UI_AMBER;
    }

  lvgldemo_outbound_label(screen, notice, 28, 464, notice_color);
  lvgldemo_outbound_button(screen, "派发任务", 28, 510, 210,
                           lvgldemo_outbound_dispatch_event, NULL);
  lvgldemo_outbound_secondary_button(screen, "取消操作", 270, 510, 210,
                           lvgldemo_outbound_home_event, NULL);
}

static void lvgldemo_outbound_create_status(lv_obj_t *screen)
{
  lv_obj_t *progress;
  static const char * const states[] =
  {
    "待派发",
    "已出发",
    "配送中",
    "已到站",
    "任务完成"
  };
  static const char * const exceptions[] =
  {
    "异常: 车辆离线 (模拟)",
    "异常: 路线丢失 (模拟)",
    "异常: 检测到障碍物 (模拟)"
  };
  lvgldemo_outbound_prepare_screen(screen, "任务状态",
                                   "当前步骤 03 / 03 · 查看订单、车辆和配送进度");
  lvgldemo_outbound_secondary_button(screen, "返回主界面", 846, 10, 150,
                           lvgldemo_outbound_back_home_event, NULL);
  if (g_outbound_vehicle == 0)
    {
      lvgldemo_outbound_panel(screen,
                              "当前订单\nORD-20260919-001\n车辆 EV-001 · STATION-07",
                              28, 140, 460, 104, LVGLDEMO_UI_STEEL);
    }
  else
    {
      lvgldemo_outbound_panel(screen,
                              "当前订单\nORD-20260919-001\n车辆 EV-002 · STATION-07",
                              28, 140, 460, 104, LVGLDEMO_UI_STEEL);
    }
  lvgldemo_outbound_panel(screen,
                          "任务流程\n待派发 > 已出发 > 配送中\n已到站 > 任务完成",
                          512, 140, 476, 104, LVGLDEMO_UI_SAGE);

  progress = lv_bar_create(screen);
  if (progress != NULL)
    {
      lv_obj_set_size(progress, 968, 12);
      lv_obj_align(progress, LV_ALIGN_TOP_LEFT, 28, 264);
      lv_bar_set_range(progress, 0, 4);
      lv_bar_set_value(progress, g_outbound_task_state, LV_ANIM_OFF);
      lv_obj_set_style_bg_color(progress, LVGLDEMO_UI_LINE_SOFT, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(progress, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_radius(progress, 5, LV_PART_MAIN);
      lv_obj_set_style_bg_color(progress, LVGLDEMO_UI_GREEN,
                                LV_PART_INDICATOR);
      lv_obj_set_style_bg_opa(progress, LV_OPA_COVER, LV_PART_INDICATOR);
      lv_obj_set_style_radius(progress, 5, LV_PART_INDICATOR);
    }

  {
    static const lv_coord_t timeline_x[] = {28, 220, 412, 604, 796};
    int i;

    for (i = 0; i < 5; i++)
      {
        lvgldemo_outbound_label(screen, states[i], timeline_x[i], 286,
                                i <= g_outbound_task_state ?
                                LVGLDEMO_UI_GREEN : LVGLDEMO_UI_FAINT);
      }
  }

  if (g_outbound_exception != 0)
    {
      lvgldemo_outbound_panel(screen,
                              exceptions[g_outbound_exception_type],
                              28, 330, 968, 82, LVGLDEMO_UI_BRICK);
      lvgldemo_outbound_label(screen,
                              "任务已暂停 · 请确认异常后继续",
                              42, 368, LVGLDEMO_UI_BRICK);
    }
  else if (g_outbound_paused != 0)
    {
      lvgldemo_outbound_panel(screen, "任务已暂停 (模拟)",
                              28, 330, 968, 82, LVGLDEMO_UI_AMBER);
    }
  else
    {
      lvgldemo_outbound_panel(screen, "状态通知\n当前无异常 · 任务受控",
                              28, 330, 968, 82, LVGLDEMO_UI_GREEN);
    }

  lvgldemo_outbound_button(screen, "推进状态", 28, 462, 210,
                           lvgldemo_outbound_advance_event, NULL);
  lvgldemo_outbound_secondary_button(screen, "模拟异常", 270, 462, 210,
                           lvgldemo_outbound_exception_event, NULL);
  lvgldemo_outbound_secondary_button(screen, "暂停任务", 512, 462, 210,
                           lvgldemo_outbound_pause_event, NULL);
  lvgldemo_outbound_button(screen, "确认异常", 754, 462, 210,
                           lvgldemo_outbound_ack_event, NULL);
}

static void lvgldemo_outbound_create(void)
{
  lv_obj_t *screen = lv_screen_active();

  if (screen == NULL)
    {
      return;
    }

  switch (g_outbound_page)
    {
      case LVGLDEMO_OUTBOUND_SCAN:
        lvgldemo_outbound_create_scan(screen);
        break;

      case LVGLDEMO_OUTBOUND_DISPATCH:
        lvgldemo_outbound_create_dispatch(screen);
        break;

      case LVGLDEMO_OUTBOUND_STATUS:
        lvgldemo_outbound_create_status(screen);
        break;

      case LVGLDEMO_OUTBOUND_HOME:
      default:
        lvgldemo_outbound_create_home(screen);
        break;
    }
}

static void lvgldemo_dashboard_outbound_event(lv_event_t *event)
{
  lv_obj_t *screen;

  if (lv_event_get_code(event) != LV_EVENT_CLICKED)
    {
      return;
    }

  g_outbound_page = LVGLDEMO_OUTBOUND_HOME;
  g_outbound_scanned = 0;
  g_outbound_vehicle = -1;
  g_outbound_dispatch_notice = 0;
  g_outbound_task_state = 0;
  g_outbound_exception = 0;
  g_outbound_exception_type = 0;
  g_outbound_paused = 0;

  lvgldemo_dashboard_stop_nsh();
  screen = lv_screen_active();
  if (screen != NULL)
    {
      lv_obj_clean(screen);
    }

  lvgldemo_outbound_font_load();
  lvgldemo_outbound_create();
}

static void lvgldemo_dashboard_create(void)
{
  lv_obj_t *screen;
  lv_obj_t *title;
  lv_obj_t *subtitle;
  lv_obj_t *feedback;

  screen = lv_screen_active();
  if (screen == NULL)
    {
      return;
    }

  lv_obj_set_style_bg_color(screen, lv_color_hex(0x0b1220), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);

  title = lv_label_create(screen);
  if (title != NULL)
    {
      lv_label_set_text(title, "Function-EV  |  Device Dashboard");
      lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 28, 10);
    }

  subtitle = lv_label_create(screen);
  if (subtitle != NULL)
    {
      lv_label_set_text(subtitle,
                        "LVGL application layer  |  touch controls enabled");
      lv_obj_set_style_text_color(subtitle, lv_color_hex(0xaab7c8),
                                  LV_PART_MAIN);
      lv_obj_align(subtitle, LV_ALIGN_TOP_LEFT, 28, 36);
    }

  lvgldemo_dashboard_card(screen, "Display\n1024x600 RGB565  |  READY",
                          28, 68, lv_color_hex(0x40c878));
  lvgldemo_dashboard_card(screen, "Touch\nGT911 /dev/input0  |  READY",
                          520, 68, lv_color_hex(0x40c878));
  lvgldemo_dashboard_card(screen, "Memory\nPSRAM 32 MB  |  READY", 28, 136,
                          lv_color_hex(0x40c878));
  lvgldemo_dashboard_card(screen, "Storage\nSmartFS /data  |  READY", 520,
                          136, lv_color_hex(0x40c878));
  lvgldemo_dashboard_card(screen, "Camera\nSC2336 CSI  |  BLOCKED", 28, 204,
                          lv_color_hex(0xe05a63));

  feedback = lv_label_create(screen);
  if (feedback != NULL)
    {
      lv_label_set_text(feedback,
                        "Status: dashboard ready (application layer only)");
      lv_obj_set_style_text_color(feedback, lv_color_hex(0xffd166),
                                  LV_PART_MAIN);
      lv_obj_align(feedback, LV_ALIGN_TOP_LEFT, 28, 274);

      lvgldemo_dashboard_button(screen, "Refresh", 28,
                                lvgldemo_dashboard_refresh_event, feedback);
      lvgldemo_dashboard_button(screen, "System info", 270,
                                lvgldemo_dashboard_info_event, feedback);
      lvgldemo_dashboard_button(screen, "Clear notice", 512,
                                lvgldemo_dashboard_clear_event, feedback);
      lvgldemo_dashboard_button(screen, "P4 Outbound", 754,
                                lvgldemo_dashboard_outbound_event, feedback);
    }

  lv_obj_t *footer = lv_label_create(screen);
  if (footer != NULL)
    {
      lv_label_set_text(footer,
                        "Audio / camera / LED / backlight / RTC: not controlled");
      lv_obj_set_style_text_color(footer, lv_color_hex(0x77869a),
                                  LV_PART_MAIN);
      lv_obj_align(footer, LV_ALIGN_TOP_LEFT, 28, 578);
    }

  lvgldemo_dashboard_create_terminal(screen);
}

static void lvgldemo_clear_display(void)
{
  lv_obj_t *screen = lv_screen_active();

  lvgldemo_dashboard_stop_nsh();

  if (screen != NULL)
    {
      lv_obj_clean(screen);
      lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_invalidate(screen);
      lv_refr_now(NULL);
    }
}

#ifdef CONFIG_LV_USE_NUTTX_LIBUV
static void lv_nuttx_uv_loop(uv_loop_t *loop, lv_nuttx_result_t *result)
{
  lv_nuttx_uv_t uv_info;
  void *data;

  uv_loop_init(loop);

  lv_memset(&uv_info, 0, sizeof(uv_info));
  uv_info.loop = loop;
  uv_info.disp = result->disp;
  uv_info.indev = result->indev;
#ifdef CONFIG_UINPUT_TOUCH
  uv_info.uindev = result->utouch_indev;
#endif

#ifdef CONFIG_LV_USE_NUTTX_MOUSE
  uv_info.mouse_indev = result->mouse_indev;
#endif

  data = lv_nuttx_uv_init(&uv_info);
  uv_run(loop, UV_RUN_DEFAULT);
  lv_nuttx_uv_deinit(&data);
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: main or lv_demos_main
 *
 * Description:
 *
 * Input Parameters:
 *   Standard argc and argv
 *
 * Returned Value:
 *   Zero on success; a positive, non-zero value on failure.
 *
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  lv_nuttx_dsc_t info;
  lv_nuttx_result_t result;

#ifdef CONFIG_LV_USE_NUTTX_LIBUV
  uv_loop_t ui_loop;
  lv_memzero(&ui_loop, sizeof(ui_loop));
#endif

#ifndef CONFIG_DISABLE_SIGNALS
  g_lvgldemo_exit = 0;
#endif

  if (lv_is_initialized())
    {
      LV_LOG_ERROR("LVGL already initialized! aborting.");
      return -1;
    }

#ifdef NEED_BOARDINIT
  /* Perform board-specific driver initialization */

  boardctl(BOARDIOC_INIT, 0);

#endif

  lv_init();

#ifndef CONFIG_DISABLE_SIGNALS
  signal(SIGINT, lvgldemo_sigint);
#endif

  lv_nuttx_dsc_init(&info);

#ifdef CONFIG_LV_USE_NUTTX_LCD
  info.fb_path = "/dev/lcd0";
#endif

#ifdef CONFIG_INPUT_TOUCHSCREEN
  info.input_path = CONFIG_EXAMPLES_LVGLDEMO_INPUT_DEVPATH;
#endif

  lv_nuttx_init(&info, &result);

  if (result.disp == NULL)
    {
      LV_LOG_ERROR("lv_demos initialization failure!");
      return 1;
    }

  if (argc <= 1 || strcmp(argv[1], "dashboard") == 0)
    {
      lvgldemo_dashboard_create();
    }
  else if (!lv_demos_create(&argv[1], argc - 1))
    {
      lv_demos_show_help();

      /* we can add custom demos here */

      goto demo_end;
    }

#ifdef CONFIG_LV_USE_NUTTX_LIBUV
  lv_nuttx_uv_loop(&ui_loop, &result);
#else
  while (1)
    {
      uint32_t idle;

#ifndef CONFIG_DISABLE_SIGNALS
      if (g_lvgldemo_exit != 0)
        {
          break;
        }
#endif

      idle = lv_timer_handler();

      /* Minimum sleep of 1ms */

      idle = idle ? idle : 1;
      usleep(idle * 1000);
    }
#endif

demo_end:
  lvgldemo_clear_display();
  lvgldemo_outbound_font_unload();
  lv_nuttx_deinit(&result);
  lv_deinit();

  return 0;
}
