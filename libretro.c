#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "libretro.h"
#include "libretro_core_options.h"

#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"
#include "clownmdemu-frontend-common/clownmdemu/clownmdemu.h"

#define MIXER_IMPLEMENTATION
#define MIXER_FORMAT int16_t
#include "clownmdemu-frontend-common/mixer.h"

#define FRAMEBUFFER_WIDTH 320
#define FRAMEBUFFER_HEIGHT 480

#define SAMPLE_RATE_NO_LOWPASS 48000
#define SAMPLE_RATE_WITH_LOWPASS 22000 /* This sounds about right compared to my PAL Model 1 Mega Drive. */

/* Mixer data. */
static Mixer_Constant mixer_constant;
static Mixer_State mixer_state;
static const Mixer mixer = {&mixer_constant, &mixer_state};

/* clownmdemu data. */
static ClownMDEmu_Configuration clownmdemu_configuration;
static ClownMDEmu_Constant clownmdemu_constant;
static ClownMDEmu_State clownmdemu_state;
static ClownMDEmu_Callbacks clownmdemu_callbacks;
static const ClownMDEmu clownmdemu = CLOWNMDEMU_PARAMETERS_INITIALISE(&clownmdemu_configuration, &clownmdemu_constant, &clownmdemu_state, &clownmdemu_callbacks);

/* Frontend data. */
static union
{
	uint16_t u16[FRAMEBUFFER_HEIGHT][FRAMEBUFFER_WIDTH];
	uint32_t u32[FRAMEBUFFER_HEIGHT][FRAMEBUFFER_WIDTH];
} fallback_framebuffer;

static union
{
	uint16_t u16[16 * 4 * 3]; /* 16 colours, 4 palette lines, 3 brightnesses. */
	uint32_t u32[16 * 4 * 3];
} colours;

static void *current_framebuffer;
static size_t current_framebuffer_pitch;
static unsigned int current_screen_width;
static unsigned int current_screen_height;
static void (*scanline_rendered_callback)(void *user_data, const cc_u8l *source_pixels, void *destination_pixels, unsigned int screen_width);
static void (*fallback_colour_updated_callback)(void *user_data, cc_u16f index, cc_u16f colour);
static void (*fallback_scanline_rendered_callback)(void *user_data, const cc_u8l *source_pixels, void *destination_pixels, unsigned int screen_width);

static const unsigned char *rom;
static size_t rom_size;
static cc_bool pal_mode_enabled;
static cc_bool tall_interlace_mode_2;
static cc_bool lowpass_filter_enabled;

static struct
{
	retro_environment_t        environment;
	retro_video_refresh_t      video;
	retro_audio_sample_t       audio;
	retro_audio_sample_batch_t audio_batch;
	retro_input_poll_t         input_poll;
	retro_input_state_t        input_state;
	CC_ATTRIBUTE_PRINTF(2, 3) retro_log_printf_t log;
} libretro_callbacks;

static cc_u8f CartridgeReadCallback(void* const user_data, const cc_u32f address)
{
	(void)user_data;

	if (address >= rom_size)
		return 0;
	else
		return rom[address];
}

static void CartridgeWriteCallback(void* const user_data, const cc_u32f address, const cc_u8f value)
{
	(void)user_data;
	(void)address;
	(void)value;
}

static void ColourUpdatedCallback_0RGB1555(void* const user_data, const cc_u16f index, const cc_u16f colour)
{
	/* Convert from 0BGR4444 to 0RGB1555. */
	const unsigned int red   = (colour >> (4 * 0)) & 0xF;
	const unsigned int green = (colour >> (4 * 1)) & 0xF;
	const unsigned int blue  = (colour >> (4 * 2)) & 0xF;

	(void)user_data;

	colours.u16[index] = (((red   << 1) | (red   >> 3)) << (5 * 2))
	                   | (((green << 1) | (green >> 3)) << (5 * 1))
	                   | (((blue  << 1) | (blue  >> 3)) << (5 * 0));
}

