/*
 cache.c
 The cache is not visible to the user. It should be flushed
 when any file is closed or changes are made to the filesystem.

 This cache implements a least-used-page replacement policy. This will
 distribute sectors evenly over the pages, so if less than the maximum
 pages are used at once, they should all eventually remain in the cache.
 This also has the benefit of throwing out old sectors, so as not to keep
 too many stale pages around.

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

#include <string.h>
#include <limits.h>

#include "common.h"
#include "cache.h"
#include "disc.h"

#include "mem_allocate.h"
#include "bit_ops.h"
#include "file_allocation_table.h"

#define CACHE_FREE UINT_MAX

CACHE* _FAT_cache_constructor (unsigned int numberOfPages, unsigned int sectorsPerPage, const DISC_INTERFACE* discInterface) {
	CACHE* cache;
	unsigned int i;
	CACHE_ENTRY* cacheEntries;

	if (numberOfPages < 2) {
		numberOfPages = 2;
	}

	if (sectorsPerPage < 8) {
		sectorsPerPage = 8;
	}

	cache = (CACHE*) _FAT_mem_allocate (sizeof(CACHE));
	if (cache == NULL) {
		return NULL;
	}

	cache->disc = discInterface;
	cache->numberOfPages = numberOfPages;
	cache->sectorsPerPage = sectorsPerPage;


	cacheEntries = (CACHE_ENTRY*) _FAT_mem_allocate ( sizeof(CACHE_ENTRY) * numberOfPages);
	if (cacheEntries == NULL) {
		_FAT_mem_free (cache);
		return NULL;
	}

	for (i = 0; i < numberOfPages; i++) {
		cacheEntries[i].sector = CACHE_FREE;
		cacheEntries[i].count = 0;
		cacheEntries[i].last_access = 0;
		cacheEntries[i].dirty = false;
		cacheEntries[i].cache = (uint8_t*) _FAT_mem_align ( sectorsPerPage * BYTES_PER_READ );
	}

	cache->cacheEntries = cacheEntries;

	return cache;
}

void _FAT_cache_destructor (CACHE* cache) {
	unsigned int i;
	// Clear out cache before destroying it
	_FAT_cache_flush(cache);

	// Free memory in reverse allocation order
	for (i = 0; i < cache->numberOfPages; i++) {
		_FAT_mem_free (cache->cacheEntries[i].cache);
	}
	_FAT_mem_free (cache->cacheEntries);
	_FAT_mem_free (cache);
}

static u32 accessCounter = 0;

static u32 accessTime(){
	accessCounter++;
	return accessCounter;
}

/*
Retrieve a sector's page from the cache. If it is not found in the cache,
load it into the cache and return the page it was loaded to.
Return CACHE_FREE on error.
*/
static unsigned int _FAT_cache_getSector (CACHE* cache, sec_t sector, void* buffer) {
	unsigned int i;
	CACHE_ENTRY* cacheEntries = cache->cacheEntries;
	unsigned int numberOfPages = cache->numberOfPages;
	unsigned int sectorsPerPage = cache->sectorsPerPage;

	unsigned int oldUsed = 0;
	unsigned int oldAccess = cacheEntries[0].last_access;

	for (i = 0; i < numberOfPages ; i++) {
		if ( sector>=cacheEntries[i].sector && sector < cacheEntries[i].sector+cacheEntries[i].count) {
			cacheEntries[i].last_access = accessTime();
			memcpy(buffer, cacheEntries[i].cache + ((sector - cacheEntries[i].sector)*BYTES_PER_READ), BYTES_PER_READ);
			return true;
		}
		// While searching for the desired sector, also search for the least recently used page
		if ( (cacheEntries[i].sector == CACHE_FREE) || (cacheEntries[i].last_access < oldAccess) ) {
			oldUsed = i;
			oldAccess = cacheEntries[i].last_access;
		}
	}


	// If it didn't, replace the least used cache page with the desired sector
	if ((cacheEntries[oldUsed].sector != CACHE_FREE) && (cacheEntries[oldUsed].dirty == true)) {
		// Write the page back to disc if it has been written to
		if (!_FAT_disc_writeSectors (cache->disc, cacheEntries[oldUsed].sector, cacheEntries[oldUsed].count, cacheEntries[oldUsed].cache)) {
			return false;
		}
		cacheEntries[oldUsed].dirty = false;
	}

	// Load the new sector into the cache
	if (!_FAT_disc_readSectors (cache->disc, sector, sectorsPerPage, cacheEntries[oldUsed].cache)) {
		return false;
	}
	cacheEntries[oldUsed].sector = sector;
	cacheEntries[oldUsed].count = sectorsPerPage;
	// Increment the usage count, don't reset it
	// This creates a paging policy of least recently used PAGE, not sector
	cacheEntries[oldUsed].last_access = accessTime();
	memcpy(buffer, cacheEntries[oldUsed].cache, BYTES_PER_READ);
	return true;
}

