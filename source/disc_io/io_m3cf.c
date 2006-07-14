/*
	io_m3cf.c based on

	compact_flash.c
	By chishm (Michael Chisholm)

	Hardware Routines for reading a compact flash card
	using the M3 Perfect CF Adapter

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


#include "io_m3cf.h"
#include "io_m3_common.h"
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

// M3 CF Addresses
#define REG_M3CF_STS		(*(vu16*)0x080C0000)	// Status of the CF Card / Device control
#define REG_M3CF_CMD		(*(vu16*)0x088E0000)	// Commands sent to control chip and status return
#define REG_M3CF_ERR		(*(vu16*)0x08820000)	// Errors / Features

#define REG_M3CF_SEC		(*(vu16*)0x08840000)	// Number of sector to transfer
#define REG_M3CF_LBA1		(*(vu16*)0x08860000)	// 1st byte of sector address
#define REG_M3CF_LBA2		(*(vu16*)0x08880000)	// 2nd byte of sector address
#define REG_M3CF_LBA3		(*(vu16*)0x088A0000)	// 3rd byte of sector address
#define REG_M3CF_LBA4		(*(vu16*)0x088C0000)	// last nibble of sector address | 0xE0

#define M3_DATA			((vu16*)0x08800000)		// Pointer to buffer of CF data transered from card


/*-----------------------------------------------------------------
_M3CF_isInserted
Is a compact flash card inserted?
bool return OUT:  true if a CF card is inserted
-----------------------------------------------------------------*/
bool _M3CF_isInserted (void) 
{
	// Change register, then check if value did change
	REG_M3CF_STS = CF_STS_INSERTED;
	return ((REG_M3CF_STS & 0xff) == CF_STS_INSERTED);
}


/*-----------------------------------------------------------------
_M3CF_clearStatus
Tries to make the CF card go back to idle mode
bool return OUT:  true if a CF card is idle
-----------------------------------------------------------------*/
bool _M3CF_clearStatus (void) 
{
	int i;
	
	// Wait until CF card is finished previous commands
	i=0;
	while ((REG_M3CF_CMD & CF_STS_BUSY) && (i < CF_CARD_TIMEOUT))
	{
		i++;
	}
	
	// Wait until card is ready for commands
	i = 0;
	while ((!(REG_M3CF_STS & CF_STS_INSERTED)) && (i < CF_CARD_TIMEOUT))
	{
		i++;
	}
	if (i >= CF_CARD_TIMEOUT)
		return false;

	return true;
}


/*-----------------------------------------------------------------
_M3CF_readSectors
Read 512 byte sector numbered "sector" into "buffer"
u32 sector IN: address of first 512 byte sector on CF card to read
u32 numSectors IN: number of 512 byte sectors to read,
 1 to 256 sectors can be read
void* buffer OUT: pointer to 512 byte buffer to store data in
bool return OUT: true if successful
-----------------------------------------------------------------*/
bool _M3CF_readSectors (u32 sector, u32 numSectors, void* buffer)
{
	int i;

	u16 *buff = (u16*)buffer;
#ifdef _CF_ALLOW_UNALIGNED
	u8 *buff_u8 = (u8*)buffer;
	int temp;
#endif
	
#if defined _CF_USE_DMA && defined NDS && defined ARM9
	DC_FlushRange( buffer, numSectors * BYTES_PER_READ);
#endif

	// Wait until CF card is finished previous commands
	i=0;
	while ((REG_M3CF_CMD & CF_STS_BUSY) && (i < CF_CARD_TIMEOUT))
	{
		i++;
	}
	
	// Wait until card is ready for commands
	i = 0;
	while ((!(REG_M3CF_STS & CF_STS_INSERTED)) && (i < CF_CARD_TIMEOUT))
	{
		i++;
	}
	if (i >= CF_CARD_TIMEOUT)
		return false;
	
	// Set number of sectors to read
	REG_M3CF_SEC = (numSectors < 256 ? numSectors : 0);	// Read a maximum of 256 sectors, 0 means 256
	
	// Set read sector
	REG_M3CF_LBA1 = sector & 0xFF;						// 1st byte of sector number
	REG_M3CF_LBA2 = (sector >> 8) & 0xFF;					// 2nd byte of sector number
	REG_M3CF_LBA3 = (sector >> 16) & 0xFF;				// 3rd byte of sector number
	REG_M3CF_LBA4 = ((sector >> 24) & 0x0F )| CF_CMD_LBA;	// last nibble of sector number
	
	// Set command to read
	REG_M3CF_CMD = CF_CMD_READ;
	
	
	while (numSectors--)
	{
		// Wait until card is ready for reading
		i = 0;
		while (((REG_M3CF_STS & 0xff) != CF_STS_READY) && (i < CF_CARD_TIMEOUT))
		{
			i++;
		}
		if (i >= CF_CARD_TIMEOUT)
			return false;
		
		// Read data
#ifdef _CF_USE_DMA
 #ifdef NDS
		DMA3_SRC = (u32)M3_DATA;
		DMA3_DEST = (u32)buff;
		DMA3_CR = 256 | DMA_COPY_HALFWORDS | DMA_SRC_FIX;
 #else
		DMA3COPY ( M3_DATA, buff, 256 | DMA16 | DMA_ENABLE | DMA_SRC_FIXED);
 #endif
		buff += BYTES_PER_READ / 2;
#elif defined _CF_ALLOW_UNALIGNED
		i=256;
		if ((u32)buff_u8 & 0x01) {
			while(i--)
			{
				temp = *M3_DATA;
				*buff_u8++ = temp & 0xFF;
				*buff_u8++ = temp >> 8;
			}
		} else {
		while(i--)
			*buff++ = *M3_DATA; 
		}
#else
		i=256;
		while(i--)
			*buff++ = *M3_DATA; 
#endif
	}
#if defined _CF_USE_DMA && defined NDS
	// Wait for end of transfer before returning
	while(DMA3_CR & DMA_BUSY);
#endif

	return true;
}



