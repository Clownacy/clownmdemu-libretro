#ifndef FILE_IO_H
#define FILE_IO_H

#include <stddef.h>

#include "libretro-interface.h"

typedef struct FileFunctions
{
	retro_vfs_open_t open;
	retro_vfs_close_t close;
	retro_vfs_size_t get_size;
	retro_vfs_tell_t tell;
	retro_vfs_seek_t seek;
	retro_vfs_read_t read;
	retro_vfs_write_t write;
	retro_vfs_remove_t remove;
} FileFunctions;

extern FileFunctions file_io;

void LoadFileIOCallbacks(void);

bool LoadFileToBuffer(const char* const path, unsigned char** const output_file_buffer, size_t* const output_file_size);

#endif /* FILE_IO_H */
