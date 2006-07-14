/*
	io_mpcf.c based on

	compact_flash.c
	By chishm (Michael Chisholm)

	Hardware Routines for reading a compact flash card
	using the GBA Movie Player

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


#include "io_mpcf.h"
#include "io_cf_common.h"

//---------------------------------------------------------------
// DMA
#ifdef _CF_USE_DMA
 #ifndef NDS
  #include "gba_dma.h"
 #else
  #include <nds/dma.h>
  #ifdef ARM9
   #include <nds/arm9/cache.h>
  #endif
 #endif
#endif

//---------------------------------------------------------------
// CF Addresses & Commands

// GBAMP CF Addresses
#define REG_MPCF_STS		(*(vu16*)0x098C0000)	// Status of the CF Card / Device control
#define REG_MPCF_CMD		(*(vu16*)0x090E0000)	// Commands sent to control chip and status return
#define REG_MPCF_ERR		(*(vu16*)0x09020000)	// Errors / Features

#define REG_MPCF_SEC		(*(vu16*)0x09040000)	// Number of sector to transfer
#define REG_MPCF_LBA1		(*(vu16*)0x09060000)	// 1st byte of sector address
#define REG_MPCF_LBA2		(*(vu16*)0x09080000)	// 2nd byte of sector address
#define REG_MPCF_LBA3		(*(vu16*)0x090A0000)	// 3rd byte of sector address
#define REG_MPCF_LBA4		(*(vu16*)0x090C0000)	// last nibble of sector address | 0xE0

#define MP_DATA			((vu16*)0x09000000)		// Pointer to buffer of CF data transered from card



/*-----------------------------------------------------------------
_MPCF_isInserted
Is a compact flash card inserted?
bool return OUT:  true if a CF card is inserted
-----------------------------------------------------------------*/
bool _MPCF_isInserted (void) {
	// Change register, then check if value did change
	REG_MPCF_STS = CF_STS_INSERTED;
	return ((REG_MPCF_STS & 0xff) == CF_STS_INSERTED);
}


/*-----------------------------------------------------------------
_MPCF_clearStatus
Tries to make the CF card go back to idle mode
bool return OUT:  true if a CF card is idle
-----------------------------------------------------------------*/
bool _MPCF_clearStatus (void) {
	int i;
	
	// Wait until CF card is finished previous commands
	i=0;
	while ((REG_MPCF_CMD & CF_STS_BUSY) && (i < CF_CARD_TIMEOUT))
	{
		i++;
	}
	
	// Wait until card is ready for commands
	i = 0;
	while ((!(REG_MPCF_STS & CF_STS_INSERTED)) && (i < CF_CARD_TIMEOUT))
	{
		i++;
	}
	if (i >= CF_CARD_TIMEOUT)
		return false;

	return true;
}


/*-----------------------------------------------------------------
_MPCF_readSectors
Read 512 byte sector numbered "sector" into "buffer"
u32 sector IN: address of first 512 byte sector on CF card to read
u32 numSectors IN: number of 512 byte sectors to read,
 1 to 256 sectors can be read
void* buffer OUT: pointer to 512 byte buffer to store data in
bool return OUT: true if successful
-----------------------------------------------------------------*/
bool _MPCF_readSectors (u32 sector, u32 numSectors, void* buffer) {
	int i;

	u16 *buff = (u16*)buffer;
#ifdef _CF_ALLOW_UNALIGNED
	u8 *buff_u8 = (u8*)buffer;
	int temp;
#endif

#if (defined _CF_USE_DMA) && (defined NDS) && (defined ARM9)
	DC_FlushRange( buffer, j * BYTES_PER_READ);
#endif

	// Wait until CF card is finished previous commands
	i=0;
	while ((REG_MPCF_CMD & CF_STS_BUSY) && (i < CF_CARD_TIMEOUT)) {
		i++;
	}
	
	// Wait until card is ready for commands
	i = 0;
	while ((!(REG_MPCF_STS & CF_STS_INSERTED)) && (i < CF_CARD_TIMEOUT)) {
		i++;
	}
	if (i >= CF_CARD_TIMEOUT)
		return false;
	
	// Set number of sectors to read
	REG_MPCF_SEC = (numSectors < 256 ? numSectors : 0);	// Read a maximum of 256 sectors, 0 means 256	
	
	// Set read sector
	REG_MPCF_LBA1 = sector & 0xFF;						// 1st byte of sector number
	REG_MPCF_LBA2 = (sector >> 8) & 0xFF;					// 2nd byte of sector number
	REG_MPCF_LBA3 = (sector >> 16) & 0xFF;				// 3rd byte of sector number
	REG_MPCF_LBA4 = ((sector >> 24) & 0x0F )| CF_CMD_LBA;	// last nibble of sector number
	
	// Set command to read
	REG_MPCF_CMD = CF_CMD_READ;
	
	
	while (numSectors--)
	{
		// Wait until card is ready for reading
		i = 0;
		while (((REG_MPCF_STS & 0xff)!= CF_STS_READY) && (i < CF_CARD_TIMEOUT))
		{
			i++;
		}
		if (i >= CF_CARD_TIMEOUT)
			return false;
		
		// Read data
#ifdef _CF_USE_DMA
 #ifdef NDS
		DMA3_SRC = (u32)MP_DATA;
		DMA3_DEST = (u32)buff;
		DMA3_CR = 256 | DMA_COPY_HALFWORDS | DMA_SRC_FIX;
 #else
		DMA3COPY ( MP_DATA, buff, 256 | DMA16 | DMA_ENABLE | DMA_SRC_FIXED);
 #endif
		buff += BYTES_PER_READ / 2;
#elif defined _CF_ALLOW_UNALIGNED
		i=256;
		if ((u32)buff_u8 & 0x01) {
			while(i--)
			{
				temp = *MP_DATA;
				*buff_u8++ = temp & 0xFF;
				*buff_u8++ = temp >> 8;
			}
		} else {
		while(i--)
			*buff++ = *MP_DATA; 
		}
#else
		i=256;
		while(i--)
			*buff++ = *MP_DATA; 
#endif
	}
#if (defined _CF_USE_DMA) && (defined NDS)
	// Wait for end of transfer before returning
	while(DMA3_CR & DMA_BUSY);
#endif
	return true;
}



