#ifndef LIBRETRO_INTERFACE_H
#define LIBRETRO_INTERFACE_H

#include "libretro.h"

#include "../common/core/libraries/clowncommon/clowncommon.h"

typedef struct LibretroCallbacks
{
	retro_environment_t        environment;
	retro_video_refresh_t      video;
	retro_audio_sample_t       audio;
	retro_audio_sample_batch_t audio_batch;
	retro_input_poll_t         input_poll;
	retro_input_state_t        input_state;
	CC_ATTRIBUTE_PRINTF(2, 3) retro_log_printf_t log;
} LibretroCallbacks;

extern LibretroCallbacks libretro_callbacks;

#endif /* LIBRETRO_INTERFACE_H */
