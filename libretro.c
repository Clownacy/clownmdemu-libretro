#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* For file IO stuff. */
#include <stdlib.h>

#include "libretro.h"
#include "libretro_core_options.h"
#include "retro_endianness.h"

#include "common/cd-reader.h"
#include "common/core/clownmdemu.h"

#define MIXER_IMPLEMENTATION
#include "common/mixer.h"

#define FRAMEBUFFER_WIDTH VDP_MAX_SCANLINE_WIDTH
#define FRAMEBUFFER_HEIGHT VDP_MAX_SCANLINES

#define CARTRIDGE_FILE_EXTENSIONS "bin|md|gen"
#define CD_FILE_EXTENSIONS "cue|iso|chd"

/* Mixer data. */
static Mixer_State mixer;

/* ClownMDEmu data. */
static ClownMDEmu_Callbacks clownmdemu_callbacks;
static ClownMDEmu clownmdemu;

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
static void (*scanline_rendered_callback)(void *user_data, const cc_u8l *source_pixels, void *destination_pixels, cc_u16f left_boundary, cc_u16f right_boundary);
static void (*fallback_colour_updated_callback)(void *user_data, cc_u16f index, cc_u16f colour);
static void (*fallback_scanline_rendered_callback)(void *user_data, const cc_u8l *source_pixels, void *destination_pixels, cc_u16f left_boundary, cc_u16f right_boundary);

static cc_u16l *rom;
static size_t rom_length;

static CDReader_State cd_reader;

static cc_bool pal_mode_enabled;

static struct retro_vfs_file_handle *buram_file_handle;

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

/************/
/* Geometry */
/************/

static struct
{
	unsigned int current_screen_width;
	unsigned int current_screen_height;
	cc_bool tall_interlace_mode_2;
	cc_bool update_pending;
} geometry;

static void Geometry_Export(struct retro_game_geometry* const output)
{
	output->base_width   = geometry.current_screen_width;
	output->base_height  = geometry.current_screen_height;
	output->max_width    = FRAMEBUFFER_WIDTH;
	output->max_height   = FRAMEBUFFER_HEIGHT;
	output->aspect_ratio = ((VDP_H40_SCREEN_WIDTH_IN_TILES + clownmdemu.vdp.configuration.widescreen_tiles * 2) * VDP_TILE_WIDTH) / (float)geometry.current_screen_height;

	/* Squish the aspect ratio vertically when in Interlace Mode 2. */
	if (!geometry.tall_interlace_mode_2 && geometry.current_screen_height >= VDP_V28_SCANLINES_IN_TILES * VDP_INTERLACE_MODE_2_TILE_HEIGHT)
		output->aspect_ratio *= 2.0f;
}

static void Geometry_Update(void)
{
	if (geometry.update_pending)
	{
		geometry.update_pending = cc_false;

		{
			struct retro_game_geometry geometry;
			Geometry_Export(&geometry);
			libretro_callbacks.environment(RETRO_ENVIRONMENT_SET_GEOMETRY, (void*)&geometry);
		}
	}
}

static void Geometry_SetScreenSize(const unsigned int width, const unsigned int height)
{
	if (geometry.current_screen_width == width && geometry.current_screen_height == height)
		return;

	geometry.current_screen_width = width;
	geometry.current_screen_height = height;

	geometry.update_pending = cc_true;
}

static void Geometry_SetTallInterlaceMode2(const cc_bool tall_interlace_mode_2)
{
	if (geometry.tall_interlace_mode_2 == tall_interlace_mode_2)
		return;

	geometry.tall_interlace_mode_2 = tall_interlace_mode_2;

	geometry.update_pending = cc_true;
}

/***********/
/* File IO */
/***********/

static retro_vfs_open_t File_Open;
static retro_vfs_close_t File_Close;
static retro_vfs_size_t File_GetSize;
static retro_vfs_tell_t File_Tell;
static retro_vfs_seek_t File_Seek;
static retro_vfs_read_t File_Read;
static retro_vfs_write_t File_Write;
static retro_vfs_remove_t File_Remove;

static struct retro_vfs_file_handle* RETRO_CALLCONV File_OpenDefault(const char* const path, const unsigned int mode, const unsigned int hints)
{
	const char* mode_standard;

	(void)hints;

	switch (mode)
	{
		case RETRO_VFS_FILE_ACCESS_READ:
		case RETRO_VFS_FILE_ACCESS_READ | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING:
			mode_standard = "rb";
			break;

		case RETRO_VFS_FILE_ACCESS_WRITE:
			mode_standard = "wb";
			break;

		case RETRO_VFS_FILE_ACCESS_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING:
		case RETRO_VFS_FILE_ACCESS_READ | RETRO_VFS_FILE_ACCESS_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING:
			mode_standard = "r+b";
			break;

		case RETRO_VFS_FILE_ACCESS_READ | RETRO_VFS_FILE_ACCESS_WRITE:
			mode_standard = "w+b";
			break;

		default:
			return NULL;
	}

	return (struct retro_vfs_file_handle*)fopen(path, mode_standard);
}

static int RETRO_CALLCONV File_CloseDefault(struct retro_vfs_file_handle* const stream)
{
	if (stream == NULL)
		return -1;

	return fclose((FILE*)stream) == 0 ? 0 : -1;
}