static void ColourUpdatedCallback_RGB565(void* const user_data, const cc_u16f index, const cc_u16f colour)
{
	/* Convert from 0BGR4444 to RGB565. */
	const unsigned int red   = (colour >> (4 * 0)) & 0xF;
	const unsigned int green = (colour >> (4 * 1)) & 0xF;
	const unsigned int blue  = (colour >> (4 * 2)) & 0xF;

	(void)user_data;

	colours.u16[index] = (((red   << 1) | (red   >> 3)) << 11)
	                   | (((green << 2) | (green >> 2)) << 5)
	                   | (((blue  << 1) | (blue  >> 3)) << 0);
}

static void ColourUpdatedCallback_XRGB8888(void* const user_data, const cc_u16f index, const cc_u16f colour)
{
	/* Convert from 0BGR4444 to XRGB8888. */
	const unsigned int red   = (colour >> (4 * 0)) & 0xF;
	const unsigned int green = (colour >> (4 * 1)) & 0xF;
	const unsigned int blue  = (colour >> (4 * 2)) & 0xF;

	(void)user_data;

	colours.u32[index] = (((red   << 4) | (red   >> 0)) << (8 * 2))
	                   | (((green << 4) | (green >> 0)) << (8 * 1))
	                   | (((blue  << 4) | (blue  >> 0)) << (8 * 0));
}

static void ScanlineRenderedCallback_16Bit(void* const user_data, const cc_u8l* const source_pixels, void* const destination_pixels, const unsigned int screen_width)
{
	const cc_u8l *source_pixel_pointer = source_pixels;
	uint16_t *destination_pixel_pointer = (uint16_t*)destination_pixels;

	unsigned int i;

	(void)user_data;

	for (i = 0; i < screen_width; ++i)
		*destination_pixel_pointer++ = colours.u16[*source_pixel_pointer++];
}

static void ScanlineRenderedCallback_32Bit(void* const user_data, const cc_u8l* const source_pixels, void* const destination_pixels, const unsigned int screen_width)
{
	const cc_u8l *source_pixel_pointer = source_pixels;
	uint32_t *destination_pixel_pointer = (uint32_t*)destination_pixels;

	unsigned int i;

	(void)user_data;

	for (i = 0; i < screen_width; ++i)
		*destination_pixel_pointer++ = colours.u32[*source_pixel_pointer++];
}

static void ScanlineRenderedCallback(void* const user_data, const cc_u16f scanline, const cc_u8l* const pixels, const cc_u16f screen_width, const cc_u16f screen_height)
{
	/* At the start of the frame, update the screen width and height
	   and obtain a new framebuffer from the frontend. */
	if (scanline == 0)
	{
		struct retro_framebuffer frontend_framebuffer;

		frontend_framebuffer.width = screen_width;
		frontend_framebuffer.height = screen_height;
		frontend_framebuffer.access_flags = RETRO_MEMORY_ACCESS_WRITE;

		if (libretro_callbacks.environment(RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER, &frontend_framebuffer)
		&& (frontend_framebuffer.format == RETRO_PIXEL_FORMAT_0RGB1555
		 || frontend_framebuffer.format == RETRO_PIXEL_FORMAT_XRGB8888
		 || frontend_framebuffer.format == RETRO_PIXEL_FORMAT_RGB565))
		{
			current_framebuffer = frontend_framebuffer.data;
			current_framebuffer_pitch = frontend_framebuffer.pitch;

			/* Select the proper callbacks based on the framebuffer format. */
			switch (frontend_framebuffer.format)
			{
				default:
					/* Should never happen. */
					assert(cc_false);
					/* Fallthrough */
				case RETRO_PIXEL_FORMAT_0RGB1555:
					clownmdemu_callbacks.colour_updated = ColourUpdatedCallback_0RGB1555;
					scanline_rendered_callback = ScanlineRenderedCallback_16Bit;
					break;

				case RETRO_PIXEL_FORMAT_XRGB8888:
					clownmdemu_callbacks.colour_updated = ColourUpdatedCallback_XRGB8888;
					scanline_rendered_callback = ScanlineRenderedCallback_32Bit;
					break;

				case RETRO_PIXEL_FORMAT_RGB565:
					clownmdemu_callbacks.colour_updated = ColourUpdatedCallback_RGB565;
					scanline_rendered_callback = ScanlineRenderedCallback_16Bit;
					break;
			}
		}
		else
		{
			/* Fall back on the internal framebuffer if the frontend one could not be
			   obtained or was in an incompatible format. */
			if (fallback_scanline_rendered_callback == ScanlineRenderedCallback_16Bit)
			{
				current_framebuffer = fallback_framebuffer.u16;
				current_framebuffer_pitch = sizeof(fallback_framebuffer.u16[0]);
			}
			else
			{
				current_framebuffer = fallback_framebuffer.u32;
				current_framebuffer_pitch = sizeof(fallback_framebuffer.u32[0]);
			}

			clownmdemu_callbacks.colour_updated = fallback_colour_updated_callback;
			scanline_rendered_callback = fallback_scanline_rendered_callback;
		}

		current_screen_width = screen_width;
		current_screen_height = screen_height;
	}

	/* Prevent mid-frame resolution changes from causing out-of-bound framebuffer accesses. */
	if (scanline < current_screen_height)
		scanline_rendered_callback(user_data, pixels, (unsigned char*)current_framebuffer + (current_framebuffer_pitch * scanline), CC_MAX(screen_width, current_screen_width));
}

