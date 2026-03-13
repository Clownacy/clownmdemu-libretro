#include "file-io.h"

#include <stdio.h>
#include <stdlib.h>

retro_vfs_open_t File_Open;
retro_vfs_close_t File_Close;
retro_vfs_size_t File_GetSize;
retro_vfs_tell_t File_Tell;
retro_vfs_seek_t File_Seek;
retro_vfs_read_t File_Read;
retro_vfs_write_t File_Write;
retro_vfs_remove_t File_Remove;

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

void LoadFileIOCallbacks(void)
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

bool LoadFileToBuffer(const char* const path, unsigned char** const output_file_buffer, size_t* const output_file_size)
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

