#pragma once

#include <stdlib.h>
#include <stddef.h>
#include <fcntl.h>

#include "types.h"

/// ======== AFL DEFINITIONS

#define SHM_ENV_VAR "__AFL_SHM_ID"
#define FORKSRV_FD_IN 198
#define FORKSRV_FD_OUT 199

/// Possible errors from the FS
#define FS_NEW_ERROR 0xeffe0000
#define FS_ERROR_MAP_SIZE 1
#define FS_ERROR_MAP_ADDR 2
#define FS_ERROR_SHM_OPEN 4
#define FS_ERROR_SHMAT 8
#define FS_ERROR_MMAP 16
#define FS_ERROR_OLD_CMPLOG 32
#define FS_ERROR_OLD_CMPLOG_QEMU 64

#define FS_NEW_VERSION_MIN 1
#define FS_NEW_VERSION_MAX 1
#define FS_NEW_OPT_MAPSIZE 0x00000001      // parameter: 32 bit value
#define FS_NEW_OPT_SHDMEM_FUZZ 0x00000002  // parameter: none
#define FS_NEW_OPT_AUTODICT 0x00000800     // autodictionary data
#define FS_OPT_NEWCMPLOG 0x02000000

/// FS handshake
#define FS_VERSION (0x41464c00) // "AFL<nul>"
#define FS_VERSION_HANDSHAKE (FS_VERSION + FS_NEW_VERSION_MAX)
#define FS_VERSION_HANDSHAKE_RES (FS_VERSION_HANDSHAKE ^ 0xffffffff)


#define DEFAULT_MAP_SIZE (1 << 16)

/// ======== AFL functions


/// Returns whether AFL is here.
int afl_is_here(); 

/// Reads somthing from AFL
/// Returns non-zero if error
int afl_read(u32 *out);

/// Writes something to AFL
/// Returns non-zero if error
int afl_write(u32 to_write);

/// Sends error to AFL
/// Returns non-zero if error.
int afl_senderr(u32 error);

/// Shared memory
/// Returns non-zero if error.
int afl_setup_shmem(void** mem, size_t* size);