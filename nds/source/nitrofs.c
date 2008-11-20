/*
	nitrofs.c - eris's wai ossum nitro filesystem device driver
		Based on information found at http://frangoassado.org/ds/rom_spec.txt and from the #dsdev ppls
		Kallisti (K) 2008-01-26 All rights reversed.
*/
#include <nds.h>
#include <string.h>
#include <fat.h>

#include <sys/dir.h>
#include <sys/iosupport.h>
#include <stdio.h>

DIR_ITER* nitroFSDirOpen(struct _reent *r, DIR_ITER *dirState, const char *path);
int nitroDirReset(struct _reent *r, DIR_ITER *dirState);
int nitroFSDirNext(struct _reent *r, DIR_ITER *dirState, char *filename, struct stat *st);
int nitroFSDirClose(struct _reent *r, DIR_ITER *dirState);
int nitroFSOpen(struct _reent *r, void *fileStruct, const char *path,int flags,int mode);
int nitroFSClose(struct _reent *r,int fd);
ssize_t nitroFSRead(struct _reent *r,int fd,char *ptr,size_t len);
off_t nitroFSSeek(struct _reent *r,int fd,off_t pos,int dir);
int nitroFSFstat(struct _reent *r,int fd,struct stat *st);

#define LOADERSTR	"PASS"	//look for this
#define LOADERSTROFFSET 0xac
#define FNTOFFSET 0x40
#define FATOFFSET 0x48

#define NITRONAMELENMAX 0x80	//max file name is 127 +1 for zero byte :D
#define NITROMAXPATHLEN	0x100	//256 bytes enuff?

#define NITROROOT 	0xf000	//root entry_file_id
#define NITRODIRMASK	0x0fff	//remove leading 0xf

#define NITROISDIR 0x80	//mask to indicate this name entry is a dir, other 7 bits = name length

//Directory filename subtable entry structure
struct ROM_FNTDir {
	u32 	entry_start;
	u16	entry_file_id;
	u16	parent_id;
};

//Yo, dis table is fat (describes the structures
struct ROM_FAT {
	u32	top;	//start of file in rom image
	u32	bottom;	//end of file in rom image
};

//used (mostly by the 'sub' functions) for the raw reading of rom image either via gba rom or dldi
struct nitroRawStruct {
	FILE *ndsFile;	//used if going thru dldi >_> (if set to null we assume GBA with loader)
	unsigned int	pos;	//where in the file am i?
};

struct nitroFSStruct {
	struct nitroRawStruct nrs;
	unsigned int start;	//where in the rom this file starts
	unsigned int end;	//where in the rom this file ends
//	unsigned int pos;	//where in current file are we?
};

struct nitroDIRStruct {
	struct nitroRawStruct nrs;
	unsigned int 	namepos;	//ptr to next name to lookup in list
	struct ROM_FAT  romfat;
	u16		entry_id;	//which entry this is (for files only) incremented with each new file in dir?
	u16		dir_id;		//which directory entry this is.. used ofc for dirs only
	u16		cur_dir_id;	//which directory entry we are using
	u16		parent_id;	//who is the parent of the current directory (this can be used to easily ../ )
};

//Globals!
u32 fntOffset;	//offset to start of filename table
u32 fatOffset;	//offset to start of file alloc table
bool isNdsFile;	//is it a nds file?
const char *ndsfilename;	//what nds file to use

devoptab_t nitroFSdevoptab={
	"nitro", //	const char *name;
	sizeof(struct nitroFSStruct),	//	int	structSize;
	&nitroFSOpen,	//	int (*open_r)(struct _reent *r, void *fileStruct, const char *path,int flags,int mode);
	&nitroFSClose,	//	int (*close_r)(struct _reent *r,int fd);
	NULL,	//	ssize_t (*write_r)(struct _reent *r,int fd,const char *ptr,size_t len);
	&nitroFSRead,	//	ssize_t (*read_r)(struct _reent *r,int fd,char *ptr,size_t len);
	&nitroFSSeek,	//	off_t (*seek_r)(struct _reent *r,int fd,off_t pos,int dir);
	&nitroFSFstat,	//	int (*fstat_r)(struct _reent *r,int fd,struct stat *st);
	NULL,	//	int (*stat_r)(struct _reent *r,const char *file,struct stat *st);
	NULL,	//	int (*link_r)(struct _reent *r,const char *existing, const char  *newLink);
	NULL,	//	int (*unlink_r)(struct _reent *r,const char *name);
	NULL,	//	int (*chdir_r)(struct _reent *r,const char *name);
	
	NULL,	//	int (*rename_r) (struct _reent *r, const char *oldName, const char *newName);
	NULL,	//	int (*mkdir_r) (struct _reent *r, const char *path, int mode);
	
	sizeof(struct nitroDIRStruct),	//	int dirStateSize;
	&nitroFSDirOpen,	//	DIR_ITER* (*diropen_r)(struct _reent *r, DIR_ITER *dirState, const char *path);
	&nitroDirReset,	//	int (*dirreset_r)(struct _reent *r, DIR_ITER *dirState);
	&nitroFSDirNext,	//	int (*dirnext_r)(struct _reent *r, DIR_ITER *dirState, char *filename, struct stat *filestat);
	&nitroFSDirClose,	//	int (*dirclose_r)(struct _reent *r, DIR_ITER *dirState);
	NULL,	//	int (*statvfs_r)(struct _reent *r, const char *path, struct statvfs *buf);
	NULL,	//	int (*ftruncate_r)(struct _reent *r, int fd, off_t len);
	NULL,	// 	int (*fsync_r)(struct _reent *r, int fd);
	NULL,	//	void *deviceData;
};

