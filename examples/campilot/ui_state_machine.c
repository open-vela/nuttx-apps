/****************************************************************************
 * apps/examples/campilot/ui_state_machine.c
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

#include <string.h>

#include "ui_state_machine.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Gesture thresholds in pixels. */

#define UI_SWIPE_DX     40   /* Horizontal move larger than this = swipe */
#define UI_TAP_DX       20   /* Move smaller than this (both axes) = tap */

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: ui_sm_init
 ****************************************************************************/

void ui_sm_init(struct ui_state_s *st)
{
  if (st == NULL)
    {
      return;
    }

  memset(st, 0, sizeof(*st));
  st->screen = SCREEN_CAMERA;
}

/****************************************************************************
 * Name: ui_sm_transition
 ****************************************************************************/

enum ui_screen_e ui_sm_transition(struct ui_state_s *st,
                                  enum ui_event_e ev)
{
  enum ui_screen_e next = st->screen;

  switch (st->screen)
    {
      case SCREEN_CAMERA:
        /* Left swipe -> next (RESULT), right swipe -> wrap to VOICE. */

        if (ev == EVENT_SWIPE_LEFT)
          {
            next = SCREEN_RESULT;
          }
        else if (ev == EVENT_SWIPE_RIGHT)
          {
            next = SCREEN_VOICE;
          }
        else if (ev == EVENT_TAP)
          {
            /* A tap triggers a capture but the screen stays on CAMERA
             * until the capture actually completes.
             */

            st->capturing = true;
          }
        else if (ev == EVENT_CAPTURE_DONE)
          {
            st->capturing = false;
            next = SCREEN_RESULT;
          }
        break;

      case SCREEN_RESULT:
        if (ev == EVENT_SWIPE_LEFT)
          {
            next = SCREEN_VOICE;
          }
        else if (ev == EVENT_SWIPE_RIGHT)
          {
            next = SCREEN_CAMERA;
          }
        else if (ev == EVENT_TAP)
          {
            /* Tap on "capture again": go back to camera and re-capture. */

            st->capturing = true;
            next = SCREEN_CAMERA;
          }
        break;

      case SCREEN_VOICE:
        if (ev == EVENT_SWIPE_LEFT)
          {
            next = SCREEN_CAMERA;
          }
        else if (ev == EVENT_SWIPE_RIGHT)
          {
            next = SCREEN_RESULT;
          }
        else if (ev == EVENT_TTS_START)
          {
            /* Voice UI reserved for M5 ASR/TTS. */
          }
        break;

      default:
        break;
    }

  st->screen = next;
  return next;
}

/****************************************************************************
 * Name: ui_sm_detect_gesture
 ****************************************************************************/

enum ui_event_e ui_sm_detect_gesture(int x0, int y0, int x1, int y1)
{
  int dx  = x1 - x0;
  int dy  = y1 - y0;
  int adx = (dx < 0) ? -dx : dx;
  int ady = (dy < 0) ? -dy : dy;

  /* A horizontal move larger than the swipe threshold is a swipe, provided
   * it is predominantly horizontal (|dx| > |dy|).
   */

  if (adx > UI_SWIPE_DX && adx > ady)
    {
      return (dx < 0) ? EVENT_SWIPE_LEFT : EVENT_SWIPE_RIGHT;
    }

  /* A small move in both axes is a tap. */

  if (adx < UI_TAP_DX && ady < UI_TAP_DX)
    {
      return EVENT_TAP;
    }

  return EVENT_NONE;
}

/****************************************************************************
 * Name: ui_eq_init
 ****************************************************************************/

void ui_eq_init(struct ui_event_queue_s *q)
{
  sem_init(&q->sem, 0, 0);
  pthread_mutex_init(&q->lock, NULL);
  q->head = 0;
  q->tail = 0;
}

/****************************************************************************
 * Name: ui_eq_post
 ****************************************************************************/

int ui_eq_post(struct ui_event_queue_s *q, enum ui_event_e ev)
{
  int next;

  if (q == NULL || ev == EVENT_NONE)
    {
      return -1;
    }

  pthread_mutex_lock(&q->lock);

  next = (q->tail + 1) % UI_EVENT_QUEUE_DEPTH;
  if (next == q->head)
    {
      /* Queue full: drop the event. */

      pthread_mutex_unlock(&q->lock);
      return -1;
    }

  q->ring[q->tail] = ev;
  q->tail = next;

  pthread_mutex_unlock(&q->lock);
  sem_post(&q->sem);

  return 0;
}

/****************************************************************************
 * Name: ui_eq_trywait
 ****************************************************************************/

int ui_eq_trywait(struct ui_event_queue_s *q, enum ui_event_e *ev)
{
  if (sem_trywait(&q->sem) < 0)
    {
      return -1;
    }

  pthread_mutex_lock(&q->lock);

  *ev = q->ring[q->head];
  q->head = (q->head + 1) % UI_EVENT_QUEUE_DEPTH;

  pthread_mutex_unlock(&q->lock);

  return 0;
}