static cc_bool InputRequestedCallback(void* const user_data, const cc_u8f player_id, const ClownMDEmu_Button button_id)
{
	cc_u16f libretro_button_id;

	(void)user_data;

	switch (button_id)
	{
		default:
			/* Fallthrough */
		case CLOWNMDEMU_BUTTON_UP:
			libretro_button_id = RETRO_DEVICE_ID_JOYPAD_UP;
			break;

		case CLOWNMDEMU_BUTTON_DOWN:
			libretro_button_id = RETRO_DEVICE_ID_JOYPAD_DOWN;
			break;

		case CLOWNMDEMU_BUTTON_LEFT:
			libretro_button_id = RETRO_DEVICE_ID_JOYPAD_LEFT;
			break;

		case CLOWNMDEMU_BUTTON_RIGHT:
			libretro_button_id = RETRO_DEVICE_ID_JOYPAD_RIGHT;
			break;

		case CLOWNMDEMU_BUTTON_A:
			libretro_button_id = RETRO_DEVICE_ID_JOYPAD_Y;
			break;

		case CLOWNMDEMU_BUTTON_B:
			libretro_button_id = RETRO_DEVICE_ID_JOYPAD_B;
			break;

		case CLOWNMDEMU_BUTTON_C:
			libretro_button_id = RETRO_DEVICE_ID_JOYPAD_A;
			break;

		case CLOWNMDEMU_BUTTON_START:
			libretro_button_id = RETRO_DEVICE_ID_JOYPAD_START;
			break;
	}

	return libretro_callbacks.input_state(player_id, RETRO_DEVICE_JOYPAD, 0, libretro_button_id);
}

static void FMAudioToBeGeneratedCallback(void* const user_data, const ClownMDEmu* const clownmdemu, const size_t total_frames, void (* const generate_fm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, size_t total_frames))
{
	(void)user_data;

	generate_fm_audio(clownmdemu, Mixer_AllocateFMSamples(&mixer, total_frames), total_frames);
}

static void PSGAudioToBeGeneratedCallback(void* const user_data, const ClownMDEmu* const clownmdemu, const size_t total_samples, void (* const generate_psg_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, size_t total_samples))
{
	(void)user_data;

	generate_psg_audio(clownmdemu, Mixer_AllocatePSGSamples(&mixer, total_samples), total_samples);
}

static void PCMAudioToBeGeneratedCallback(void* const user_data, const ClownMDEmu* const clownmdemu, const size_t total_frames, void (* const generate_pcm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, size_t total_frames))
{
	(void)user_data;

	generate_pcm_audio(clownmdemu, Mixer_AllocatePCMSamples(&mixer, total_frames), total_frames);
}

static void CDDAAudioToBeGeneratedCallback(void* const user_data, const ClownMDEmu* const clownmdemu, const size_t total_frames, void (* const generate_cdda_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, size_t total_frames))
{
	(void)user_data;

	generate_cdda_audio(clownmdemu, Mixer_AllocateCDDASamples(&mixer, total_frames), total_frames);
}

static void CDSeekCallback(void* const user_data, const cc_u32f sector_index)
{
	(void)user_data;
	(void)sector_index;
}