bool _FAT_cache_getSectors (CACHE* cache, sec_t sector, sec_t numSectors, void* buffer) {
	unsigned int i;
	CACHE_ENTRY* cacheEntries = cache->cacheEntries;
	unsigned int numberOfPages = cache->numberOfPages;
	sec_t sec;
	sec_t secs_to_read;

	unsigned int oldUsed = 0;
	unsigned int oldAccess = cacheEntries[0].last_access;

	while(numSectors>0)
	{
		i=0;
		while (i < numberOfPages ) {
			if ( sector>=cacheEntries[i].sector && sector < cacheEntries[i].sector+cacheEntries[i].count) {
				sec=sector-cacheEntries[i].sector;
				secs_to_read=cacheEntries[i].count-sec;
				if(secs_to_read>numSectors)secs_to_read=numSectors;
				memcpy(buffer,cacheEntries[i].cache + (sec*BYTES_PER_READ), secs_to_read*BYTES_PER_READ);
				cacheEntries[i].last_access = accessTime();
				numSectors=numSectors-secs_to_read;
				if(numSectors==0) return true;
				buffer+=secs_to_read*BYTES_PER_READ;
				sector+=secs_to_read;
				i=-1; // recheck all pages again
				oldUsed = 0;
				oldAccess = cacheEntries[0].last_access;

			}
			else // While searching for the desired sector, also search for the least recently used page
			if ( (cacheEntries[i].sector == CACHE_FREE) || (cacheEntries[i].last_access < oldAccess) ) {
				oldUsed = i;
				oldAccess = cacheEntries[i].last_access;
			}
			i++;
	    }
		// If it didn't, replace the least recently used cache page with the desired sector
		if ((cacheEntries[oldUsed].sector != CACHE_FREE) && (cacheEntries[oldUsed].dirty == true)) {
			// Write the page back to disc if it has been written to
			if (!_FAT_disc_writeSectors (cache->disc, cacheEntries[oldUsed].sector, cacheEntries[oldUsed].count, cacheEntries[oldUsed].cache)) {
				return false;
			}
			cacheEntries[oldUsed].dirty = false;
		}

		cacheEntries[oldUsed].sector = sector;
		cacheEntries[oldUsed].count = cache->sectorsPerPage;

		if (!_FAT_disc_readSectors (cache->disc, sector, cacheEntries[oldUsed].count,  cacheEntries[oldUsed].cache)) {
			return false;
		}

		// Increment the usage count, don't reset it
		// This creates a paging policy of least used PAGE, not sector
		cacheEntries[oldUsed].last_access = accessTime();

		sec=0;
		secs_to_read=cacheEntries[oldUsed].count-sec;
		if(secs_to_read>numSectors)secs_to_read=numSectors;
		memcpy(buffer,cacheEntries[oldUsed].cache + (sec*BYTES_PER_READ), secs_to_read*BYTES_PER_READ);
		numSectors=numSectors-secs_to_read;
		if(numSectors==0) return true;
		buffer+=secs_to_read*BYTES_PER_READ;

		sector+=secs_to_read;
		oldUsed = 0;
		oldAccess = cacheEntries[0].last_access;
	}
	return false;
}

