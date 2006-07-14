/*
	io_scsd.c 

	Hardware Routines for reading a Secure Digital card
	using the SC SD
	
	Some code based on scsd_c.c, written by Amadeus 
	and Jean-Pierre Thomasset as part of DSLinux.

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

#include "io_scsd.h"
#include "io_sd_common.h"
#include "io_sc_common.h"

//---------------------------------------------------------------
// SCSD register addresses
#define REG_SCSD_CMD	(*(vu16*)(0x09800000))
	/* bit 0: command bit to read  		*/
	/* bit 7: command bit to write 		*/

#define REG_SCSD_DATAWRITE (*(vu16*)(0x09000000))
#define REG_SCSD_DATAREAD  (*(vu16*)(0x09100000))
#define REG_SCSD_DATAREAD_32  (*(vu32*)(0x09100000))
#define REG_SCSD_LOCK      (*(vu16*)(0x09FFFFFE))
	/* bit 0: 1				*/
	/* bit 1: enable IO interface (SD,CF)	*/
	/* bit 2: enable R/W SDRAM access 	*/

//---------------------------------------------------------------
// Responses
#define SCSD_STS_BUSY			0x100
#define SCSD_STS_INSERTED		0x300

//---------------------------------------------------------------
// Send / receive timeouts, to stop infinite wait loops
#define MAX_STARTUP_TRIES 100	// Arbitrary value, check if the card is ready 100 times before giving up
#define NUM_STARTUP_CLOCKS 100	// Number of empty (0xFF when sending) bytes to send/receive to/from the card
#define TRANSMIT_TIMEOUT 0x100	// Time to wait for the M3 to respond to transmit or receive requests
#define RESPONSE_TIMEOUT 256	// Number of clocks sent to the SD card before giving up
#define BUSY_WAIT_TIMEOUT 500000
//---------------------------------------------------------------
// Variables required for tracking SD state
static u32 relativeCardAddress = 0;	// Preshifted Relative Card Address

//---------------------------------------------------------------
// Internal M3 SD functions

static inline void _SCSD_unlock (void) {
	_SC_changeMode (SC_MODE_MEDIA);	
}

static bool _SCSD_sendCommand (u8 command, u32 argument) {
	u8 databuff[6];
	u8 *tempDataPtr = databuff;
	int length = 6;
	u16 dataByte;
	int curBit;
	int i;

	*tempDataPtr++ = command | 0x40;
	*tempDataPtr++ = argument>>24;
	*tempDataPtr++ = argument>>16;
	*tempDataPtr++ = argument>>8;
	*tempDataPtr++ = argument;
	*tempDataPtr = _SD_CRC7 (databuff, 5);

	i = BUSY_WAIT_TIMEOUT;
	while (((REG_SCSD_CMD & 0x01) == 0) && (--i));
	if (i == 0) {
		return false;
	}
		
	dataByte = REG_SCSD_CMD;

	tempDataPtr = databuff;
	
	do {
		dataByte = *tempDataPtr++;
		for (curBit = 7; curBit >=0; curBit--){
			REG_SCSD_CMD = dataByte;
			dataByte = dataByte << 1;
		}
	} while (length--);
	
	return true;
}

static u8 _SCSD_getByte (void) {
	// With every 16 bit read to REG_SCSD_CMD, read a single bit.
	u32 res = 0;
	int i;
	
	for (i = 1; i < 8; i++) {
		res = (res << 1) | REG_SCSD_CMD;
	}
	
	return (u8)res;
}

// Returns the response from the SD card to a previous command.
static bool _SCSD_getResponse (u8* dest, u32 length) {
	u32 i;	
	u8 dataByte;
	int shiftAmount;
	
	// Wait for the card to be non-busy
	for (i = 0; i < RESPONSE_TIMEOUT; i++) {
		dataByte = _SCSD_getByte();
		if (dataByte != SD_CARD_BUSY) {
			break;
		}
	}
	
	if (dest == NULL) {
		return true;
	}
	
	// Still busy after the timeout has passed
	if (dataByte == 0xff) {
		return false;
	}

	// Read response into buffer
	for ( i = 0; i < length; i++) {
		dest[i] = dataByte;
		dataByte = _SCSD_getByte();
	}
	// dataByte will contain the last piece of the response

	// Send 16 more clocks, 8 more than the delay required between a response and the next command
	i = _SCSD_getByte();
	i = _SCSD_getByte();
	
	// Shift response so that the bytes are correctly aligned
	// The register may not contain properly aligned data
	for (shiftAmount = 0; ((dest[0] << shiftAmount) & 0x80) != 0x00; shiftAmount++) {
		if (shiftAmount >= 7) {
			return false;
		}
	}
	
	for (i = 0; i < length - 1; i++) {
		dest[i] = (dest[i] << shiftAmount) | (dest[i+1] >> (8-shiftAmount));
	}
	// Get the last piece of the response from dataByte
	dest[i] = (dest[i] << shiftAmount) | (dataByte >> (8-shiftAmount));

	return true;
}