//inline these mebbe? these 4 'sub' functions deal with actually reading from either gba rom or .nds file :)
//what i rly rly rly wanna know is how an actual nds cart reads from itself, but it seems no one can tell me ~_~
//so, instead we have this weird weird haxy try gbaslot then try dldi method. If i (or you!!) ever do figure out
//how to read the proper way can replace these 4 functions and everything should work normally :)
int nitroSubOpen(struct nitroRawStruct *nrs) {
	if(isNdsFile) {
		if((nrs->ndsFile = fopen(ndsfilename,"rb+"))) {
			nrs->pos=0;
			return 1;
		}
	} else {
		nrs->ndsFile=(FILE*)NULL;
		nrs->pos=0;
		return 1;
	}
	return(0);
}

int nitroSubClose(struct nitroRawStruct *nrs) {
	if(isNdsFile) 
		return(fclose(nrs->ndsFile));
	return(0);
}


//reads from rom image either gba rom or dldi
int nitroSubRead(struct nitroRawStruct *nrs, void *ptr, int len) {
	if(isNdsFile) { //read from ndsfile
		len=fread(ptr,1,len,nrs->ndsFile);
	} else {	//reading from gbarom
		memcpy(ptr, nrs->pos+(void*)GBAROM,len); //len isnt checked here because other checks exist in the callers (hopefully)
	}
	nrs->pos+=len;
	return(len);
}

int nitroSubSeek(struct nitroRawStruct *nrs, int pos, int dir) {
	if(dir==SEEK_SET) 	//otherwise just set the pos :)
		nrs->pos=pos;
	else if(dir==SEEK_CUR) 
		nrs->pos+=pos;	//see ez!
	if(isNdsFile) { //read from ndsfile actually do a seek
		return(fseek(nrs->ndsFile,pos,dir));
	} else {
		return(nrs->pos);
	}
}

//Figure out if its gba or ds, setup stuff
bool nitroFSInit() {
	struct nitroRawStruct nrs;
	char romstr[0x10];

	sysSetCartOwner(BUS_OWNER_ARM9 );

	if(strncmp(((const char *)GBAROM)+LOADERSTROFFSET,LOADERSTR,strlen(LOADERSTR))==0) {	// found standard nds file in gba cart
		fntOffset=((u32)*(u32*)(((const char *)GBAROM)+FNTOFFSET));	
		fatOffset=((u32)*(u32*)(((const char *)GBAROM)+FATOFFSET));
		isNdsFile=false;
		AddDevice(&nitroFSdevoptab);
		return true;

	} else {	//okay then try something else ~_~

		isNdsFile=true;

		if ( __system_argv->argvMagic == ARGV_MAGIC && __system_argv->argc >= 1 ) {
			ndsfilename=__system_argv->argv[0];	//set global for what file.nds to use
			if (!fatInitDefault())	return false;
			if(nitroSubOpen(&nrs)) {
				nitroSubSeek(&nrs,LOADERSTROFFSET,SEEK_SET);
				nitroSubRead(&nrs,romstr,strlen(LOADERSTR));

				if(strncmp(romstr,LOADERSTR,strlen(LOADERSTR))!=0) return false;	

				nitroSubSeek(&nrs,FNTOFFSET,SEEK_SET);
				nitroSubRead(&nrs,&fntOffset,sizeof(fntOffset));
				nitroSubSeek(&nrs,FATOFFSET,SEEK_SET);
				nitroSubRead(&nrs,&fatOffset,sizeof(fatOffset));
				nitroSubClose(&nrs);

				AddDevice(&nitroFSdevoptab);
				return true;
			}
		} 
	}
	return false;
}