static const cc_u8l* CDSectorReadCallback(void* const user_data)
{
	static cc_u8l sector_buffer[2048];

	(void)user_data;

	return sector_buffer;
}

CC_ATTRIBUTE_PRINTF(2, 3) static void FallbackErrorLogCallback(const enum retro_log_level level, const char* const format, ...)
{
	va_list args;

	switch (level)
	{
		case RETRO_LOG_DEBUG:
			fputs("RETRO_LOG_DEBUG: ", stderr);
			break;

		case RETRO_LOG_INFO:
			fputs("RETRO_LOG_INFO: ", stderr);
			break;

		case RETRO_LOG_WARN:
			fputs("RETRO_LOG_WARN: ", stderr);
			break;

		case RETRO_LOG_ERROR:
			fputs("RETRO_LOG_ERROR: ", stderr);
			break;

		case RETRO_LOG_DUMMY:
			break;
	}

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}

static void ClownMDEmuLog(void* const user_data, const char* const format, va_list arg)
{
	/* libretro lacks an error log callback that takes a va_list,
	   so we'll have to expand the message to a plain string here. */
	char message_buffer[0x100];

	(void)user_data;

	/* TODO: This unbounded printf is so gross... */
	vsprintf(message_buffer, format, arg);
	strcat(message_buffer, "\n");

	libretro_callbacks.log(RETRO_LOG_WARN, "%s", message_buffer);
}

static cc_bool DoOptionBoolean(const char* const key, const char* const true_value)
{
	struct retro_variable variable;
	variable.key = key;
	return libretro_callbacks.environment(RETRO_ENVIRONMENT_GET_VARIABLE, &variable) && variable.value != NULL && strcmp(variable.value, true_value) == 0;
}

