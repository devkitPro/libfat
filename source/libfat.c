/*
	libfat.c
	Simple functionality for startup, mounting and unmounting of FAT-based devices.
	
 Copyright (c) 2006 Michael "Chishm" Chisholm
	
 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation and/or
     other materials provided with the distribution.
  3. The name of the author may not be used to endorse or promote products derived
     from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

	2006-07-11 - Chishm
		* Original release

	2006-08-13 - Chishm
		* Moved all externally visible directory related functions to fatdir
		
	2006-08-14 - Chishm
		* Added extended devoptab_t functions
		
	2007-01-10 - Chishm
		* fatInit now sets the correct path when setAsDefaultDevice
		
	2007-01-11 - Chishm
		* Added missing #include <unistd.h>

	2007-10-25 - Chishm
		* Added statvfs functionality
*/

#include <sys/iosupport.h>
#include <unistd.h>
#include <string.h>

#include "common.h"
#include "partition.h"
#include "fatfile.h"
#include "fatdir.h"
#include "lock.h"

#ifdef GBA
#define DEFAULT_CACHE_PAGES 2
#else
#define DEFAULT_CACHE_PAGES 8
#endif

const devoptab_t dotab_fat = {
	"fat",
	sizeof (FILE_STRUCT),
	_FAT_open_r,
	_FAT_close_r,
	_FAT_write_r,
	_FAT_read_r,
	_FAT_seek_r,
	_FAT_fstat_r,
	_FAT_stat_r,
	_FAT_link_r,
	_FAT_unlink_r,
	_FAT_chdir_r,
	_FAT_rename_r,
	_FAT_mkdir_r,
	sizeof (DIR_STATE_STRUCT),
	_FAT_diropen_r,
	_FAT_dirreset_r,
	_FAT_dirnext_r,
	_FAT_dirclose_r,
	_FAT_statvfs_r
};

bool fatInit (u32 cacheSize, bool setAsDefaultDevice) {

	int i;
	bool device = false, setDefault = false;

	if ( PI_MAX_PARTITIONS == 1 ) {
		if ( _FAT_partition_mount ( 0 , cacheSize) ) {
			_FAT_partition_setDefaultInterface (0);
		} else {
			return false;
		}

	} else {
	
		for ( i = 1; i < PI_MAX_PARTITIONS; i++ ) {
			device = _FAT_partition_mount ( i , cacheSize);
			if ( device && !setDefault ) {
				_FAT_partition_setDefaultInterface (i);
				setDefault = true;
			}
		}
		if ( !setDefault ) return false;
	}

	AddDevice (&dotab_fat);
	
	if (setAsDefaultDevice) {
		char filePath[MAXPATHLEN * 2] = "fat:/";
#ifndef GBA
		if ( __system_argv->argvMagic == ARGV_MAGIC && __system_argv->argc >= 1 ) {
		
			if ( !strncasecmp( __system_argv->argv[0], "fat", 3)) {
			
				strcpy(filePath, __system_argv->argv[0]);
				char *lastSlash = strrchr( filePath, '/' );

				if ( NULL != lastSlash) {
					if ( *(lastSlash - 1) == ':') lastSlash++;
					*lastSlash = 0;
				}
			}
		}
#endif
		chdir (filePath);
	}

	_FAT_lock_init();

	return true;
}

bool fatInitDefault (void) {
	return fatInit (DEFAULT_CACHE_PAGES, true);
}

bool fatMountNormalInterface (PARTITION_INTERFACE partitionNumber, u32 cacheSize) {
	return _FAT_partition_mount (partitionNumber, cacheSize);
}

bool fatMountCustomInterface (const IO_INTERFACE* device, u32 cacheSize) {
	return _FAT_partition_mountCustomInterface (device, cacheSize);
}

bool fatUnmount (PARTITION_INTERFACE partitionNumber) {
	return _FAT_partition_unmount (partitionNumber);
}


bool fatUnsafeUnmount (PARTITION_INTERFACE partitionNumber) {
	return _FAT_partition_unsafeUnmount (partitionNumber);
}

bool fatSetDefaultInterface (PARTITION_INTERFACE partitionNumber) {
	return _FAT_partition_setDefaultInterface (partitionNumber);
}
