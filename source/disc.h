/*
 disc.h
 Interface to the low level disc functions. Used by the higher level
 file system code.
 
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

*/
#ifndef _DISC_H
#define _DISC_H

#include "common.h"

//----------------------------------------------------------------------
// Customisable features

// Use DMA to read the card, remove this line to use normal reads/writes
// #define _IO_USE_DMA

// Allow buffers not alligned to 16 bits when reading files. 
// Note that this will slow down access speed, so only use if you have to.
// It is also incompatible with DMA
#define _IO_ALLOW_UNALIGNED

#if defined _IO_USE_DMA && defined _IO_ALLOW_UNALIGNED
 #error "You can't use both DMA and unaligned memory"
#endif

#define DEVICE_TYPE_WII	0
#define DEVICE_TYPE_GC	1

#define FEATURE_MEDIUM_CANREAD		0x00000001
#define FEATURE_MEDIUM_CANWRITE		0x00000002
#define FEATURE_SLOT_GBA			0x00000010
#define FEATURE_SLOT_NDS			0x00000020
#define FEATURE_GAMECUBE_SLOTA		0x00000010
#define FEATURE_GAMECUBE_SLOTB		0x00000020

typedef bool (* FN_MEDIUM_STARTUP)(void) ;
typedef bool (* FN_MEDIUM_ISINSERTED)(void) ;
typedef bool (* FN_MEDIUM_READSECTORS)(u32 sector, u32 numSectors, void* buffer) ;
typedef bool (* FN_MEDIUM_WRITESECTORS)(u32 sector, u32 numSectors, const void* buffer) ;
typedef bool (* FN_MEDIUM_CLEARSTATUS)(void) ;
typedef bool (* FN_MEDIUM_SHUTDOWN)(void) ;

struct IO_INTERFACE_STRUCT {
	unsigned long			ioType ;
	unsigned long			features ;
	FN_MEDIUM_STARTUP		fn_startup ;
	FN_MEDIUM_ISINSERTED	fn_isInserted ;
	FN_MEDIUM_READSECTORS	fn_readSectors ;
	FN_MEDIUM_WRITESECTORS	fn_writeSectors ;
	FN_MEDIUM_CLEARSTATUS	fn_clearStatus ;
	FN_MEDIUM_SHUTDOWN		fn_shutdown ;
} ;

typedef struct IO_INTERFACE_STRUCT IO_INTERFACE ;

/*
Search for a block based device in all available slots.
Return a pointer to a usable interface if one is found,
NULL if not.
*/
extern const IO_INTERFACE* _FAT_disc_findInterface (void);

/*
Search for a block based device in a specific slot.
Return a pointer to a usable interface if one is found,
NULL if not.
*/
extern const IO_INTERFACE* _FAT_disc_findInterfaceSlot (PARTITION_INTERFACE partitionNumber);

/*
Check if a disc is inserted
Return true if a disc is inserted and ready, false otherwise
*/
static inline bool _FAT_disc_isInserted (const IO_INTERFACE* disc) {
	return disc->fn_isInserted();
}

/*
Read numSectors sectors from a disc, starting at sector. 
numSectors is between 1 and 256
sector is from 0 to 2^28
buffer is a pointer to the memory to fill
*/
static inline bool _FAT_disc_readSectors (const IO_INTERFACE* disc, u32 sector, u32 numSectors, void* buffer) {
	return disc->fn_readSectors (sector, numSectors, buffer);
}

/*
Write numSectors sectors to a disc, starting at sector. 
numSectors is between 1 and 256
sector is from 0 to 2^28
buffer is a pointer to the memory to read from
*/
static inline bool _FAT_disc_writeSectors (const IO_INTERFACE* disc, u32 sector, u32 numSectors, const void* buffer) {
	return disc->fn_writeSectors (sector, numSectors, buffer);
}

/*
Reset the card back to a ready state
*/
static inline bool _FAT_disc_clearStatus (const IO_INTERFACE* disc) {
	return disc->fn_clearStatus();
}

/*
Initialise the disc to a state ready for data reading or writing
*/
static inline bool _FAT_disc_startup (const IO_INTERFACE* disc) {
	return disc->fn_startup();
}

/*
Put the disc in a state ready for power down.
Complete any pending writes and disable the disc if necessary
*/
static inline bool _FAT_disc_shutdown (const IO_INTERFACE* disc) {
	return disc->fn_shutdown();
}

/*
Return a 32 bit value unique to each type of interface
*/
static inline u32 _FAT_disc_hostType (const IO_INTERFACE* disc) {
	return disc->ioType;
}

/*
Return a 32 bit value that specifies the capabilities of the disc
*/
static inline u32 _FAT_disc_features (const IO_INTERFACE* disc) {
	return disc->features;
}

#endif // _DISC_H