static int64_t RETRO_CALLCONV File_GetSizeDefault(struct retro_vfs_file_handle* const stream)
{
	FILE* const file = (FILE*)stream;
	fpos_t position;
	int64_t result = -1;

	if (fgetpos(file, &position) == 0)
	{
		if (fseek(file, 0, SEEK_END) == 0)
			result = ftell(file);

		if (fsetpos(file, &position) != 0)
			result = -1;
	}

	return result;
}

static int64_t RETRO_CALLCONV File_TellDefault(struct retro_vfs_file_handle* const stream)
{
	return ftell((FILE*)stream);
}

static int64_t RETRO_CALLCONV File_SeekDefault(struct retro_vfs_file_handle* const stream, const int64_t offset, const int seek_position)
{
	int whence;

	if (offset < LONG_MIN || offset > LONG_MAX)
		return -1;

	switch (seek_position)
	{
		case RETRO_VFS_SEEK_POSITION_START:
			whence = SEEK_SET;
			break;

		case RETRO_VFS_SEEK_POSITION_CURRENT:
			whence = SEEK_CUR;
			break;

		case RETRO_VFS_SEEK_POSITION_END:
			whence = SEEK_END;
			break;

		default:
			return -1;
	}

	if (fseek((FILE*)stream, offset, whence) != 0)
		return -1;

	return File_TellDefault(stream);
}

static int64_t RETRO_CALLCONV File_ReadDefault(struct retro_vfs_file_handle* const stream, void* const s, const uint64_t len)
{
	if (len > (size_t)-1)
		return -1;

	return fread(s, 1, len, (FILE*)stream);
}

static int64_t RETRO_CALLCONV File_WriteDefault(struct retro_vfs_file_handle* const stream, const void* const s, const uint64_t len)
{
	if (len > (size_t)-1)
		return -1;

	return fwrite(s, 1, len, (FILE*)stream);
}

static int RETRO_CALLCONV File_RemoveDefault(const char* const path)
{
	return remove(path);
}

static void LoadFileIOCallbacks(void)
{
	struct retro_vfs_interface_info info;

	info.required_interface_version = 1;

	if (libretro_callbacks.environment(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, (void*)&info))
	{
		File_Open    = info.iface->open;
		File_Close   = info.iface->close;
		File_GetSize = info.iface->size;
		File_Tell    = info.iface->tell;
		File_Seek    = info.iface->seek;
		File_Read    = info.iface->read;
		File_Write   = info.iface->write;
		File_Remove  = info.iface->remove;
	}
	else
	{
		File_Open    = File_OpenDefault;
		File_Close   = File_CloseDefault;
		File_GetSize = File_GetSizeDefault;
		File_Tell    = File_TellDefault;
		File_Seek    = File_SeekDefault;
		File_Read    = File_ReadDefault;
		File_Write   = File_WriteDefault;
		File_Remove  = File_RemoveDefault;
	}
}

static bool LoadFileToBuffer(const char* const path, unsigned char** const output_file_buffer, size_t* const output_file_size)
{
	bool success = false;
	struct retro_vfs_file_handle* const file = File_Open(path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);

	if (file != NULL)
	{
		const int64_t file_size = File_GetSize(file);

		if (file_size >= 0)
		{
			unsigned char *file_buffer = (unsigned char*)malloc((size_t)file_size);

			if (file_buffer != NULL)
			{
				if (File_Seek(file, 0, RETRO_VFS_SEEK_POSITION_START) == 0)
				{
					if (File_Read(file, file_buffer, file_size) == file_size)
					{
						*output_file_buffer = file_buffer;
						*output_file_size = file_size;
						file_buffer = NULL;

						success = true;
					}
				}

				free(file_buffer);
			}
		}

		File_Close(file);
	}

	return success;
}

static bool CreateROMBuffer(const unsigned char* const input_buffer, const size_t input_buffer_length, cc_u16l** const output_buffer, size_t* const output_buffer_length)
{
	const size_t buffer_length = input_buffer_length / 2;

	cc_u16l* const buffer = (cc_u16l*)malloc(buffer_length * sizeof(cc_u16l));

	if (buffer == NULL)
	{
		return false;
	}
	else
	{
		size_t i;

		for (i = 0; i < buffer_length; ++i)
			buffer[i] = input_buffer[i * 2 + 0] << 8 | input_buffer[i * 2 + 1] << 0;

		*output_buffer = buffer;
		*output_buffer_length = buffer_length;

		return true;
	}
}

/************************/
/* ClownMDEmu Callbacks */
/************************/

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

static void ScanlineRenderedCallback_16Bit(void* const user_data, const cc_u8l* const source_pixels, void* const destination_pixels, const cc_u16f left_boundary, const cc_u16f right_boundary)
{
	const cc_u8l *source_pixel_pointer = source_pixels + left_boundary;
	uint16_t *destination_pixel_pointer = (uint16_t*)destination_pixels + left_boundary;

	unsigned int i;

	(void)user_data;

	for (i = left_boundary; i < right_boundary; ++i)
		*destination_pixel_pointer++ = colours.u16[*source_pixel_pointer++];
}

static void ScanlineRenderedCallback_32Bit(void* const user_data, const cc_u8l* const source_pixels, void* const destination_pixels, const cc_u16f left_boundary, const cc_u16f right_boundary)
{
	const cc_u8l *source_pixel_pointer = source_pixels + left_boundary;
	uint32_t *destination_pixel_pointer = (uint32_t*)destination_pixels + left_boundary;

	unsigned int i;

	(void)user_data;

	for (i = left_boundary; i < right_boundary; ++i)
		*destination_pixel_pointer++ = colours.u32[*source_pixel_pointer++];
}

