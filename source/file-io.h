#ifndef FILE_IO_H
#define FILE_IO_H

#include <stddef.h>

#include "libretro-interface.h"

extern retro_vfs_open_t File_Open;
extern retro_vfs_close_t File_Close;
extern retro_vfs_size_t File_GetSize;
extern retro_vfs_tell_t File_Tell;
extern retro_vfs_seek_t File_Seek;
extern retro_vfs_read_t File_Read;
extern retro_vfs_write_t File_Write;
extern retro_vfs_remove_t File_Remove;

void LoadFileIOCallbacks(void);

bool LoadFileToBuffer(const char* const path, unsigned char** const output_file_buffer, size_t* const output_file_size);

#endif /* FILE_IO_H */