static void UpdateOptions(const cc_bool only_update_flags)
{
	const cc_bool lowpass_filter_changed = lowpass_filter_enabled != DoOptionBoolean("clownmdemu_lowpass_filter", "enabled");
	const cc_bool pal_mode_changed = pal_mode_enabled != DoOptionBoolean("clownmdemu_tv_standard", "pal");

	lowpass_filter_enabled ^= lowpass_filter_changed;
	pal_mode_enabled ^= pal_mode_changed;

	if ((lowpass_filter_changed || pal_mode_changed) && !only_update_flags)
	{
		Mixer_State_Initialise(&mixer_state, lowpass_filter_enabled ? SAMPLE_RATE_WITH_LOWPASS : SAMPLE_RATE_NO_LOWPASS, pal_mode_enabled, cc_false);

		{
		struct retro_system_av_info info;
		retro_get_system_av_info(&info);
		libretro_callbacks.environment(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &info);
		}
	}

	tall_interlace_mode_2 = DoOptionBoolean("clownmdemu_tall_interlace_mode_2", "enabled");

	clownmdemu_configuration.general.region = DoOptionBoolean("clownmdemu_overseas_region", "elsewhere") ? CLOWNMDEMU_REGION_OVERSEAS : CLOWNMDEMU_REGION_DOMESTIC;
	clownmdemu_configuration.general.tv_standard = pal_mode_enabled ? CLOWNMDEMU_TV_STANDARD_PAL : CLOWNMDEMU_TV_STANDARD_NTSC;
	clownmdemu_configuration.vdp.sprites_disabled = DoOptionBoolean("clownmdemu_disable_sprite_plane", "enabled");
	clownmdemu_configuration.vdp.window_disabled = DoOptionBoolean("clownmdemu_disable_window_plane", "enabled");
	clownmdemu_configuration.vdp.planes_disabled[0] = DoOptionBoolean("clownmdemu_disable_plane_a", "enabled");
	clownmdemu_configuration.vdp.planes_disabled[1] = DoOptionBoolean("clownmdemu_disable_plane_b", "enabled");
	clownmdemu_configuration.fm.fm_channels_disabled[0] = DoOptionBoolean("clownmdemu_disable_fm1", "enabled");
	clownmdemu_configuration.fm.fm_channels_disabled[1] = DoOptionBoolean("clownmdemu_disable_fm2", "enabled");
	clownmdemu_configuration.fm.fm_channels_disabled[2] = DoOptionBoolean("clownmdemu_disable_fm3", "enabled");
	clownmdemu_configuration.fm.fm_channels_disabled[3] = DoOptionBoolean("clownmdemu_disable_fm4", "enabled");
	clownmdemu_configuration.fm.fm_channels_disabled[4] = DoOptionBoolean("clownmdemu_disable_fm5", "enabled");
	clownmdemu_configuration.fm.fm_channels_disabled[5] = DoOptionBoolean("clownmdemu_disable_fm6", "enabled");
	clownmdemu_configuration.fm.dac_channel_disabled = DoOptionBoolean("clownmdemu_disable_dac", "enabled");
	clownmdemu_configuration.fm.ladder_effect_disabled = !DoOptionBoolean("clownmdemu_ladder_effect", "enabled");
	clownmdemu_configuration.psg.tone_disabled[0] = DoOptionBoolean("clownmdemu_disable_psg1", "enabled");
	clownmdemu_configuration.psg.tone_disabled[1] = DoOptionBoolean("clownmdemu_disable_psg2", "enabled");
	clownmdemu_configuration.psg.tone_disabled[2] = DoOptionBoolean("clownmdemu_disable_psg3", "enabled");
	clownmdemu_configuration.psg.noise_disabled = DoOptionBoolean("clownmdemu_disable_psg_noise", "enabled");
	clownmdemu_configuration.pcm.channels_disabled[0] = DoOptionBoolean("clownmdemu_disable_pcm1", "enabled");
	clownmdemu_configuration.pcm.channels_disabled[1] = DoOptionBoolean("clownmdemu_disable_pcm2", "enabled");
	clownmdemu_configuration.pcm.channels_disabled[2] = DoOptionBoolean("clownmdemu_disable_pcm3", "enabled");
	clownmdemu_configuration.pcm.channels_disabled[3] = DoOptionBoolean("clownmdemu_disable_pcm4", "enabled");
	clownmdemu_configuration.pcm.channels_disabled[4] = DoOptionBoolean("clownmdemu_disable_pcm5", "enabled");
	clownmdemu_configuration.pcm.channels_disabled[5] = DoOptionBoolean("clownmdemu_disable_pcm6", "enabled");
	clownmdemu_configuration.pcm.channels_disabled[6] = DoOptionBoolean("clownmdemu_disable_pcm7", "enabled");
	clownmdemu_configuration.pcm.channels_disabled[7] = DoOptionBoolean("clownmdemu_disable_pcm8", "enabled");
}

void retro_init(void)
{
	/* Inform frontend of serialisation quirks. */
	{
	uint64_t serialisation_quirks = RETRO_SERIALIZATION_QUIRK_ENDIAN_DEPENDENT | RETRO_SERIALIZATION_QUIRK_PLATFORM_DEPENDENT;
	libretro_callbacks.environment(RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS, &serialisation_quirks);
	}

	/* Initialise clownmdemu. */
	clownmdemu_callbacks.user_data = NULL;
	clownmdemu_callbacks.cartridge_read = CartridgeReadCallback;
	clownmdemu_callbacks.cartridge_written = CartridgeWriteCallback;
	clownmdemu_callbacks.colour_updated = ColourUpdatedCallback_0RGB1555;
	clownmdemu_callbacks.scanline_rendered = ScanlineRenderedCallback;
	clownmdemu_callbacks.input_requested = InputRequestedCallback;
	clownmdemu_callbacks.fm_audio_to_be_generated = FMAudioToBeGeneratedCallback;
	clownmdemu_callbacks.psg_audio_to_be_generated = PSGAudioToBeGeneratedCallback;
	clownmdemu_callbacks.pcm_audio_to_be_generated = PCMAudioToBeGeneratedCallback;
	clownmdemu_callbacks.cdda_audio_to_be_generated = CDDAAudioToBeGeneratedCallback;
	clownmdemu_callbacks.cd_seeked = CDSeekCallback;
	clownmdemu_callbacks.cd_sector_read = CDSectorReadCallback;

	UpdateOptions(cc_true);

	ClownMDEmu_SetLogCallback(ClownMDEmuLog, NULL);

	clownmdemu_constant = ClownMDEmu_Constant_Initialise();
	ClownMDEmu_State_Initialise(&clownmdemu_state);

	/* Initialise the mixer. */
	Mixer_Constant_Initialise(&mixer_constant);
	Mixer_State_Initialise(&mixer_state, lowpass_filter_enabled ? SAMPLE_RATE_WITH_LOWPASS : SAMPLE_RATE_NO_LOWPASS, pal_mode_enabled, cc_false);
}