/*
Reads some data from a cache page, determined by the sector number
*/
bool _FAT_cache_readPartialSector (CACHE* cache, void* buffer, sec_t sector, unsigned int offset, size_t size) {
	void* sec;

	if (offset + size > BYTES_PER_READ) {
		return false;
	}
	sec = (void*) _FAT_mem_align ( BYTES_PER_READ );
	if(sec == NULL) return false;
	if(! _FAT_cache_getSector(cache, sector, sec) ) {
		_FAT_mem_free(sec);
		return false;
	}
	memcpy(buffer, sec + offset, size);
	_FAT_mem_free(sec);
	return true;
}

bool _FAT_cache_readLittleEndianValue (CACHE* cache, uint32_t *value, sec_t sector, unsigned int offset, int num_bytes) {
  uint8_t buf[4];
  if (!_FAT_cache_readPartialSector(cache, buf, sector, offset, num_bytes)) return false;

  switch(num_bytes) {
  case 1: *value = buf[0]; break;
  case 2: *value = u8array_to_u16(buf,0); break;
  case 4: *value = u8array_to_u32(buf,0); break;
  default: return false;
  }
  return true;
}

/*
Writes some data to a cache page, making sure it is loaded into memory first.
*/
bool _FAT_cache_writePartialSector (CACHE* cache, const void* buffer, sec_t sector, unsigned int offset, size_t size) {
	unsigned int i;
	void* sec;
	CACHE_ENTRY* cacheEntries = cache->cacheEntries;
	unsigned int numberOfPages = cache->numberOfPages;

	if (offset + size > BYTES_PER_READ) {
		return false;
	}

	//To be sure sector is in cache
	sec = (void*) _FAT_mem_align ( BYTES_PER_READ );
	if(sec == NULL) return false;
	if(! _FAT_cache_getSector(cache, sector, sec) ) {
		_FAT_mem_free(sec);
		return false;
	}
	_FAT_mem_free(sec);

	//Find where sector is and write
	for (i = 0; i < numberOfPages ; i++) {
		if ( sector>=cacheEntries[i].sector && sector < cacheEntries[i].sector+cacheEntries[i].count) {
			cacheEntries[i].last_access = accessTime();
			memcpy (cacheEntries[i].cache + ((sector-cacheEntries[i].sector)*BYTES_PER_READ) + offset, buffer, size);
			cache->cacheEntries[i].dirty = true;
			return true;
	    }
	}
	return false;
}

bool _FAT_cache_writeLittleEndianValue (CACHE* cache, const uint32_t value, sec_t sector, unsigned int offset, int size) {
  uint8_t buf[4] = {0, 0, 0, 0};

  switch(size) {
  case 1: buf[0] = value; break;
  case 2: u16_to_u8array(buf, 0, value); break;
  case 4: u32_to_u8array(buf, 0, value); break;
  default: return false;
  }

  return _FAT_cache_writePartialSector(cache, buf, sector, offset, size);
}

/*
Writes some data to a cache page, zeroing out the page first
*/
bool _FAT_cache_eraseWritePartialSector (CACHE* cache, const void* buffer, sec_t sector, unsigned int offset, size_t size) {
	unsigned int i;
	void* sec;
	CACHE_ENTRY* cacheEntries = cache->cacheEntries;
	unsigned int numberOfPages = cache->numberOfPages;

	if (offset + size > BYTES_PER_READ) {
		return false;
	}

	//To be sure sector is in cache
	sec = (void*) _FAT_mem_align ( BYTES_PER_READ );
	if(sec == NULL) return false;
	if(! _FAT_cache_getSector(cache, sector, sec) ) {
		_FAT_mem_free(sec);
		return false;
	}
	_FAT_mem_free(sec);

	//Find where sector is and write
	for (i = 0; i < numberOfPages ; i++) {
		if ( sector>=cacheEntries[i].sector && sector < cacheEntries[i].sector+cacheEntries[i].count) {
			cacheEntries[i].last_access = accessTime();
			memset (cacheEntries[i].cache + ((sector-cacheEntries[i].sector)*BYTES_PER_READ), 0, BYTES_PER_READ);
			memcpy (cacheEntries[i].cache + ((sector-cacheEntries[i].sector)*BYTES_PER_READ) + offset, buffer, size);
			cache->cacheEntries[i].dirty = true;
			return true;
	    }
	}
	return false;
}