static void ScanlineRenderedCallback(void* const user_data, const cc_u16f scanline, const cc_u8l* const pixels, const cc_u16f left_boundary, const cc_u16f right_boundary, const cc_u16f screen_width, const cc_u16f screen_height)
{
	/* At the start of the frame, update the screen width and height
	   and obtain a new framebuffer from the frontend. */
	if (scanline == 0)
	{
		struct retro_framebuffer frontend_framebuffer;

		frontend_framebuffer.width = screen_width;
		frontend_framebuffer.height = screen_height;
		frontend_framebuffer.access_flags = RETRO_MEMORY_ACCESS_WRITE;

		if (libretro_callbacks.environment(RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER, (void*)&frontend_framebuffer)
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

		Geometry_SetScreenSize(screen_width, screen_height);
	}

	/* Prevent mid-frame resolution changes from causing out-of-bound framebuffer accesses. */
	if (scanline < geometry.current_screen_height)
		scanline_rendered_callback(user_data, pixels, (unsigned char*)current_framebuffer + (current_framebuffer_pitch * scanline), left_boundary, right_boundary);
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

		case CLOWNMDEMU_BUTTON_X:
			libretro_button_id = RETRO_DEVICE_ID_JOYPAD_L;
			break;

		case CLOWNMDEMU_BUTTON_Y:
			libretro_button_id = RETRO_DEVICE_ID_JOYPAD_X;
			break;

		case CLOWNMDEMU_BUTTON_Z:
			libretro_button_id = RETRO_DEVICE_ID_JOYPAD_R;
			break;

		case CLOWNMDEMU_BUTTON_START:
			libretro_button_id = RETRO_DEVICE_ID_JOYPAD_START;
			break;

		case CLOWNMDEMU_BUTTON_MODE:
			libretro_button_id = RETRO_DEVICE_ID_JOYPAD_SELECT;
			break;
	}

	return libretro_callbacks.input_state(player_id, RETRO_DEVICE_JOYPAD, 0, libretro_button_id);
}

static void FMAudioToBeGeneratedCallback(void* const user_data, ClownMDEmu* const clownmdemu, const size_t total_frames, void (* const generate_fm_audio)(ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, size_t total_frames))
{
	(void)user_data;

	generate_fm_audio(clownmdemu, Mixer_AllocateFMSamples(&mixer, total_frames), total_frames);
}

static void PSGAudioToBeGeneratedCallback(void* const user_data, ClownMDEmu* const clownmdemu, const size_t total_samples, void (* const generate_psg_audio)(ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, size_t total_samples))
{
	(void)user_data;

	generate_psg_audio(clownmdemu, Mixer_AllocatePSGSamples(&mixer, total_samples), total_samples);
}

static void PCMAudioToBeGeneratedCallback(void* const user_data, ClownMDEmu* const clownmdemu, const size_t total_frames, void (* const generate_pcm_audio)(ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, size_t total_frames))
{
	(void)user_data;

	generate_pcm_audio(clownmdemu, Mixer_AllocatePCMSamples(&mixer, total_frames), total_frames);
}

static void CDDAAudioToBeGeneratedCallback(void* const user_data, ClownMDEmu* const clownmdemu, const size_t total_frames, void (* const generate_cdda_audio)(ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, size_t total_frames))
{
	(void)user_data;

	generate_cdda_audio(clownmdemu, Mixer_AllocateCDDASamples(&mixer, total_frames), total_frames);
}

static void CDSeekCallback(void* const user_data, const cc_u32f sector_index)
{
	(void)user_data;

	CDReader_SeekToSector(&cd_reader, sector_index);
}

static void CDSectorReadCallback(void* const user_data, cc_u16l* const buffer)
{
	(void)user_data;

	CDReader_ReadSector(&cd_reader, buffer);
}

static cc_bool CDSeekTrackCallback(void* const user_data, const cc_u16f track_index, const ClownMDEmu_CDDAMode mode)
{
	CDReader_PlaybackSetting playback_setting;

	(void)user_data;

	switch (mode)
	{
		default:
			assert(cc_false);
			return cc_false;

		case CLOWNMDEMU_CDDA_PLAY_ALL:
			playback_setting = CDREADER_PLAYBACK_ALL;
			break;

		case CLOWNMDEMU_CDDA_PLAY_ONCE:
			playback_setting = CDREADER_PLAYBACK_ONCE;
			break;

		case CLOWNMDEMU_CDDA_PLAY_REPEAT:
			playback_setting = CDREADER_PLAYBACK_REPEAT;
			break;
	}

	return CDReader_PlayAudio(&cd_reader, track_index, playback_setting);
}

static size_t CDAudioReadCallback(void* const user_data, cc_s16l* const sample_buffer, const size_t total_frames)
{
	(void)user_data;

	return CDReader_ReadAudio(&cd_reader, sample_buffer, total_frames);
}

static const char* GetBuRAMDirectory(void)
{
	const char *path;

	if (libretro_callbacks.environment(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, (void*)&path))
		return path;

	if (libretro_callbacks.environment(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, (void*)&path))
		return path;

	return "";
}

static char* GetBuRAMPath(const char* const filename)
{
	const char* const directory = GetBuRAMDirectory();
	const size_t directory_length = strlen(directory);
	const size_t filename_length = strlen(filename);
	char* const buffer = (char*)malloc(directory_length + 1 + filename_length + 1);

	if (buffer != NULL)
	{
		memcpy(&buffer[0], directory, directory_length);
		buffer[directory_length] = '/';
		memcpy(&buffer[directory_length + 1], filename, filename_length);
		buffer[directory_length + 1 + filename_length] = '\0';
	}

	return buffer;
}