//Directory functs
DIR_ITER* nitroFSDirOpen(struct _reent *r, DIR_ITER *dirState, const char *path) {
	struct nitroDIRStruct *dirStruct=(struct nitroDIRStruct*)dirState->dirStruct; //this makes it lots easier!
	struct stat st;
	char dirname[NITRONAMELENMAX];
	char *cptr;
	char mydirpath[NITROMAXPATHLEN];	//to hold copy of path string
	char *dirpath=mydirpath;
//NOTE: might add prepending of chdir path? seems like lotta work for something silly that shoulda been handled in newlib anyways ~_~
	bool pathfound;
	if((cptr=strchr(path,':')))
		path=cptr+1;	//move path past any device names (if it was nixy style wouldnt need this step >_>)
	strncpy(dirpath,path,sizeof(mydirpath)-1);	//copy the string (as im gonna mutalate it)
	nitroSubOpen(&dirStruct->nrs);		//open a file for me to use
	dirStruct->cur_dir_id=NITROROOT;	//first root dir
	nitroDirReset(r,dirState);		//set dir to current path
	do {
		while((cptr=strchr(dirpath,'/'))==dirpath) {
			dirpath++;	//move past any leading / or // together
		}
		if(cptr)
			*cptr=0;	//erase /
		//here is prolly where you should handle .. and . filenames if desired 
		if(*dirpath==0) {//are we at the end of the path string?? if so there is nothing to search for we're already here !
			pathfound=true; //mostly this handles searches for root or /  or no path specified cases 
			break;
		}
		pathfound=false;
		while(nitroFSDirNext(r,dirState,dirname,&st)==0) {	
			if((st.st_mode==S_IFDIR) && !(strcmp(dirname,dirpath))) { //if its a directory and name matches dirpath
				dirStruct->cur_dir_id=dirStruct->dir_id;  //move us to the next dir in tree
				nitroDirReset(r,dirState);		//set dir to current path we just found...
				pathfound=true;
				break;
			}
		};
		if(!pathfound) 
			break;
		dirpath=cptr+1;	//move to right after last / we found
	} while(cptr); // go till after the last /
	if(pathfound) {
		return(dirState);
	} else {
		nitroSubClose(&dirStruct->nrs);	//oops almost forgot to close the file we opened <_<
		return(NULL);
	}
}


int nitroFSDirClose(struct _reent *r, DIR_ITER *dirState) {
	return(nitroSubClose(&((struct nitroDIRStruct*)dirState->dirStruct)->nrs));
	
}

//reset dir to start of entry selected by dirStruct->cur_dir_id which should be set in dirOpen
int nitroDirReset(struct _reent *r, DIR_ITER *dirState) {
	struct nitroDIRStruct *dirStruct=(struct nitroDIRStruct*)dirState->dirStruct; //this makes it lots easier!
	struct ROM_FNTDir dirsubtable;
	nitroSubSeek(&dirStruct->nrs,fntOffset+((dirStruct->cur_dir_id&NITRODIRMASK)*sizeof(struct ROM_FNTDir)),SEEK_SET);
	nitroSubRead(&dirStruct->nrs, &dirsubtable, sizeof(dirsubtable));
	dirStruct->namepos=dirsubtable.entry_start;	//set namepos to first entry in this dir's table
	dirStruct->entry_id=dirsubtable.entry_file_id;	//get number of first file ID in this branch
	dirStruct->parent_id=dirsubtable.parent_id;	//save parent ID in case we wanna add ../ functionality
	return(0);
}

int nitroFSDirNext(struct _reent *r, DIR_ITER *dirState, char *filename, struct stat *st) {
	unsigned char next;
	struct nitroDIRStruct *dirStruct=(struct nitroDIRStruct*)dirState->dirStruct; //this makes it lots easier!
	nitroSubSeek(&dirStruct->nrs,fntOffset+dirStruct->namepos,SEEK_SET);
	nitroSubRead(&dirStruct->nrs, &next , sizeof(next));
	// next: high bit 0x80 = entry isdir.. other 7 bits r size, the 16 bits following name are dir's entryid (starts with f000)
	//  00 = endoftable //
	if(next) {
		if(next&NITROISDIR) {
			if(st) st->st_mode=S_IFDIR;
			next&=NITROISDIR^0xff;	//invert bits and mask off 0x80
			nitroSubRead(&dirStruct->nrs,filename,next);	
//			nitroSubRead(&dirStruct->nrs,&dirStruct->dir_id,sizeof(struct nitroDIRStruct.dir_id)); //read the dir_id
//grr cant get the struct member size?, just wanna test it so moving on...
			nitroSubRead(&dirStruct->nrs,&dirStruct->dir_id,sizeof(u16)); //read the dir_id
			dirStruct->namepos+=next+sizeof(u16)+1;		//now we points to next one plus dir_id size:D
		} else {
			if(st) st->st_mode=0;
			nitroSubRead(&dirStruct->nrs,filename,next);
			dirStruct->namepos+=next+1;		//now we points to next one :D
			//read file info to get filesize (and for fileopen)
			nitroSubSeek(&dirStruct->nrs,fatOffset+(dirStruct->entry_id*sizeof(struct ROM_FAT)),SEEK_SET);	
			nitroSubRead(&dirStruct->nrs, &dirStruct->romfat, sizeof(dirStruct->romfat));	//retrieve romfat entry (contains filestart and end positions)
			dirStruct->entry_id++; //advance ROM_FNTStrFile ptr
			if(st) st->st_size=dirStruct->romfat.bottom-dirStruct->romfat.top; //calculate filesize
		}
		filename[(int)next]=0;	//zero last char		
		return(0);
	} else {
		return(-1);
	}
}

