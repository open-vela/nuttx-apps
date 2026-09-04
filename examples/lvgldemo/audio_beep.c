/****************************************************************************
 * apps/examples/lvgldemo/audio_beep.c
 *
 * 扬声器提示音播放。
 *
 * 说明：当前为占位实现（播放逻辑未接入），扬声器链路（ES8311 + I2S）
 * 在 openvela 侧完成后，替换此文件为真实播放实现。UI 通过
 * board_audio_beep() 触发提示音，二者解耦。
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
 *   播放一句话/提示音。真实实现待接入 openvela 音频链路后补充。
 ****************************************************************************/

void board_audio_beep(void)
{
#if defined(CONFIG_AUDIO) && defined(CONFIG_AUDIO_I2S)
  /* 占位：后续实现通过 /dev/audio/pcm0 播放合成提示音。 */
#endif
}