/*
 filetime.c
 Conversion of file time and date values to various other types

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


#include "filetime.h"

#ifdef NDS
#include <nds/ipc.h>
#endif

u16 _FAT_filetime_getTimeFromRTC (void) {
#ifdef NDS
	return (
		( ( (IPC->rtc_hours > 11 ? IPC->rtc_hours - 40 : IPC->rtc_hours) & 0x1F) << 11) |
		( (IPC->rtc_minutes & 0x3F) << 5) |
		( (IPC->rtc_seconds >> 1) & 0x1F) );
#else
	return 0;
#endif
}


u16 _FAT_filetime_getDateFromRTC (void) {
#ifdef NDS
	return ( 
		( ((IPC->rtc_year + 20) & 0x7F) <<9) |
		( (IPC->rtc_month & 0xF) << 5) |
		(IPC->rtc_day & 0x1F) );
#else
	return 0;
#endif
}

time_t _FAT_filetime_to_time_t (u16 time, u16 date) {
	int hour, minute, second;
	int day, month, year;
	
	time_t result;
	
	hour = time >> 11;
	minute = (time >> 5) & 0x3F;
	second = (time & 0x1F) << 1;
	
	day = date & 0x1F;
	month = (date >> 5) & 0x0F;
	year = date >> 9;
	
	// Second values are averages, so time value won't be 100% accurate,
	// but should be within the correct month.
	result 	= second
			+ minute * 60
			+ hour * 3600
			+ day * 86400
			+ month * 2629743
			+ year * 31556926
			;

	return result;
}
