/****************************************************************************
 * apps/examples/lvgldemo/audio_beep.c
 *
 * Speaker beep placeholder implementation.
 *
 * The real playback path (ES8311 + I2S on the openvela side) is not
 * wired yet; once it lands, replace the body of board_audio_beep()
 * with an actual playback implementation. The UI only calls
 * board_audio_beep(), so the two sides stay decoupled.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#ifdef CONFIG_AUDIO
#  include <sys/ioctl.h>
#  include <fcntl.h>
#  include <nuttx/audio/audio.h>
#  include <nuttx/audio/pcm_decode.h>
#endif

#include "ui_main.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: board_audio_beep
 *
 * Description:
 *   Play a short beep / spoken prompt. The real implementation will be
 *   added once the openvela audio path is ready.
 ****************************************************************************/

void board_audio_beep(void)
{
#if defined(CONFIG_AUDIO) && defined(CONFIG_AUDIO_I2S)
  /* Placeholder: play a synthesized beep via /dev/audio/pcm0 later. */
#endif
}
