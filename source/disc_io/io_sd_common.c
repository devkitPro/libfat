/*
	io_sd_common.c

	By chishm (Michael Chisholm)

	Common SD card routines

	SD routines partially based on sd.s by Romman 

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

#include "io_sd_common.h"

/*
Improved CRC7 function provided by cory1492
Calculates the CRC of an SD command, and includes the end bit in the byte
*/
u8 _SD_CRC7(u8* data, int cnt) {
    int i, a;
    u8 crc, temp;

    crc = 0;
    for (a = 0; a < cnt; a++)
    {
        temp = data[a];
        for (i = 0; i < 8; i++)
        {
            crc <<= 1;
            if ((temp & 0x80) ^ (crc & 0x80)) crc ^= 0x09;
            temp <<= 1;
        }
    }
    crc = (crc << 1) | 1;
    return(crc);
} 

/*
Calculates the CRC16 for a sector of data. Calculates it 
as 4 separate lots, merged into one buffer. This is used
for 4 SD data lines, not for 1 data line alone.
*/
void _SD_CRC16 (u8* buff, int buffLength, u8* crc16buff) {
	u32 a, b, c, d;
	int count;
	u32 bitPattern = 0x80808080;	// r7
	u32 crcConst = 0x1021;	// r8
	u32 dataByte = 0;	// r2

	a = 0;	// r3
	b = 0;	// r4
	c = 0;	// r5
	d = 0;	// r6
	
	buffLength = buffLength * 8;
	
	
	do {
		if (bitPattern & 0x80) dataByte = *buff++;
		
		a = a << 1;
		if ( a & 0x10000) a ^= crcConst;
		if (dataByte & (bitPattern >> 24)) a ^= crcConst;
		
		b = b << 1;
		if (b & 0x10000) b ^= crcConst;
		if (dataByte & (bitPattern >> 25)) b ^= crcConst;
	
		c = c << 1;
		if (c & 0x10000) c ^= crcConst;
		if (dataByte & (bitPattern >> 26)) c ^= crcConst;
		
		d = d << 1;
		if (d & 0x10000) d ^= crcConst;
		if (dataByte & (bitPattern >> 27)) d ^= crcConst;
		
		bitPattern = (bitPattern >> 4) | (bitPattern << 28);
	} while (buffLength-=4);
	
	count = 16;	// r8
	
	do {
		bitPattern = bitPattern << 4;
		if (a & 0x8000) bitPattern |= 8;
		if (b & 0x8000) bitPattern |= 4;
		if (c & 0x8000) bitPattern |= 2;
		if (d & 0x8000) bitPattern |= 1;
	
		a = a << 1;
		b = b << 1;
		c = c << 1;
		d = d << 1;
		
		count--;
		
		if (!(count & 0x01)) {
			*crc16buff++ = (u8)(bitPattern & 0xff);
		}
	} while (count != 0);
	
	return;
}