static cc_bool SaveFileOpened(void* const user_data, const char* const filename, const bool read_or_write)
{
	cc_bool success = cc_false;

	char* const path = GetBuRAMPath(filename);

	(void)user_data;

	if (path != NULL)
	{
		buram_file_handle = File_Open(path, read_or_write ? RETRO_VFS_FILE_ACCESS_WRITE : RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
		success = buram_file_handle != NULL;

		free(path);
	}

	return success;
}

static cc_bool SaveFileOpenedForReadingCallback(void* const user_data, const char* const filename)
{
	return SaveFileOpened(user_data, filename, false);
}

static cc_s16f SaveFileReadCallback(void* const user_data)
{
	uint8_t byte;

	(void)user_data;

	if (File_Read(buram_file_handle, &byte, 1) == 0)
		return -1;

	return byte;
}

static cc_bool SaveFileOpenedForWritingCallback(void* const user_data, const char* const filename)
{
	return SaveFileOpened(user_data, filename, true);
}

static void SaveFileWrittenCallback(void* const user_data, const cc_u8f byte)
{
	const uint8_t value = byte;

	(void)user_data;

	File_Write(buram_file_handle, &value, 1);
}

static void SaveFileClosedCallback(void* const user_data)
{
	(void)user_data;

	File_Close(buram_file_handle);
}

static cc_bool SaveFileRemovedCallback(void* const user_data, const char* const filename)
{
	cc_bool success = cc_false;

	char* const path = GetBuRAMPath(filename);

	(void)user_data;

	if (path != NULL)
	{
		success = File_Remove(path) == 0;

		free(path);
	}

	return success;
}

static cc_bool SaveFileSizeObtainedCallback(void* const user_data, const char* const filename, size_t* const size)
{
	cc_bool success = cc_false;

	char* const path = GetBuRAMPath(filename);

	(void)user_data;

	if (path != NULL)
	{
		struct retro_vfs_file_handle* const file = File_Open(path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);

		if (file != NULL)
		{
			*size = File_GetSize(file);
			success = cc_true;

			File_Close(file);
		}

		free(path);
	}


	return success;
}

/***********/
/* Logging */
/***********/

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

static void ClownCDLog(void* const user_data, const char* const message)
{
	(void)user_data;

	libretro_callbacks.log(RETRO_LOG_WARN, "ClownCD: %s", message);
}

static void ClownMDEmuLog(void* const user_data, const char* const format, va_list arg)
{
	/* libretro lacks an error log callback that takes a va_list,
	   so we'll have to expand the message to a plain string here. */
	char message_buffer[0x100];

	(void)user_data;

	/* TODO: This unbounded printf is so nasty... */
	vsprintf(message_buffer, format, arg);
	strcat(message_buffer, "\n");

	libretro_callbacks.log(RETRO_LOG_WARN, "%s", message_buffer);
}

/***********/
/* Options */
/***********/

static cc_bool DoOptionBoolean(const char* const key, const char* const true_value)
{
	struct retro_variable variable;
	variable.key = key;
	return libretro_callbacks.environment(RETRO_ENVIRONMENT_GET_VARIABLE, (void*)&variable) && variable.value != NULL && strcmp(variable.value, true_value) == 0;
}

static int DoOptionNumerical(const char* const key)
{
	struct retro_variable variable;

	variable.key = key;
	if (libretro_callbacks.environment(RETRO_ENVIRONMENT_GET_VARIABLE, (void*)&variable) && variable.value != NULL)
		return atoi(variable.value);

	return 0;
}

static void UpdateOptions(const cc_bool only_update_flags)
{
	const cc_bool pal_mode_changed = pal_mode_enabled != DoOptionBoolean("clownmdemu_tv_standard", "pal");

	pal_mode_enabled ^= pal_mode_changed;

	if (pal_mode_changed && !only_update_flags)
	{
		Mixer_Deinitialise(&mixer);
		Mixer_Initialise(&mixer, pal_mode_enabled);

		{
			struct retro_system_av_info info;
			retro_get_system_av_info(&info);
			libretro_callbacks.environment(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, (void*)&info);
		}
	}

	Geometry_SetTallInterlaceMode2(DoOptionBoolean("clownmdemu_tall_interlace_mode_2", "enabled"));

	clownmdemu.configuration.region                           =  DoOptionBoolean("clownmdemu_overseas_region", "elsewhere") ? CLOWNMDEMU_REGION_OVERSEAS : CLOWNMDEMU_REGION_DOMESTIC;
	clownmdemu.configuration.tv_standard                      =  pal_mode_enabled ? CLOWNMDEMU_TV_STANDARD_PAL : CLOWNMDEMU_TV_STANDARD_NTSC;
	clownmdemu.configuration.low_pass_filter_disabled         = !DoOptionBoolean("clownmdemu_lowpass_filter", "enabled");
	clownmdemu.configuration.cd_add_on_enabled                =  DoOptionBoolean("clownmdemu_cd_addon", "enabled");
	clownmdemu.vdp.configuration.sprites_disabled             =  DoOptionBoolean("clownmdemu_disable_sprite_plane", "enabled");
	clownmdemu.vdp.configuration.window_disabled              =  DoOptionBoolean("clownmdemu_disable_window_plane", "enabled");
	clownmdemu.vdp.configuration.planes_disabled[0]           =  DoOptionBoolean("clownmdemu_disable_plane_a", "enabled");
	clownmdemu.vdp.configuration.planes_disabled[1]           =  DoOptionBoolean("clownmdemu_disable_plane_b", "enabled");
	clownmdemu.vdp.configuration.widescreen_tiles             =  DoOptionNumerical("clownmdemu_widescreen_tiles");
	clownmdemu.fm.configuration.fm_channels_disabled[0]       =  DoOptionBoolean("clownmdemu_disable_fm1", "enabled");
	clownmdemu.fm.configuration.fm_channels_disabled[1]       =  DoOptionBoolean("clownmdemu_disable_fm2", "enabled");
	clownmdemu.fm.configuration.fm_channels_disabled[2]       =  DoOptionBoolean("clownmdemu_disable_fm3", "enabled");
	clownmdemu.fm.configuration.fm_channels_disabled[3]       =  DoOptionBoolean("clownmdemu_disable_fm4", "enabled");
	clownmdemu.fm.configuration.fm_channels_disabled[4]       =  DoOptionBoolean("clownmdemu_disable_fm5", "enabled");
	clownmdemu.fm.configuration.fm_channels_disabled[5]       =  DoOptionBoolean("clownmdemu_disable_fm6", "enabled");
	clownmdemu.fm.configuration.dac_channel_disabled          =  DoOptionBoolean("clownmdemu_disable_dac", "enabled");
	clownmdemu.fm.configuration.ladder_effect_disabled        = !DoOptionBoolean("clownmdemu_ladder_effect", "enabled");
	clownmdemu.psg.configuration.tone_disabled[0]             =  DoOptionBoolean("clownmdemu_disable_psg1", "enabled");
	clownmdemu.psg.configuration.tone_disabled[1]             =  DoOptionBoolean("clownmdemu_disable_psg2", "enabled");
	clownmdemu.psg.configuration.tone_disabled[2]             =  DoOptionBoolean("clownmdemu_disable_psg3", "enabled");
	clownmdemu.psg.configuration.noise_disabled               =  DoOptionBoolean("clownmdemu_disable_psg_noise", "enabled");
	clownmdemu.mega_cd.pcm.configuration.channels_disabled[0] =  DoOptionBoolean("clownmdemu_disable_pcm1", "enabled");
	clownmdemu.mega_cd.pcm.configuration.channels_disabled[1] =  DoOptionBoolean("clownmdemu_disable_pcm2", "enabled");
	clownmdemu.mega_cd.pcm.configuration.channels_disabled[2] =  DoOptionBoolean("clownmdemu_disable_pcm3", "enabled");
	clownmdemu.mega_cd.pcm.configuration.channels_disabled[3] =  DoOptionBoolean("clownmdemu_disable_pcm4", "enabled");
	clownmdemu.mega_cd.pcm.configuration.channels_disabled[4] =  DoOptionBoolean("clownmdemu_disable_pcm5", "enabled");
	clownmdemu.mega_cd.pcm.configuration.channels_disabled[5] =  DoOptionBoolean("clownmdemu_disable_pcm6", "enabled");
	clownmdemu.mega_cd.pcm.configuration.channels_disabled[6] =  DoOptionBoolean("clownmdemu_disable_pcm7", "enabled");
	clownmdemu.mega_cd.pcm.configuration.channels_disabled[7] =  DoOptionBoolean("clownmdemu_disable_pcm8", "enabled");
	clownmdemu.mega_cd.cdda.configuration.disabled            =  DoOptionBoolean("clownmdemu_disable_cdda", "enabled");
}

/************************/
/* ClownCD IO Callbacks */
/************************/

static void* ClownCDFileOpen(const char* const filename, const ClownCD_FileMode mode)
{
	int libretro_mode;

	switch (mode)
	{
		case CLOWNCD_RB:
			libretro_mode = RETRO_VFS_FILE_ACCESS_READ;
			break;

		case CLOWNCD_WB:
			libretro_mode = RETRO_VFS_FILE_ACCESS_WRITE;
			break;

		default:
			return NULL;
	}

	return File_Open(filename, libretro_mode, RETRO_VFS_FILE_ACCESS_HINT_NONE);
}

static int ClownCDFileClose(void* const stream)
{
	return File_Close((struct retro_vfs_file_handle*)stream);
}

static size_t ClownCDFileRead(void* const buffer, const size_t size, const size_t count, void* const stream)
{
	int64_t total_read;

	if (size == 0 || count == 0)
		return 0;

	total_read = File_Read((struct retro_vfs_file_handle*)stream, buffer, size * count) / size;

	if (total_read < 0 || (uint64_t)total_read > (size_t)-1)
		return 0;

	return total_read;
}

static size_t ClownCDFileWrite(const void* const buffer, const size_t size, const size_t count, void* const stream)
{
	int64_t total_written;

	if (size == 0 || count == 0)
		return 0;

	total_written = File_Write((struct retro_vfs_file_handle*)stream, buffer, size * count) / size;

	if (total_written < 0 || (uint64_t)total_written > (size_t)-1)
		return 0;

	return total_written;
}

static long ClownCDFileTell(void* const stream)
{
	const int64_t position = File_Tell((struct retro_vfs_file_handle*)stream);

	if (position < 0 || position > LONG_MAX)
		return -1;

	return position;
}

static int ClownCDFileSeek(void* const stream, const long position, const ClownCD_FileOrigin origin)
{
	int libretro_origin;

	switch (origin)
	{
		case CLOWNCD_SEEK_SET:
			libretro_origin = RETRO_VFS_SEEK_POSITION_START;
			break;

		case CLOWNCD_SEEK_CUR:
			libretro_origin = RETRO_VFS_SEEK_POSITION_CURRENT;
			break;

		case CLOWNCD_SEEK_END:
			libretro_origin = RETRO_VFS_SEEK_POSITION_END;
			break;

		default:
			return -1;
	}

	return File_Seek((struct retro_vfs_file_handle*)stream, position, libretro_origin) == -1 ? -1 : 0;
}

static const ClownCD_FileCallbacks clowncd_callbacks = {
	ClownCDFileOpen,
	ClownCDFileClose,
	ClownCDFileRead,
	ClownCDFileWrite,
	ClownCDFileTell,
	ClownCDFileSeek
};

/****************/
/* libretro API */
/****************/

void retro_init(void)
{
	LoadFileIOCallbacks();

	/* Inform frontend of serialisation quirks. */
	{
		uint64_t serialisation_quirks = RETRO_SERIALIZATION_QUIRK_ENDIAN_DEPENDENT | RETRO_SERIALIZATION_QUIRK_PLATFORM_DEPENDENT;
		libretro_callbacks.environment(RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS, (void*)&serialisation_quirks);
	}

	/* Initialise ClownMDEmu. */
	clownmdemu_callbacks.user_data = NULL;
	clownmdemu_callbacks.colour_updated    = ColourUpdatedCallback_0RGB1555;
	clownmdemu_callbacks.scanline_rendered = ScanlineRenderedCallback;
	clownmdemu_callbacks.input_requested   = InputRequestedCallback;
	clownmdemu_callbacks.fm_audio_to_be_generated   = FMAudioToBeGeneratedCallback;
	clownmdemu_callbacks.psg_audio_to_be_generated  = PSGAudioToBeGeneratedCallback;
	clownmdemu_callbacks.pcm_audio_to_be_generated  = PCMAudioToBeGeneratedCallback;
	clownmdemu_callbacks.cdda_audio_to_be_generated = CDDAAudioToBeGeneratedCallback;
	clownmdemu_callbacks.cd_seeked       = CDSeekCallback;
	clownmdemu_callbacks.cd_sector_read  = CDSectorReadCallback;
	clownmdemu_callbacks.cd_track_seeked = CDSeekTrackCallback;
	clownmdemu_callbacks.cd_audio_read   = CDAudioReadCallback;
	clownmdemu_callbacks.save_file_opened_for_reading = SaveFileOpenedForReadingCallback;
	clownmdemu_callbacks.save_file_read               = SaveFileReadCallback;
	clownmdemu_callbacks.save_file_opened_for_writing = SaveFileOpenedForWritingCallback;
	clownmdemu_callbacks.save_file_written            = SaveFileWrittenCallback;
	clownmdemu_callbacks.save_file_closed             = SaveFileClosedCallback;
	clownmdemu_callbacks.save_file_removed            = SaveFileRemovedCallback;
	clownmdemu_callbacks.save_file_size_obtained      = SaveFileSizeObtainedCallback;

	ClownCD_SetErrorCallback(ClownCDLog, NULL);
	ClownMDEmu_SetLogCallback(ClownMDEmuLog, NULL);

	ClownMDEmu_Constant_Initialise();
	{
		ClownMDEmu_InitialConfiguration configuration;
		memset(&configuration, 0, sizeof(configuration));
		ClownMDEmu_Initialise(&clownmdemu, &configuration, &clownmdemu_callbacks);
	}

	UpdateOptions(cc_true);

	/* Initialise the mixer. */
	Mixer_Initialise(&mixer, pal_mode_enabled);

	CDReader_Initialise(&cd_reader);
}

void retro_deinit(void)
{
	CDReader_Deinitialise(&cd_reader);
	Mixer_Deinitialise(&mixer);
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

#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif

void retro_get_system_info(struct retro_system_info* const info)
{
	info->library_name     = "ClownMDEmu";
	info->library_version  = "v1.6.5" GIT_VERSION;
	info->need_fullpath    = true;
	info->valid_extensions = CARTRIDGE_FILE_EXTENSIONS "|" CD_FILE_EXTENSIONS;
	info->block_extract    = false;
}

void retro_get_system_av_info(struct retro_system_av_info* const info)
{
	enum retro_pixel_format pixel_format;

	/* Determine which pixel format to render as in the event that
	   'RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER' fails or produces a framebuffer
	   that is in a format that we don't support. */
	pixel_format = RETRO_PIXEL_FORMAT_RGB565;
	if (libretro_callbacks.environment(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, (void*)&pixel_format))
	{
		fallback_colour_updated_callback = ColourUpdatedCallback_RGB565;
		fallback_scanline_rendered_callback = ScanlineRenderedCallback_16Bit;
	}
	else
	{
		pixel_format = RETRO_PIXEL_FORMAT_XRGB8888;
		if (libretro_callbacks.environment(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, (void*)&pixel_format))
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

	/* Initialise these to avoid a division by 0 in Geometry_Export. */
	Geometry_SetScreenSize(VDP_H40_SCREEN_WIDTH_IN_TILE_PAIRS * VDP_TILE_PAIR_WIDTH, VDP_V28_SCANLINES_IN_TILES * VDP_STANDARD_TILE_HEIGHT);

	/* Populate the 'retro_system_av_info' struct. */
	Geometry_Export(&info->geometry);

	info->timing.fps = pal_mode_enabled ? CLOWNMDEMU_MULTIPLY_BY_PAL_FRAMERATE(1.0) : CLOWNMDEMU_MULTIPLY_BY_NTSC_FRAMERATE(1.0);	/* Standard PAL and NTSC framerates. */
	info->timing.sample_rate = pal_mode_enabled ? MIXER_OUTPUT_SAMPLE_RATE_PAL : MIXER_OUTPUT_SAMPLE_RATE_NTSC;
}

void retro_set_environment(const retro_environment_t environment_callback)
{
	libretro_callbacks.environment = environment_callback;

	/* Declare the options to the frontend. */
	libretro_set_core_options(libretro_callbacks.environment);

	/* Retrieve a log callback from the frontend. */
	{
		struct retro_log_callback logging;
		if (libretro_callbacks.environment(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, (void*)&logging) && logging.log != NULL)
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
			{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,     "Up"    },
			{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,   "Down"  },
			{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,   "Left"  },
			{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT,  "Right" },
			{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,      "A"     },
			{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,      "B"     },
			{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,      "C"     },
			{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,      "X"     },
			{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,      "Y"     },
			{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,      "Z"     },
			{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Start" },
			{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Mode"  },
			/* Player 2. */
			{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,     "Up"    },
			{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,   "Down"  },
			{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,   "Left"  },
			{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT,  "Right" },
			{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,      "A"     },
			{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,      "B"     },
			{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,      "C"     },
			{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,      "X"     },
			{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,      "Y"     },
			{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,      "Z"     },
			{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Start" },
			{ 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Mode"  },
			/* End. */
			{ 0, 0, 0, 0, NULL }
		};

		libretro_callbacks.environment(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, (void*)&desc);
	}

	/* Declare Mega CD Mode 1 subsystem. */
	{
		static const struct retro_subsystem_rom_info rom_info[] = {
			{ "Cartridge", CARTRIDGE_FILE_EXTENSIONS, false, false, true, NULL, 0 },
			{ "CD",        CD_FILE_EXTENSIONS,         true, false, true, NULL, 0 }
		};

		static const struct retro_subsystem_info info[] = {
			{ "Cartridge + CD", "cartandcd", rom_info, CC_COUNT_OF(rom_info), 0 },
			{ NULL, NULL, NULL, 0, 0 }
		};

		libretro_callbacks.environment(RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO, (void*)&info);
	}

	/* Allow Mega Drive games to be soft-patched by the frontend. */
	{
		static const struct retro_system_content_info_override overrides[] = {
			{ CARTRIDGE_FILE_EXTENSIONS, false, false },
			{ NULL, false, false }
		};

		libretro_callbacks.environment(RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE, (void*)&overrides);
	}

	/* Inform frontend of achievement support (implemented by `RETRO_ENVIRONMENT_SET_MEMORY_MAPS`). */
	{
		const bool achievements_supported = true;
		libretro_callbacks.environment(RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS, (void*)&achievements_supported);
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
	ClownMDEmu_SoftReset(&clownmdemu, rom != NULL, CDReader_IsOpen(&cd_reader));
}

static void MixerCompleteCallback(void* const user_data, const cc_s16l* const audio_samples, const size_t total_frames)
{
	(void)user_data;

	libretro_callbacks.audio_batch(audio_samples, total_frames);
}

void retro_run(void)
{
	bool options_updated;

	/* Refresh options if they've been updated. */
	if (libretro_callbacks.environment(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, (void*)&options_updated) && options_updated)
		UpdateOptions(cc_false);

	/* Poll inputs. */
	libretro_callbacks.input_poll();

	Mixer_Begin(&mixer);

	ClownMDEmu_Iterate(&clownmdemu);

	Mixer_End(&mixer, MixerCompleteCallback, NULL);

	Geometry_Update();

	/* Upload the completed frame to the frontend. */
	libretro_callbacks.video(current_framebuffer, geometry.current_screen_width, geometry.current_screen_height, current_framebuffer_pitch);
}

#if RETRO_IS_BIG_ENDIAN
#define MEMDESC_NATIVE_ENDIAN RETRO_MEMDESC_BIGENDIAN
#else
#define MEMDESC_NATIVE_ENDIAN 0
#endif

static void SetMemoryMaps(const cc_u16l* const rom, const size_t rom_length)
{
	/* Does not reflect the actual memory layout, as addresses are arbitrarily defined by RetroAchievements:
	   https://github.com/RetroAchievements/rcheevos/blob/86aeb6e783e0b9f8687129d79d2e53ea92f3e5f0/src/rcheevos/consoleinfo.c#L838-L842 */
	struct retro_memory_descriptor descriptors[] = {
		{RETRO_MEMDESC_CONST      | MEMDESC_NATIVE_ENDIAN, (void*)0,                                        0, 0x00000000, 0, 0, 0                                               , "ROM"    },
		{RETRO_MEMDESC_SYSTEM_RAM | MEMDESC_NATIVE_ENDIAN, (void*)clownmdemu.state.m68k.ram,                0, 0x00FF0000, 0, 0, sizeof(clownmdemu.state.m68k.ram)               , "68KRAM" },
		{RETRO_MEMDESC_SYSTEM_RAM | MEMDESC_NATIVE_ENDIAN, (void*)clownmdemu.state.mega_cd.prg_ram.buffer,  0, 0x80020000, 0, 0, sizeof(clownmdemu.state.mega_cd.prg_ram.buffer) , "PRGRAM" },
		{RETRO_MEMDESC_SYSTEM_RAM | MEMDESC_NATIVE_ENDIAN, (void*)clownmdemu.state.mega_cd.word_ram.buffer, 0, 0x00200000, 0, 0, sizeof(clownmdemu.state.mega_cd.word_ram.buffer), "WORDRAM"},
		{RETRO_MEMDESC_SYSTEM_RAM,                         (void*)clownmdemu.state.z80.ram,                 0, 0x00A00000, 0, 0, sizeof(clownmdemu.state.z80.ram)                , "Z80RAM" },
	};

	struct retro_memory_map memory_maps;

	memory_maps.descriptors = descriptors;
	memory_maps.num_descriptors = CC_COUNT_OF(descriptors);

	descriptors[0].ptr = (void*)rom;
	descriptors[0].len = sizeof(*rom) * rom_length;

	libretro_callbacks.environment(RETRO_ENVIRONMENT_SET_MEMORY_MAPS, (void*)&memory_maps);
}

static bool LoadCartridge(const struct retro_game_info* const info)
{
	const unsigned char *buffer = (const unsigned char*)info->data;
	size_t buffer_size = info->size;

	unsigned char *local_rom_buffer = NULL;

	if (buffer == NULL && LoadFileToBuffer(info->path, &local_rom_buffer, &buffer_size))
		buffer = local_rom_buffer;

	if (buffer != NULL && CreateROMBuffer(buffer, buffer_size, &rom, &rom_length))
	{
		ClownMDEmu_SetCartridge(&clownmdemu, rom, rom_length);
		return true;
	}

	free(local_rom_buffer);
	return false;
}

static bool LoadCD(const struct retro_game_info* const info)
{
	if (info->data != NULL)
		return false;

	CDReader_Open(&cd_reader, NULL, info->path, &clowncd_callbacks);

	if (!CDReader_IsOpen(&cd_reader))
		return false;

	CDReader_SeekToSector(&cd_reader, 0);
	return true;
}

static bool LoadCartridgeOrCD(const struct retro_game_info* const info)
{
	if (LoadCD(info))
	{
		if (CDReader_IsMegaCDGame(&cd_reader))
			return true;

		CDReader_Close(&cd_reader);
	}

	return LoadCartridge(info);
}

static void UnloadCartridge(void)
{
	free(rom);
	rom = NULL;
	rom_length = 0;
}

static void UnloadCD(void)
{
	CDReader_Close(&cd_reader);
}

bool retro_load_game(const struct retro_game_info* const info)
{
	return retro_load_game_special(0, info, 1);
}

void retro_unload_game(void)
{
	UnloadCartridge();
	UnloadCD();
}

unsigned int retro_get_region(void)
{
	return pal_mode_enabled ? RETRO_REGION_PAL : RETRO_REGION_NTSC;
}

bool retro_load_game_special(const unsigned int type, const struct retro_game_info* const info, const size_t num)
{
	bool success = true;

	if (type != 0)
		return false;

	switch (num)
	{
		case 1:
			if (!LoadCartridgeOrCD(&info[0]))
				success = false;
			break;

		case 2:
			if (!LoadCartridge(&info[0]) || !LoadCD(&info[1]))
				success = false;
			break;

		default:
			success = false;
			break;
	}

	if (!success)
	{
		UnloadCartridge();
		UnloadCD();
		return false;
	}

	/* Provide memory descriptors to the frontend (needed for achievements, cheats, and the like). */
	SetMemoryMaps(rom, rom_length);

	/* Boot the emulated Mega Drive. */
	retro_reset();

	return true;
}

typedef struct SerialisedState
{
	ClownMDEmu_StateBackup clownmdemu;
	CDReader_StateBackup cd_reader;
} SerialisedState;

size_t retro_serialize_size(void)
{
	return sizeof(SerialisedState);
}

bool retro_serialize(void* const data, const size_t size)
{
	SerialisedState* const serialised_state = (SerialisedState*)data;

	(void)size;

	ClownMDEmu_SaveState(&clownmdemu, &serialised_state->clownmdemu);
	CDReader_SaveState(&cd_reader, &serialised_state->cd_reader);
	return true;
}

bool retro_unserialize(const void* const data, const size_t size)
{
	const SerialisedState* const serialised_state = (SerialisedState*)data;

	(void)size;

	ClownMDEmu_LoadState(&clownmdemu, &serialised_state->clownmdemu);
	CDReader_LoadState(&cd_reader, &serialised_state->cd_reader);
	return true;
}

void* retro_get_memory_data(const unsigned int id)
{
	switch (id)
	{
		case RETRO_MEMORY_SAVE_RAM:
			return clownmdemu.state.external_ram.buffer;

		case RETRO_MEMORY_SYSTEM_RAM:
			return clownmdemu.state.m68k.ram;

		case RETRO_MEMORY_VIDEO_RAM:
			return clownmdemu.vdp.state.vram;
	}

	return NULL;
}

size_t retro_get_memory_size(const unsigned int id)
{
	switch (id)
	{
		case RETRO_MEMORY_SAVE_RAM:
			return sizeof(clownmdemu.state.external_ram.buffer);

		case RETRO_MEMORY_SYSTEM_RAM:
			return sizeof(clownmdemu.state.m68k.ram);

		case RETRO_MEMORY_VIDEO_RAM:
			return sizeof(clownmdemu.vdp.state.vram);
	}

	return 0;
}

void retro_cheat_reset(void)
{
	/* TODO: This. */
}

void retro_cheat_set(const unsigned int index, const bool enabled, const char* const code)
{
	(void)index;
	(void)enabled;
	(void)code;

	/* TODO: This. */
}
