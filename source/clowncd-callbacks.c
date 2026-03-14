#include "clowncd-callbacks.h"

#include "file-io.h"

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

	return file_io.open(filename, libretro_mode, RETRO_VFS_FILE_ACCESS_HINT_NONE);
}

static int ClownCDFileClose(void* const stream)
{
	return file_io.close((struct retro_vfs_file_handle*)stream);
}

static size_t ClownCDFileRead(void* const buffer, const size_t size, const size_t count, void* const stream)
{
	int64_t total_read;

	if (size == 0 || count == 0)
		return 0;

	total_read = file_io.read((struct retro_vfs_file_handle*)stream, buffer, size * count) / size;

	if (total_read < 0 || (uint64_t)total_read > (size_t)-1)
		return 0;

	return total_read;
}

static size_t ClownCDFileWrite(const void* const buffer, const size_t size, const size_t count, void* const stream)
{
	int64_t total_written;

	if (size == 0 || count == 0)
		return 0;

	total_written = file_io.write((struct retro_vfs_file_handle*)stream, buffer, size * count) / size;

	if (total_written < 0 || (uint64_t)total_written > (size_t)-1)
		return 0;

	return total_written;
}

static long ClownCDFileTell(void* const stream)
{
	const int64_t position = file_io.tell((struct retro_vfs_file_handle*)stream);

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

	return file_io.seek((struct retro_vfs_file_handle*)stream, position, libretro_origin) == -1 ? -1 : 0;
}

const ClownCD_FileCallbacks clowncd_callbacks = {
	ClownCDFileOpen,
	ClownCDFileClose,
	ClownCDFileRead,
	ClownCDFileWrite,
	ClownCDFileTell,
	ClownCDFileSeek
};