static inline bool _SCSD_getResponse_R1 (u8* dest) {
	return _SCSD_getResponse (dest, 6);
}

static inline bool _SCSD_getResponse_R1b (u8* dest) {
	return _SCSD_getResponse (dest, 6);
}

static inline bool _SCSD_getResponse_R2 (u8* dest) {
	return _SCSD_getResponse (dest, 17);
}

static inline bool _SCSD_getResponse_R3 (u8* dest) {
	return _SCSD_getResponse (dest, 6);
}

static inline bool _SCSD_getResponse_R6 (u8* dest) {
	return _SCSD_getResponse (dest, 6);
}

static void _SCSD_sendClocks (u32 numClocks) {
	u16 temp;
	do {
		temp = REG_SCSD_CMD;
	} while (numClocks--);
}

static bool _SCSD_initCard (void) {
//iprintf ("init card\n");
	int i;
	u8 responseBuffer[17];		// sizeof 17 to hold the maximum response size possible
	
	// Give the card time to stabilise
	_SCSD_sendClocks (NUM_STARTUP_CLOCKS);
	
	// Reset the card
	if (!_SCSD_sendCommand (GO_IDLE_STATE, 0)) {
//iprintf ("can't idle\n");
		return false;
	}

	_SCSD_sendClocks (NUM_STARTUP_CLOCKS);
	
	// Card is now reset, including it's address
	relativeCardAddress = 0;

	for (i = 0; i < MAX_STARTUP_TRIES ; i++) {
		_SCSD_sendCommand (APP_CMD, 0);
		_SCSD_getResponse_R1 (responseBuffer);
	
		_SCSD_sendCommand (SD_APP_OP_COND, 3<<16);
		if ((_SCSD_getResponse_R3 (responseBuffer)) && ((responseBuffer[1] & 0x80) != 0)) {	
			// Card is ready to receive commands now
			break;
		}
	}
	
	if (i >= MAX_STARTUP_TRIES) {
//iprintf ("timeout on OP_COND\n");
		return false;
	}
	
	// The card's name, as assigned by the manufacturer
	_SCSD_sendCommand (ALL_SEND_CID, 0);
	_SCSD_getResponse_R2 (responseBuffer);
	
	// Get a new address
	_SCSD_sendCommand (SEND_RELATIVE_ADDR, 0);
	_SCSD_getResponse_R6 (responseBuffer);
	relativeCardAddress = (responseBuffer[1] << 24) | (responseBuffer[2] << 16);
//iprintf ("Relative Address: %08X\n", relativeCardAddress);
	
	// Some cards won't go to higher speeds unless they think you checked their capabilities
	_SCSD_sendCommand (SEND_CSD, relativeCardAddress);
	_SCSD_getResponse_R2 (responseBuffer);
	
	// Only this card should respond to all future commands
	_SCSD_sendCommand (SELECT_CARD, relativeCardAddress);
	_SCSD_getResponse_R1 (responseBuffer);
	
	// Set a 4 bit data bus
	_SCSD_sendCommand (APP_CMD, relativeCardAddress);
	_SCSD_getResponse_R1 (responseBuffer);
	
	_SCSD_sendCommand (SET_BUS_WIDTH, 2);
	_SCSD_getResponse_R1 (responseBuffer);
		
	// Use 512 byte blocks
	_SCSD_sendCommand (SET_BLOCKLEN, BYTES_PER_READ);
	_SCSD_getResponse_R1 (responseBuffer);

	// Wait until card is ready for data
	i = 0;
	do {
		if (i >= RESPONSE_TIMEOUT) {
//iprintf ("timeout on SEND_STATUS\n");
			return false;
		}
		i++;
		_SCSD_sendCommand (SEND_STATUS, relativeCardAddress);
	} while ((!_SCSD_getResponse_R1 (responseBuffer)) && ((responseBuffer[3] & 0x1f) != ((SD_STATE_TRAN << 1) | READY_FOR_DATA)));
	
	return true;
}

static bool _SCSD_readData (void* buffer) {
	u8* buff_u8 = (u8*)buffer;
	u16* buff = (u16*)buffer;
	u32 temp;
	int i;
	
	i = BUSY_WAIT_TIMEOUT;
	while ((REG_SCSD_DATAREAD & SCSD_STS_BUSY) && (--i));
	if (i == 0) {
		return false;
	}

	i=256;
	if ((u32)buff_u8 & 0x01) {
		while(i--)
		{
			temp = REG_SCSD_DATAREAD_32;
			temp = REG_SCSD_DATAREAD_32 >> 16;
			*buff_u8++ = (u8)temp;
			*buff_u8++ = (u8)(temp >> 8);
		}
	} else {
		while(i--)
			temp = REG_SCSD_DATAREAD_32;
			temp = REG_SCSD_DATAREAD_32 >> 16;
			*buff++ = (u16)temp; 
	}

	
	for (i = 0; i < 8; i++) {
		temp = REG_SCSD_DATAREAD_32;
	}
	
	temp = REG_SCSD_DATAREAD;
	
	return true;
}

