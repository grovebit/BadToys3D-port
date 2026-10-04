#include "bt3d_platform.h"

#include <stdio.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

/* Both paths must be on the same filesystem. Never remove the old file first. */
int bt3d_replace_file(const char *source, const char *destination) {
#if defined(_WIN32)
    return MoveFileExA(source, destination, MOVEFILE_REPLACE_EXISTING) != 0;
#else
    return rename(source, destination) == 0;
#endif
}
