/*
	io_sccf.c based on

	compact_flash.c
	By chishm (Michael Chisholm)

	Hardware Routines for reading a compact flash card
	using the Super Card CF

	CF routines modified with help from Darkfader

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
*/


#include "io_sccf.h"
#include "io_sc_common.h"

/*-----------------------------------------------------------------
Since all CF addresses and commands are the same for the GBAMP,
simply use it's functions instead.
-----------------------------------------------------------------*/

extern bool _MPCF_isInserted (void);
extern bool _MPCF_clearStatus (void);
extern bool _MPCF_readSectors (u32 sector, u32 numSectors, void* buffer);
extern bool _MPCF_writeSectors (u32 sector, u32 numSectors, void* buffer);


/*-----------------------------------------------------------------
_SCCF_unlock
Returns true if SuperCard was unlocked, false if failed
Added by MightyMax
Modified by Chishm
-----------------------------------------------------------------*/
bool _SCCF_unlock(void) {
#define CF_REG_LBA1 (*(vu16*)0x09060000)
	unsigned char temp;
	_SC_changeMode (SC_MODE_MEDIA);	
	// provoke a ready reply
	temp = CF_REG_LBA1;
	CF_REG_LBA1 = (~temp & 0xFF);
	temp = (~temp & 0xFF);
	return (CF_REG_LBA1 == temp);
#undef CF_REG_LBA1
} 

bool _SCCF_shutdown(void) {
	return _MPCF_clearStatus() ;
} ;

bool _SCCF_startUp(void) {
	return _SCCF_unlock() ;
} ;


IO_INTERFACE _io_sccf = {
	DEVICE_TYPE_SCCF,
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_SLOT_GBA,
	(FN_MEDIUM_STARTUP)&_SCCF_startUp,
	(FN_MEDIUM_ISINSERTED)&_MPCF_isInserted,
	(FN_MEDIUM_READSECTORS)&_MPCF_readSectors,
	(FN_MEDIUM_WRITESECTORS)&_MPCF_writeSectors,
	(FN_MEDIUM_CLEARSTATUS)&_MPCF_clearStatus,
	(FN_MEDIUM_SHUTDOWN)&_SCCF_shutdown
} ;
