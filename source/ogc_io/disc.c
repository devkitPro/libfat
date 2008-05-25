/*

	disc.c

	uniformed io-interface to work with Chishm's FAT library

	Written by MightyMax
  

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

	2005-11-06 - Chishm
		* Added WAIT_CR modifications for NDS

	2006-02-03 www.neoflash.com
		* Added SUPPORT_* defines, comment out any of the SUPPORT_* defines in disc_io.h to remove support
		  for the given interface and stop code being linked to the binary

	    * Added support for MK2 MMC interface

		* Added disc_Cache* functions

	2006-02-05 - Chishm
		* Added Supercard SD support

	2006-02-26 - Cytex
		* Added EFA2 support

	2006-05-18 - Chishm
		* Rewritten for FATlib disc.c
		
	2006-06-19 - Chishm
		* Changed read and write interface to accept a u32 instead of a u8 for the number of sectors
		
	2006-07-11 - Chishm
		* Removed disc_Cache* functions, since there is now a proper unified cache
		* Removed SUPPORT_* defines
		* Rewrote device detection functions
		* First libfat release
	
	2006-07-25 - Chishm
		* Changed IO_INTERFACEs to const
		
	2006-08-02 - Chishm
		* Added NinjaDS
		
	2006-12-25 - Chishm
		* Added DLDI
		* Removed experimental interfaces
		
	2007-05-01 - Chishm
		* Removed FCSR
*/

#include "../disc.h"
#include "wiisd.h"
#include "gcsd.h"


const IO_INTERFACE* ioInterfaces[] = {
#ifdef __gamecube__
	&__io_gcsda,
	&__io_gcsdb,
#endif
#ifdef __wii__
	&__io_wiisd
#endif
};

#ifdef __wii__
const IO_INTERFACE* _FAT_disc_wiiFindInterface(void)
{
	int i;

	for (i = 0; i < (sizeof(ioInterfaces) / sizeof(IO_INTERFACE*)); i++) {
		if ((ioInterfaces[i]->ioType == DEVICE_TYPE_WII) && (ioInterfaces[i]->fn_startup())) {
			return ioInterfaces[i];
		}
	}
	return NULL;
}
#endif

const IO_INTERFACE* _FAT_disc_gcFindInterface(void)
{
	int i;


	for (i = 0; i < (sizeof(ioInterfaces) / sizeof(IO_INTERFACE*)); i++) {
		if ((ioInterfaces[i]->ioType == DEVICE_TYPE_GC) && (ioInterfaces[i]->fn_startup())) {
			return ioInterfaces[i];
		}
	}
	return NULL;
}

const IO_INTERFACE* _FAT_disc_gcFindInterfaceSlot(int slot)
{
	int i;
	int mask;

	if(slot == 0)
		mask = FEATURE_GAMECUBE_SLOTA;
	else if(slot == 1)
		mask = FEATURE_GAMECUBE_SLOTB;
	else
		return NULL;

	for (i = 0; i < (sizeof(ioInterfaces) / sizeof(IO_INTERFACE*)); i++) {
		if ((ioInterfaces[i]->ioType == DEVICE_TYPE_GC) && (ioInterfaces[i]->features & mask) && (ioInterfaces[i]->fn_startup())) {
			return ioInterfaces[i];
		}
	}
	return NULL;
}


const IO_INTERFACE* _FAT_disc_findInterface(void)
{
#ifdef __wii__
	return _FAT_disc_wiiFindInterface();
#else
	return _FAT_disc_gcFindInterface();
#endif
}

const IO_INTERFACE* _FAT_disc_findInterfaceSlot (PARTITION_INTERFACE partitionNumber)
{
	switch(partitionNumber)
	{
#ifdef __gamecube__
		case PI_SDGECKO_A:
			return _FAT_disc_gcFindInterfaceSlot(0);
			break;
		case PI_SDGECKO_B:
			return _FAT_disc_gcFindInterfaceSlot(1);
			break;
#endif
#ifdef __wii__
		case PI_INTERNAL_SD:
			return _FAT_disc_wiiFindInterface();
			break;
#endif
		default:
			return NULL;
			break;
	}
}