/*-----------------------------------------------------------------
_MPCF_writeSectors
Write 512 byte sector numbered "sector" from "buffer"
u32 sector IN: address of 512 byte sector on CF card to read
u32 numSectors IN: number of 512 byte sectors to read,
 1 to 256 sectors can be read
void* buffer IN: pointer to 512 byte buffer to read data from
bool return OUT: true if successful
-----------------------------------------------------------------*/
bool _MPCF_writeSectors (u32 sector, u32 numSectors, void* buffer) {
	int i;

	u16 *buff = (u16*)buffer;
#ifdef _CF_ALLOW_UNALIGNED
	u8 *buff_u8 = (u8*)buffer;
	int temp;
#endif
	
#if defined _CF_USE_DMA && defined NDS && defined ARM9
	DC_FlushRange( buffer, j * BYTES_PER_READ);
#endif

	// Wait until CF card is finished previous commands
	i=0;
	while ((REG_MPCF_CMD & CF_STS_BUSY) && (i < CF_CARD_TIMEOUT))
	{
		i++;
	}
	
	// Wait until card is ready for commands
	i = 0;
	while ((!(REG_MPCF_STS & CF_STS_INSERTED)) && (i < CF_CARD_TIMEOUT))
	{
		i++;
	}
	if (i >= CF_CARD_TIMEOUT)
		return false;
	
	// Set number of sectors to write
	REG_MPCF_SEC = (numSectors < 256 ? numSectors : 0);	// Write a maximum of 256 sectors, 0 means 256	
	
	// Set write sector
	REG_MPCF_LBA1 = sector & 0xFF;						// 1st byte of sector number
	REG_MPCF_LBA2 = (sector >> 8) & 0xFF;					// 2nd byte of sector number
	REG_MPCF_LBA3 = (sector >> 16) & 0xFF;				// 3rd byte of sector number
	REG_MPCF_LBA4 = ((sector >> 24) & 0x0F )| CF_CMD_LBA;	// last nibble of sector number
	
	// Set command to write
	REG_MPCF_CMD = CF_CMD_WRITE;
	
	while (numSectors--)
	{
		// Wait until card is ready for writing
		i = 0;
		while (((REG_MPCF_STS & 0xff) != CF_STS_READY) && (i < CF_CARD_TIMEOUT))
		{
			i++;
		}
		if (i >= CF_CARD_TIMEOUT)
			return false;
		
		// Write data
#ifdef _CF_USE_DMA
 #ifdef NDS
		DMA3_SRC = (u32)buff;
		DMA3_DEST = (u32)MP_DATA;
		DMA3_CR = 256 | DMA_COPY_HALFWORDS | DMA_DST_FIX;
 #else
		DMA3COPY( buff, MP_DATA, 256 | DMA16 | DMA_ENABLE | DMA_DST_FIXED);
 #endif
		buff += BYTES_PER_READ / 2;
#elif defined _CF_ALLOW_UNALIGNED
		i=256;
		if ((u32)buff_u8 & 0x01) {
			while(i--)
			{
				temp = *buff_u8++;
				temp |= *buff_u8++ << 8;
				*MP_DATA = temp;
			}
		} else {
		while(i--)
			*MP_DATA = *buff++; 
		}
#else
		i=256;
		while(i--)
			*MP_DATA = *buff++; 
#endif
	}
#if defined _CF_USE_DMA && defined NDS
	// Wait for end of transfer before returning
	while(DMA3_CR & DMA_BUSY);
#endif
	
	return true;
}

/*-----------------------------------------------------------------
_MPCF_Shutdown
unload the GBAMP CF interface
-----------------------------------------------------------------*/
bool _MPCF_shutdown(void) {
	return _MPCF_clearStatus() ;
}

/*-----------------------------------------------------------------
_MPCF_startUp
initializes the CF interface, returns true if successful,
otherwise returns false
-----------------------------------------------------------------*/
bool _MPCF_startUp(void) {
	// See if there is a read/write register
	u16 temp = REG_MPCF_LBA1;
	REG_MPCF_LBA1 = (~temp & 0xFF);
	temp = (~temp & 0xFF);
	if (!(REG_MPCF_LBA1 == temp)) {
		return false;
	}
	// Make sure it is 8 bit
	REG_MPCF_LBA1 = 0xAA55;
	if (REG_MPCF_LBA1 == 0xAA55) {
		return false;
	}
	return true;
}

/*-----------------------------------------------------------------
the actual interface structure
-----------------------------------------------------------------*/
IO_INTERFACE _io_mpcf = {
	DEVICE_TYPE_MPCF,
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_SLOT_GBA,
	(FN_MEDIUM_STARTUP)&_MPCF_startUp,
	(FN_MEDIUM_ISINSERTED)&_MPCF_isInserted,
	(FN_MEDIUM_READSECTORS)&_MPCF_readSectors,
	(FN_MEDIUM_WRITESECTORS)&_MPCF_writeSectors,
	(FN_MEDIUM_CLEARSTATUS)&_MPCF_clearStatus,
	(FN_MEDIUM_SHUTDOWN)&_MPCF_shutdown
} ;
