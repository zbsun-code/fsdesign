#pragma once
#include <stdio.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#define NADDR     		10			//max count of disk block address (max filesize = BLKSIZE * NADDR)
#define BLKSIZE			4096		//bytes of disk block
#define INODESTSIZE 	1008		//max count of inode super stack, also the max count of inodes
#define BLKSTSIZE		10			//max count of block super stack
#define ROOTINODEID		128			//id of root(/)
#define MINBLKNUM		1024		//min total block num
#define DIRFILEMAXCOUNT (BLKSIZE/sizeof(fileitem))	//max count of fileitems in single block
#define INODESIZE		sizeof(inode)
#define INODEPERBLK		(BLKSIZE/INODESIZE) //max count of inodes in one block
#define MAXFILENAMELEN 	128
#define inodest_top(disk) (disk[0].superblk.sp_freeinode?(disk[0].superblk.s_freeinode[disk[0].superblk.sp_freeinode - 1]):0)	//Return inode stack top, 0 for empty stack.
#define blkst_top(disk) (disk[0].superblk.s_freeblock[disk[0].superblk.sp_freeblock - 1]) //Return block stack top, 0 for empty stack.
#define get_inode(disk, ino_id) (&(disk[ino_id/INODEPERBLK + (ino_id % INODEPERBLK != 0)].inodeblk.inodeTable[ino_id % INODEPERBLK - 1]))

typedef bool prior_t[9];

enum inodeflag { 
	INODE_NONE=0, INODE_FILE, INODE_DIR, INODE_SYMLINK
};

typedef struct inode {
	enum inodeflag i_flag;			//0=none 1=file 2=directory
	unsigned int i_ino;				//inode id
	unsigned int i_count;			//reference count
	unsigned int i_size;			//file size (byte)
	prior_t di_mode;					//permission(owner, group, others)
	unsigned short di_uid;			//owner user id
	unsigned short di_gid;			//owner group id
	time_t i_mtime; 				//most recently modify time
	time_t i_ctime; 				//create time
	unsigned short di_blkcount;		//number of blocks
	unsigned short di_blksize;		//block size (byte)
	unsigned int di_addr[NADDR];	
} inode;

typedef struct dinode {
	unsigned int di_ino;			//inode no.
	// bool di_mode[9];				//permission(owner, group, others)
	// unsigned short di_uid;
	// unsigned short di_gid;
	// unsigned int di_size;
	// unsigned short di_blkcount;		//number of blocks
} dinode;

typedef struct fileitem {
	dinode d_inode;
	bool valid;
	char filename[MAXFILENAMELEN];
} fileitem;

typedef union block
{
	struct {	
		// inode inodeTable[INODESTSIZE];			//inode table
		unsigned int s_freeinode[INODESTSIZE];	//free inode stack
		unsigned int sp_freeinode;				//free inode stack ptr
		unsigned int s_freeblock[BLKSTSIZE];	//free block stack
		unsigned int sp_freeblock;				//free block ptr
		unsigned int n_freeinode;				//free inode count
		unsigned int n_freeblock;				//free block count
		unsigned int n_inode;					//total inode count
		unsigned int n_block;					//total block count
	} superblk;

	struct {
		inode inodeTable[INODEPERBLK];
	} inodeblk;

	struct {
		char data[BLKSIZE];
	} normalblk;

	struct {
		fileitem fileitemTable[DIRFILEMAXCOUNT];
	} dirblk;

	struct {
		unsigned int s_freeblock[BLKSTSIZE];
	} blkstblk;		//filled with old block stacks, waiting for recovery.
} block;

extern int format(const int blocknum, const char *path);
extern block* mount(const char *path);
extern int chmod(block *disk, const unsigned int i_ino, bool *mode);
extern inode* chdir_rel(block *disk, inode *cwd, const char *vpath);
extern inode* chdir_abs(block *disk, const char *vpath);
extern inode* chdir_to_root(block *disk);
extern inode* mkdir(block *disk, inode *cwd, const char *dirname);
extern int rmdir(block *disk, inode *cwd, const char *dirname);
extern inode* find_in_dir(block *disk, inode *dir, const char *filename);
extern fileitem* get_dir_fileitem(block *disk, inode *dir);
extern fileitem* get_file_fileitem(block *disk, inode *dir, const char *filename);
extern inode* touch(block *disk, inode *dir, const char *filename);

enum echo_mode {
	ECHO_W = 0, ECHO_A
};

extern int echo(block *disk, inode* file, const char *content, const enum echo_mode mode);
extern int cat(block *disk, inode* file, char *buffer, const unsigned int buffer_size);
extern int rm(block *disk, inode* dir, const char *filename);

// fsutils.c
extern int _initInodes(block *disk, const unsigned int rootino);
extern int _initBlocks(block *disk);
extern int _popInoSt(block *disk);
extern int _pushInoSt(block *disk, const unsigned int ino);
extern int _popBlkSt(block *disk);
extern int _pushBlkSt(block *disk, const unsigned int bno);
extern unsigned int ialloc(block *disk);
extern unsigned int icalloc(block *disk);
extern unsigned int balloc(block *disk, inode *ino);
extern void* clearBlock(block *blk);
extern fileitem* get_available_fileitem(block *disk, inode *dirinode);
extern int free_inode(block* disk, inode *ino);
extern int free_block(block *disk, unsigned int bno);
extern int free_fileitem(fileitem *fileitem);
extern int init_dirblock(block *dirblock, inode *dirinode, inode *parentinode);