static bool _SCSD_writeData (u8* data, u8* crc) {
	int pos;
	u16 dataHWord;
	u16 temp;
	
	while ((REG_SCSD_DATAREAD & SCSD_STS_BUSY) == 0);
		
	temp = REG_SCSD_DATAREAD;
	
	REG_SCSD_DATAWRITE = 0;		// start bit;
		
	pos = BYTES_PER_READ / 2;
	while (pos--) {
		dataHWord = data[0] | (data[1] << 8);
		data+=2;
		
		REG_SCSD_DATAWRITE = dataHWord;
		REG_SCSD_DATAWRITE = dataHWord << 4;
		REG_SCSD_DATAWRITE = dataHWord << 8;
		REG_SCSD_DATAWRITE = dataHWord << 12;
	}
	
	if (crc != 0) {
		pos = 4;
		while (pos--) {
			dataHWord = *crc++;
			
			REG_SCSD_DATAWRITE = dataHWord;
			REG_SCSD_DATAWRITE = dataHWord << 4;
			REG_SCSD_DATAWRITE = dataHWord << 8;
			REG_SCSD_DATAWRITE = dataHWord << 12;
		}
	}
	
	REG_SCSD_DATAWRITE = 0xff;		// end bit
	
	while ((REG_SCSD_DATAREAD & SCSD_STS_BUSY) == 0);

	temp = REG_SCSD_DATAREAD;
	temp = REG_SCSD_DATAREAD;
	temp = REG_SCSD_DATAREAD;
	temp = REG_SCSD_DATAREAD;

	return true;
}

//---------------------------------------------------------------
// Functions needed for the external interface

bool _SCSD_startUp (void) {
	_SCSD_unlock();
	return _SCSD_initCard();
}

bool _SCSD_isInserted (void) {
	u8 responseBuffer [6];
	// Make sure the card receives the command
	if (!_SCSD_sendCommand (SEND_STATUS, 0)) {
		return false;
	}
	// Make sure the card responds
	if (!_SCSD_getResponse_R1 (responseBuffer)) {
		return false;
	}
	// Make sure the card responded correctly
	if (responseBuffer[0] != SEND_STATUS) {
		return false;
	}
	return true;
}

bool _SCSD_readSectors (u32 sector, u32 numSectors, void* buffer) {
	u32 i;
	u8* dest = (u8*) buffer;
	u8 responseBuffer[6];
	
	if (numSectors == 1) {
		// If it's only reading one sector, use the (slightly faster) READ_SINGLE_BLOCK
		if (!_SCSD_sendCommand (READ_SINGLE_BLOCK, sector * BYTES_PER_READ)) {
			return false;
		}

		if (!_SCSD_readData (buffer)) {
			return false;
		}

	} else {
		// Stream the required number of sectors from the card
		if (!_SCSD_sendCommand (READ_MULTIPLE_BLOCK, sector * BYTES_PER_READ)) {
			return false;
		}
	
		for(i=0; i < numSectors; i++, dest+=BYTES_PER_READ) {
			if (!_SCSD_readData(dest)) {
				return false;
			}
		}
	
		// Stop the streaming
		_SCSD_sendCommand (STOP_TRANSMISSION, 0);
		_SCSD_getResponse_R1b (responseBuffer);
	}

	_SCSD_sendClocks(0x10);
	return true;
}

bool _SCSD_writeSectors (u32 sector, u32 numSectors, const void* buffer) {
	u16 crcBuff[4];
	u8* crc = (u8*)crcBuff;	// Force crcBuff to be halfword aligned
	u8 responseBuffer[6];
	u32 offset = sector * BYTES_PER_READ;
	u8* data = (u8*) buffer;

	while (numSectors--) {
		// Send write command and get a response
		_SCSD_sendCommand (WRITE_BLOCK, offset);
		if (!_SCSD_getResponse_R1 (responseBuffer)) {
			return false;
		}
	
		// Send the data and CRC
		_SD_CRC16 ( data, BYTES_PER_READ, crc);
		if (! _SCSD_writeData( data, crc)) {
			return false;
		}

		// Send a few clocks to the SD card
		_SCSD_sendClocks(0x10);
		
		offset += BYTES_PER_READ;
		data += BYTES_PER_READ;
	}
	
	return true;

}

bool _SCSD_clearStatus (void) {
	return _SCSD_initCard ();
}

bool _SCSD_shutdown (void) {
	_SC_changeMode (SC_MODE_RAM_RO);
	return true;
}

IO_INTERFACE _io_scsd = {
	DEVICE_TYPE_SCSD,
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_SLOT_GBA,
	(FN_MEDIUM_STARTUP)&_SCSD_startUp,
	(FN_MEDIUM_ISINSERTED)&_SCSD_isInserted,
	(FN_MEDIUM_READSECTORS)&_SCSD_readSectors,
	(FN_MEDIUM_WRITESECTORS)&_SCSD_writeSectors,
	(FN_MEDIUM_CLEARSTATUS)&_SCSD_clearStatus,
	(FN_MEDIUM_SHUTDOWN)&_SCSD_shutdown
} ;


