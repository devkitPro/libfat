/*

	gcsd.h

	Hardware routines for reading and writing to SD geckos connected
	to the memory card ports.

	These functions are just wrappers around libsdcard's functions.

 Copyright (c) 2008 Sven "svpe" Peter <svpe@gmx.net>
	
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
*/

#include "gcsd.h"

#include <sdcard/card_cmn.h>
#include <sdcard/card_io.h>
#include <sdcard/card_buf.h>

static int __gcsd_init = 0;

bool __gcsd_isInserted(int n)
{
	if(sdgecko_readStatus(n) == CARDIO_ERROR_NOCARD)
		return false;
	return true;
}

bool __gcsd_startup(int n)
{
	if(__gcsd_init == 1)
		return __gcsd_isInserted(n);
	sdgecko_initBufferPool();
	sdgecko_initIODefault();
	__gcsd_init = 1;
	return __gcsd_isInserted(n);
}


bool __gcsd_readSectors(int n, u32 sector, u32 numSectors, void *buffer)
{
	s32 i;
	s32 ret;

	for(i = 0; i < numSectors; i++)
	{
		ret = sdgecko_readSector(n, sector + i, buffer + (i * 512), 512);
		if(ret != CARDIO_ERROR_READY)
			return false;
	}

	if(ret == CARDIO_ERROR_READY)
		return true;
	return false;
}

bool __gcsd_writeSectors(int n, u32 sector, u32 numSectors, void *buffer)
{
	s32 i;
	s32 ret;

	for(i = 0; i < numSectors; i++)
	{
		ret = sdgecko_writeSector(n, sector + i, buffer + (i * 512), 512);
		if(ret != CARDIO_ERROR_READY)
			return false;
	}

	if(ret == CARDIO_ERROR_READY)
		return true;
	return false;
}

bool __gcsd_clearStatus(int n)
{
	return true; // FIXME
}

bool __gcsd_shutdown(int n)
{
	sdgecko_doUnmount(n);
	return true;
}


bool __gcsda_startup(void)
{
	return __gcsd_startup(0);
}

bool __gcsda_isInserted(void)
{
	return __gcsd_isInserted(0);
}

bool __gcsda_readSectors(u32 sector, u32 numSectors, void *buffer)
{
	return __gcsd_readSectors(0, sector, numSectors, buffer);
}

bool __gcsda_writeSectors(u32 sector, u32 numSectors, void *buffer)
{
	return __gcsd_writeSectors(0, sector, numSectors, buffer);
}

bool __gcsda_clearStatus(void)
{
	return __gcsd_clearStatus(0);
}

bool __gcsda_shutdown(void)
{
	return __gcsd_shutdown(0);
}



bool __gcsdb_startup(void)
{
	return __gcsd_startup(1);
}

bool __gcsdb_isInserted(void)
{
	return __gcsd_isInserted(1);
}

bool __gcsdb_readSectors(u32 sector, u32 numSectors, void *buffer)
{
	return __gcsd_readSectors(1, sector, numSectors, buffer);
}

bool __gcsdb_writeSectors(u32 sector, u32 numSectors, void *buffer)
{
	return __gcsd_writeSectors(1, sector, numSectors, buffer);
}

bool __gcsdb_clearStatus(void)
{
	return __gcsd_clearStatus(1);
}

bool __gcsdb_shutdown(void)
{
	return __gcsd_shutdown(1);
}

const IO_INTERFACE __io_gcsda = {
	DEVICE_TYPE_GC,
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_GAMECUBE_SLOTA,
	(FN_MEDIUM_STARTUP)&__gcsda_startup,
	(FN_MEDIUM_ISINSERTED)&__gcsda_isInserted,
	(FN_MEDIUM_READSECTORS)&__gcsda_readSectors,
	(FN_MEDIUM_WRITESECTORS)&__gcsda_writeSectors,
	(FN_MEDIUM_CLEARSTATUS)&__gcsda_clearStatus,
	(FN_MEDIUM_SHUTDOWN)&__gcsda_shutdown
} ;
const IO_INTERFACE __io_gcsdb = {
	DEVICE_TYPE_GC,
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_GAMECUBE_SLOTB,
	(FN_MEDIUM_STARTUP)&__gcsdb_startup,
	(FN_MEDIUM_ISINSERTED)&__gcsdb_isInserted,
	(FN_MEDIUM_READSECTORS)&__gcsdb_readSectors,
	(FN_MEDIUM_WRITESECTORS)&__gcsdb_writeSectors,
	(FN_MEDIUM_CLEARSTATUS)&__gcsdb_clearStatus,
	(FN_MEDIUM_SHUTDOWN)&__gcsdb_shutdown
} ;