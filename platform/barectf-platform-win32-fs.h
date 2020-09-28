#ifndef _BARECTF_PLATFORM_WIN32_FS_H
#define _BARECTF_PLATFORM_WIN32_FS_H

#include <stdint.h>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

struct barectf_platform_win32_fs;

struct barectf_platform_win32_fs *barectf_platform_win32_fs_create(LPCWSTR trace_folder_path);
void barectf_platform_win32_fs_destroy(struct barectf_platform_win32_fs *ctx);

struct barectf_default_ctx *barectf_platform_win32_fs_get_barectf_ctx(struct barectf_platform_win32_fs *platform);

#ifdef __cplusplus
}
#endif

#endif // _BARECTF_PLATFORM_WIN32_FS_H
