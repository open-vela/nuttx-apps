/****************************************************************************
 * apps/examples/campilot/ui_state_machine.h
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

#ifndef __APPS_EXAMPLES_CAMPILOT_UI_STATE_MACHINE_H
#define __APPS_EXAMPLES_CAMPILOT_UI_STATE_MACHINE_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include <semaphore.h>
#include <pthread.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Maximum length of the MiMo recognition text kept in the UI state. */

#define UI_RESULT_MAX 512

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* The three horizontally-arranged screens.  Left is the camera preview,
 * middle is the recognition result, right is the voice dialog.
 */

enum ui_screen_e
{
  SCREEN_CAMERA = 0,  /* Left: live camera preview */
  SCREEN_RESULT,      /* Middle: AI recognition result */
  SCREEN_VOICE,       /* Right: voice dialog */
  SCREEN_MAX
};

/* Events that drive screen transitions.  EVENT_TAP is a click (touch
 * down/up within 20px); swipes and taps come from the GT911 gesture
 * detector, while the capture/recognition/TTS events are posted by the
 * campilot main thread.
 */

enum ui_event_e
{
  EVENT_NONE = 0,
  EVENT_SWIPE_LEFT,
  EVENT_SWIPE_RIGHT,
  EVENT_TAP,
  EVENT_CAPTURE_DONE,
  EVENT_RECOGNITION_DONE,
  EVENT_TTS_START,
  EVENT_TTS_DONE
};

/* UI state, owned by the LVGL task. */

struct ui_state_s
{
  enum ui_screen_e screen;              /* Currently visible screen */
  bool capturing;                       /* True while a capture is in flight */
  char last_result[UI_RESULT_MAX];      /* Latest MiMo recognition text */
};

/* Bounded, thread-safe event queue.  Producers (campilot main thread, the
 * GT911 gesture detector) call ui_eq_post(); the LVGL task drains it with
 * ui_eq_wait()/ui_eq_trywait().
 */

#define UI_EVENT_QUEUE_DEPTH 8

struct ui_event_queue_s
{
  sem_t sem;                            /* Counting semaphore */
  pthread_mutex_t lock;                 /* Protects the ring buffer */
  enum ui_event_e ring[UI_EVENT_QUEUE_DEPTH];
  int head;                             /* Next slot to read */
  int tail;                             /* Next slot to write */
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: ui_sm_init
 *
 * Description:
 *   Reset the UI state to the camera screen.
 *
 ****************************************************************************/

void ui_sm_init(struct ui_state_s *st);

/****************************************************************************
 * Name: ui_sm_transition
 *
 * Description:
 *   Apply one event to the state machine and return the resulting screen.
 *   This is pure logic and has no side effects beyond updating st.
 *
 ****************************************************************************/

enum ui_screen_e ui_sm_transition(struct ui_state_s *st,
                                  enum ui_event_e ev);

/****************************************************************************
 * Name: ui_sm_detect_gesture
 *
 * Description:
 *   Map a touch down/up pair to an event.  A horizontal move of more than
 *   40px is a swipe (direction from the sign of dx); a move of less than
 *   20px in both axes is a tap.  Everything else maps to EVENT_NONE.
 *
 ****************************************************************************/

enum ui_event_e ui_sm_detect_gesture(int x0, int y0, int x1, int y1);

/****************************************************************************
 * Name: ui_eq_init
 *
 * Description:
 *   Initialize an event queue.
 *
 ****************************************************************************/

void ui_eq_init(struct ui_event_queue_s *q);

/****************************************************************************
 * Name: ui_eq_post
 *
 * Description:
 *   Append an event to the queue and wake a waiter.  Safe to call from any
 *   thread.
 *
 * Returned Value:
 *   Zero on success, -1 if the queue is full.
 *
 ****************************************************************************/

int ui_eq_post(struct ui_event_queue_s *q, enum ui_event_e ev);

/****************************************************************************
 * Name: ui_eq_trywait
 *
 * Description:
 *   Non-blocking dequeue.  Returns 0 and stores the event in *ev if one is
 *   pending, or -1 if the queue is empty.
 *
 ****************************************************************************/

int ui_eq_trywait(struct ui_event_queue_s *q, enum ui_event_e *ev);

#endif /* __APPS_EXAMPLES_CAMPILOT_UI_STATE_MACHINE_H */