void retro_deinit(void)
{
}

unsigned int retro_api_version(void)
{
	return RETRO_API_VERSION;
}

void retro_set_controller_port_device(const unsigned int port, const unsigned int device)
{
	(void)port;
	(void)device;

	/* TODO */
	/*libretro_callbacks.log(RETRO_LOG_INFO, "Plugging device %u into port %u.\n", device, port);*/
}

void retro_get_system_info(struct retro_system_info* const info)
{
	info->library_name     = "clownmdemu";
	info->library_version  = "v0.9";
	info->need_fullpath    = false;
	info->valid_extensions = "bin|md|gen";
	info->block_extract    = false;
}

static void SetGeometry(struct retro_game_geometry* const geometry)
{
	geometry->base_width   = current_screen_width;
	geometry->base_height  = current_screen_height;
	geometry->max_width    = FRAMEBUFFER_WIDTH;
	geometry->max_height   = FRAMEBUFFER_HEIGHT;
	geometry->aspect_ratio = 320.0f / (float)current_screen_height;

	/* Squish the aspect ratio vertically when in Interlace Mode 2. */
	if (!tall_interlace_mode_2 && current_screen_height >= 448)
		geometry->aspect_ratio *= 2.0f;
}

void retro_get_system_av_info(struct retro_system_av_info* const info)
{
	enum retro_pixel_format pixel_format;

	/* Determine which pixel format to render as in the event that
	   'RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER' fails or produces a framebuffer
	   that is in a format that we don't support. */
	pixel_format = RETRO_PIXEL_FORMAT_RGB565;
	if (libretro_callbacks.environment(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &pixel_format))
	{
		fallback_colour_updated_callback = ColourUpdatedCallback_RGB565;
		fallback_scanline_rendered_callback = ScanlineRenderedCallback_16Bit;
	}
	else
	{
		pixel_format = RETRO_PIXEL_FORMAT_XRGB8888;
		if (libretro_callbacks.environment(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &pixel_format))
		{
			fallback_colour_updated_callback = ColourUpdatedCallback_XRGB8888;
			fallback_scanline_rendered_callback = ScanlineRenderedCallback_32Bit;
		}
		else
		{
			fallback_colour_updated_callback = ColourUpdatedCallback_0RGB1555;
			fallback_scanline_rendered_callback = ScanlineRenderedCallback_16Bit;
		}
	}

	/* Initialise these to avoid a division by 0 in SetGeometry. */
	current_screen_width = 320;
	current_screen_height = 224;

	/* Populate the 'retro_system_av_info' struct. */
	SetGeometry(&info->geometry);

	info->timing.fps = pal_mode_enabled ? CLOWNMDEMU_MULTIPLY_BY_PAL_FRAMERATE(1.0) : CLOWNMDEMU_MULTIPLY_BY_NTSC_FRAMERATE(1.0);	/* Standard PAL and NTSC framerates. */
	info->timing.sample_rate = lowpass_filter_enabled ? (double)SAMPLE_RATE_WITH_LOWPASS : (double)SAMPLE_RATE_NO_LOWPASS;
}

void retro_set_environment(const retro_environment_t environment_callback)
{
	libretro_callbacks.environment = environment_callback;

	/* Declare the options to the frontend. */
	libretro_set_core_options(libretro_callbacks.environment);

	/* Retrieve a log callback from the frontend. */
	{
	struct retro_log_callback logging;
	if (libretro_callbacks.environment(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging) && logging.log != NULL)
		libretro_callbacks.log = logging.log;
	else
		libretro_callbacks.log = FallbackErrorLogCallback;
	}

	/* TODO: Specialised controller types. */
	{
	/*static const struct retro_controller_description controllers[] = {
		{"Control Pad", RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0)},
	};

	static const struct retro_controller_info ports[] = {
		{controllers, CC_COUNT_OF(controllers)},
		{NULL, 0}
	};

	libretro_callbacks.environment(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);*/
	}

	/* Give the buttons proper names. */
	{
	static const struct retro_input_descriptor desc[] = {
		/* Player 1. */
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Up"    },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Down"  },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left"  },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "A"     },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "B"     },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "C"     },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
		/* Player 2. */
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Up"    },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Down"  },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left"  },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "A"     },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "B"     },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "C"     },
		{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
		/* End. */
		{ 0, 0, 0, 0, NULL }
	};

	libretro_callbacks.environment(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, (void*)&desc);
	}
}