bool _FAT_cache_writeSectors (CACHE* cache, sec_t sector, sec_t numSectors, const void* buffer) {
	unsigned int i;
	CACHE_ENTRY* cacheEntries = cache->cacheEntries;
	unsigned int numberOfPages = cache->numberOfPages;
	sec_t sec;
	sec_t secs_to_write;

	unsigned int oldUsed = 0;
	unsigned int oldAccess = cacheEntries[0].last_access;

	while(numSectors>0)
	{
		i=0;
		while (i < numberOfPages ) {
			if ( (sector>=cacheEntries[i].sector && sector < cacheEntries[i].sector+cacheEntries[i].count) ||
				 (sector == cacheEntries[i].sector+cacheEntries[i].count && cacheEntries[i].count < cache->sectorsPerPage)) {
				sec=sector-cacheEntries[i].sector;
				secs_to_write=cache->sectorsPerPage-sec;
				if(secs_to_write>numSectors)secs_to_write=numSectors;
				memcpy(cacheEntries[i].cache + (sec*BYTES_PER_READ), buffer, secs_to_write*BYTES_PER_READ);
				cacheEntries[i].last_access = accessTime();
				cacheEntries[i].dirty = true;
				cacheEntries[i].count = sec + secs_to_write;
				numSectors=numSectors-secs_to_write;
				if(numSectors==0) return true;
				buffer+=secs_to_write*BYTES_PER_READ;
				sector+=secs_to_write;
				i=-1; // recheck all pages again
				oldUsed = 0;
				oldAccess = cacheEntries[0].last_access;

			}
			else // While searching for the desired sector, also search for the least recently used page
			if ( (cacheEntries[i].sector == CACHE_FREE) || (cacheEntries[i].last_access < oldAccess) ) {
				oldUsed = i;
				oldAccess = cacheEntries[i].last_access;
			}
			i++;
	    }
		// If it didn't, replace the least recently used cache page with the desired sector
		if ((cacheEntries[oldUsed].sector != CACHE_FREE) && (cacheEntries[oldUsed].dirty == true)) {
			// Write the page back to disc if it has been written to
			if (!_FAT_disc_writeSectors (cache->disc, cacheEntries[oldUsed].sector, cacheEntries[oldUsed].count, cacheEntries[oldUsed].cache)) {
				return false;
			}
			cacheEntries[oldUsed].dirty = false;
		}

		secs_to_write=numSectors;
		if(secs_to_write>cache->sectorsPerPage)secs_to_write=cache->sectorsPerPage;
		cacheEntries[oldUsed].sector = sector;
		cacheEntries[oldUsed].count = secs_to_write;

		memcpy(cacheEntries[oldUsed].cache, buffer, secs_to_write*BYTES_PER_READ);
		buffer+=secs_to_write*BYTES_PER_READ;
		sector+=secs_to_write;
		numSectors=numSectors-secs_to_write;

		// Increment the usage count, don't reset it
		// This creates a paging policy of least used PAGE, not sector
		cacheEntries[oldUsed].last_access = accessTime();
		cacheEntries[oldUsed].dirty = true;
		if(numSectors==0) return true;
		oldUsed = 0;
		oldAccess = cacheEntries[0].last_access;
	}
	return false;
}

/*
Flushes all dirty pages to disc, clearing the dirty flag.
*/
bool _FAT_cache_flush (CACHE* cache) {
	unsigned int i;

	for (i = 0; i < cache->numberOfPages; i++) {
		if (cache->cacheEntries[i].dirty) {
			if (!_FAT_disc_writeSectors (cache->disc, cache->cacheEntries[i].sector, cache->cacheEntries[i].count, cache->cacheEntries[i].cache)) {
				return false;
			}
		}
		cache->cacheEntries[i].dirty = false;
	}

	return true;
}

void _FAT_cache_invalidate (CACHE* cache) {
	unsigned int i;
	_FAT_cache_flush(cache);
	for (i = 0; i < cache->numberOfPages; i++) {
		cache->cacheEntries[i].sector = CACHE_FREE;
		cache->cacheEntries[i].last_access = 0;
		cache->cacheEntries[i].count = 0;
		cache->cacheEntries[i].dirty = false;
	}
}