/*-----------------------------------------------------------------
_M3CF_writeSectors
Write 512 byte sector numbered "sector" from "buffer"
u32 sector IN: address of 512 byte sector on CF card to read
u32 numSecs IN: number of 512 byte sectors to read,
 1 to 256 sectors can be read, 0 = 256
void* buffer IN: pointer to 512 byte buffer to read data from
bool return OUT: true if successful
-----------------------------------------------------------------*/
bool _M3CF_writeSectors (u32 sector, u32 numSectors, void* buffer)
{
	int i;

	u16 *buff = (u16*)buffer;
#ifdef _CF_ALLOW_UNALIGNED
	u8 *buff_u8 = (u8*)buffer;
	int temp;
#endif
	
#if defined _CF_USE_DMA && defined NDS && defined ARM9
	DC_FlushRange( buffer, numSectors * BYTES_PER_READ);
#endif

	// Wait until CF card is finished previous commands
	i=0;
	while ((REG_M3CF_CMD & CF_STS_BUSY) && (i < CF_CARD_TIMEOUT))
	{
		i++;
	}
	
	// Wait until card is ready for commands
	i = 0;
	while ((!(REG_M3CF_STS & CF_STS_INSERTED)) && (i < CF_CARD_TIMEOUT))
	{
		i++;
	}
	if (i >= CF_CARD_TIMEOUT)
		return false;
	
	// Set number of sectors to write
	REG_M3CF_SEC = (numSectors < 256 ? numSectors : 0);	// Max of 256, 0 means 256
	
	// Set write sector
	REG_M3CF_LBA1 = sector & 0xFF;						// 1st byte of sector number
	REG_M3CF_LBA2 = (sector >> 8) & 0xFF;					// 2nd byte of sector number
	REG_M3CF_LBA3 = (sector >> 16) & 0xFF;				// 3rd byte of sector number
	REG_M3CF_LBA4 = ((sector >> 24) & 0x0F )| CF_CMD_LBA;	// last nibble of sector number
	
	// Set command to write
	REG_M3CF_CMD = CF_CMD_WRITE;
	
	while (numSectors--)
	{
		// Wait until card is ready for writing
		i = 0;
		while (((REG_M3CF_STS & 0xff) != CF_STS_READY) && (i < CF_CARD_TIMEOUT))
		{
			i++;
		}
		if (i >= CF_CARD_TIMEOUT)
			return false;
		
		// Write data
#ifdef _CF_USE_DMA
 #ifdef NDS
		DMA3_SRC = (u32)buff;
		DMA3_DEST = (u32)M3_DATA;
		DMA3_CR = 256 | DMA_COPY_HALFWORDS | DMA_DST_FIX;
 #else
		DMA3COPY( buff, M3_DATA, 256 | DMA16 | DMA_ENABLE | DMA_DST_FIXED);
 #endif
		buff += BYTES_PER_READ / 2;
#elif defined _CF_ALLOW_UNALIGNED
		i=256;
		if ((u32)buff_u8 & 0x01) {
			while(i--)
			{
				temp = *buff_u8++;
				temp |= *buff_u8++ << 8;
				*M3_DATA = temp;
			}
		} else {
		while(i--)
			*M3_DATA = *buff++; 
		}
#else
		i=256;
		while(i--)
			*M3_DATA = *buff++; 
#endif
	}
#if defined _CF_USE_DMA && defined NDS
	// Wait for end of transfer before returning
	while(DMA3_CR & DMA_BUSY);
#endif
	
	return true;
}


/*-----------------------------------------------------------------
M3_Unlock
Returns true if M3 was unlocked, false if failed
Added by MightyMax
-----------------------------------------------------------------*/
static bool _M3CF_unlock(void) 
{
	u16 temp;
	_M3_changeMode (M3_MODE_MEDIA);
	// test that we have register access
	temp = REG_M3CF_LBA1;
	temp = (~temp & 0xFF);
	REG_M3CF_LBA1 = temp;
	// did it change?
	return (REG_M3CF_LBA1 == temp) ;
}

bool _M3CF_shutdown(void) {
	if ( !_M3CF_clearStatus() ) {
		return false;
	}
	_M3_changeMode (M3_MODE_ROM);
	return true;
}

bool _M3CF_startUp(void) {
	return _M3CF_unlock() ;
}


IO_INTERFACE _io_m3cf = {
	DEVICE_TYPE_M3CF,
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_SLOT_GBA,
	(FN_MEDIUM_STARTUP)&_M3CF_startUp,
	(FN_MEDIUM_ISINSERTED)&_M3CF_isInserted,
	(FN_MEDIUM_READSECTORS)&_M3CF_readSectors,
	(FN_MEDIUM_WRITESECTORS)&_M3CF_writeSectors,
	(FN_MEDIUM_CLEARSTATUS)&_M3CF_clearStatus,
	(FN_MEDIUM_SHUTDOWN)&_M3CF_shutdown
} ;