//fs functs
int nitroFSOpen(struct _reent *r, void *fileStruct, const char *path,int flags,int mode) {
	struct nitroFSStruct *fatStruct=(struct nitroFSStruct *)fileStruct;
	struct nitroDIRStruct dirStruct;
	DIR_ITER dirState;
	dirState.dirStruct=&dirStruct;	//create a temp dirstruct
	struct _reent dre;	
	struct stat st;		//all these are just used for reading the dir ~_~
	char dirfilename[NITROMAXPATHLEN]; // to hold a full path (i tried to avoid using so much stack but blah :/)
	char *filename; // to hold filename
	char *cptr;	//used to string searching and manipulation
	cptr=(char*)path+strlen(path);	//find the end...
	filename=NULL;	
	do {
		if((*cptr=='/') || (*cptr==':')) { // split at either / or : (whichever comes first from the end!)
			cptr++;
			strncpy(dirfilename,path,cptr-path);	//copy string up till and including/ or : zero rest
			dirfilename[cptr-path]=0;		//it seems strncpy doesnt always zero?!
			filename=cptr;	//filename = now remainder of string
			break;			
		}
	} while(cptr--!=dirfilename); //search till start
	if(!filename) { //we didnt find a / or : ? shouldnt realyl happen but if it does...
		filename=(char*)path;	//filename = complete path
		dirfilename[0]=0;	//make directory path ""
	}
	if(nitroFSDirOpen(&dre,&dirState,dirfilename)) {
		fatStruct->start=0;
		while(nitroFSDirNext(&dre,&dirState, dirfilename, &st)==0) {
			if(!(st.st_mode & S_IFDIR) && (strcmp(dirfilename,filename)==0)) { //Found the *file* your looking for!!
				fatStruct->start=dirStruct.romfat.top;
				fatStruct->end=dirStruct.romfat.bottom;
				break;
			}
		}
		if(fatStruct->start) {
			fatStruct->nrs.ndsFile=dirStruct.nrs.ndsFile;	//reuse already open'd filehandle
			nitroSubSeek(&fatStruct->nrs,fatStruct->start,SEEK_SET);	//seek to start of file
			return(0);	//woot!
		}
		nitroFSDirClose(&dre,&dirState);
	}
	return(-1);
}

int nitroFSClose(struct _reent *r,int fd) {
	return(nitroSubClose(&((struct nitroFSStruct *)fd)->nrs));	
}

ssize_t nitroFSRead(struct _reent *r,int fd,char *ptr,size_t len) {
	struct nitroFSStruct *fatStruct=(struct nitroFSStruct *)fd;
	struct nitroRawStruct *nrs=&((struct nitroFSStruct *)fd)->nrs;
	if(nrs->pos+len > fatStruct->end) 
		len=fatStruct->end-nrs->pos;	//dont let us read past the end plz!
	if(nrs->pos > fatStruct->end)
		return(0);	//hit eof
	return(nitroSubRead(nrs,ptr,len));
}

off_t nitroFSSeek(struct _reent *r,int fd ,off_t pos,int dir) {
	//need check for eof here...
	struct nitroFSStruct *fatStruct=(struct nitroFSStruct *)fd;
	struct nitroRawStruct *nrs=&((struct nitroFSStruct *)fd)->nrs;
	if(dir==SEEK_SET)
		pos+=fatStruct->start;	//add start from .nds file offset
	if(pos > fatStruct->end) 
		return(0);	//dont let us read past the end plz!
	return(nitroSubSeek(nrs,pos,dir));

}

int nitroFSFstat(struct _reent *r,int fd,struct stat *st) {
	struct nitroFSStruct *fatStruct=(struct nitroFSStruct *)fd;
	//cant think of what else to do here besides report the size atm
	st->st_size=fatStruct->end-fatStruct->start;
	return(0);	
}