void retro_set_audio_sample(const retro_audio_sample_t audio_callback)
{
	libretro_callbacks.audio = audio_callback;
}

void retro_set_audio_sample_batch(const retro_audio_sample_batch_t audio_batch_callback)
{
	libretro_callbacks.audio_batch = audio_batch_callback;
}

void retro_set_input_poll(const retro_input_poll_t input_poll_callback)
{
	libretro_callbacks.input_poll = input_poll_callback;
}

void retro_set_input_state(const retro_input_state_t input_state_callback)
{
	libretro_callbacks.input_state = input_state_callback;
}

void retro_set_video_refresh(const retro_video_refresh_t video_callback)
{
	libretro_callbacks.video = video_callback;
}

void retro_reset(void)
{
	ClownMDEmu_Reset(&clownmdemu, cc_false); /* TODO: CD support. */
}

static void MixerCompleteCallback(void* const user_data, const MIXER_FORMAT* const audio_samples, const size_t total_frames)
{
	(void)user_data;

	libretro_callbacks.audio_batch(audio_samples, total_frames);
}

void retro_run(void)
{
	bool options_updated;

	/* Refresh options if they've been updated. */
	if (libretro_callbacks.environment(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &options_updated) && options_updated)
		UpdateOptions(cc_false);

	/* Poll inputs. */
	libretro_callbacks.input_poll();

	Mixer_Begin(&mixer);

	ClownMDEmu_Iterate(&clownmdemu);

	Mixer_End(&mixer, 1, 1, MixerCompleteCallback, NULL);

	/* Update aspect ratio. */
	{
	struct retro_game_geometry geometry;
	SetGeometry(&geometry);
	libretro_callbacks.environment(RETRO_ENVIRONMENT_SET_GEOMETRY, &geometry);
	}

	/* Upload the completed frame to the frontend. */
	libretro_callbacks.video(current_framebuffer, current_screen_width, current_screen_height, current_framebuffer_pitch);
}

bool retro_load_game(const struct retro_game_info* const info)
{
	/* Initialise the ROM. */
	rom = (const unsigned char*)info->data;
	rom_size = info->size;

	/* Boot the emulated Mega Drive. */
	ClownMDEmu_Reset(&clownmdemu, cc_false); /* TODO: CD support. */

	return true;
}

void retro_unload_game(void)
{
	/* Nothing to do here... */
}

unsigned int retro_get_region(void)
{
	return pal_mode_enabled ? RETRO_REGION_PAL : RETRO_REGION_NTSC;
}

bool retro_load_game_special(const unsigned int type, const struct retro_game_info* const info, const size_t num)
{
	(void)type;
	(void)info;
	(void)num;

	/* We don't need anything special. */
	return false;
}

size_t retro_serialize_size(void)
{
	return sizeof(clownmdemu_state);
}

bool retro_serialize(void* const data, const size_t size)
{
	(void)size;

	memcpy(data, &clownmdemu_state, sizeof(clownmdemu_state));
	return true;
}

bool retro_unserialize(const void* const data, const size_t size)
{
	(void)size;

	memcpy(&clownmdemu_state, data, sizeof(clownmdemu_state));
	return true;
}

void* retro_get_memory_data(const unsigned int id)
{
	(void)id;

	return NULL;
}

size_t retro_get_memory_size(const unsigned int id)
{
	(void)id;

	return 0;
}

void retro_cheat_reset(void)
{
}

void retro_cheat_set(const unsigned int index, const bool enabled, const char* const code)
{
	(void)index;
	(void)enabled;
	(void)code;
}
