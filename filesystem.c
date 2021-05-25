#include "filesystem.h"

int initRootDir(block *disk, const unsigned int root_ino);

//Format the virtual disk file, 0 for success, -1 for failed.
int format(int blocknum, const char *path) {
    FILE *fp = fopen(path, "wb+");
    if (!fp) return -1;
    block *disk = (block *)calloc(blocknum, sizeof(block));
    block *sblk = disk;
    sblk->superblk.n_block = blocknum;
    sblk->superblk.n_freeblock = blocknum - 1;
    sblk->superblk.n_freeinode = INODESTSIZE;
    sblk->superblk.n_inode = INODESTSIZE;
    sblk->superblk.sp_freeblock = 0;
    sblk->superblk.sp_freeinode = 0;
    if (_initInodes(disk, ROOTINODEID) == -1 || _initBlocks(disk) == -1)
    {
        return -1;
    }
    initRootDir(disk, ROOTINODEID);
    for (int i=0; i<blocknum; ++i) {
        if (!fwrite(&(disk[i]), BLKSIZE, 1, fp)) return -1;
    }
    fclose(fp);
    fp = NULL;
    free(disk);
    return 0;
}

//Init root dir(incliding inode and dirblk) for new formatted virtual disk, -1 for failed.
int initRootDir(block *disk, const unsigned int root_ino) {
    inode *ino = get_inode(disk, root_ino);
    ino->i_flag = INODE_DIR;
    ino->di_uid = 0;
    ino->di_gid = 0;
    ino->i_count = 1;
    ino->i_ctime = time(NULL);
    ino->i_mtime = ino->i_ctime;
    ino->i_size = 1 * sizeof(fileitem); // './'
    bool mode[9] = {1,1,1,1,1,1,1,0,1};
    chmod(disk, root_ino, mode);
    unsigned int bno = balloc(disk, ino);
    if (bno == 0) return -1;
    disk[bno].dirblk.fileitemTable[0].availale = true;
    dinode *dno = &(disk[bno].dirblk.fileitemTable[0].d_inode);
    dno->di_blkcount = ino->di_blkcount;
    dno->di_gid = ino->di_gid;
    dno->di_uid = ino->di_uid;
    dno->di_ino = ino->i_ino;
    dno->di_size = ino->i_size;
    for (int i=0; i<9; ++i) {
        dno->di_mode[i] = ino->di_mode[i];
    }
    strcpy(disk[bno].dirblk.fileitemTable[0].filename, ".");
    return 0;
}

//Change the inode permission mode, -1 for failed. 
int chmod(block *disk, const unsigned int i_ino, bool *mode) {
    //TODO: need to check user's permission
    inode *ino = get_inode(disk, i_ino);
    for (int i=0; i<9; ++i) {
        ino->di_mode[i] = mode[i]; 
    }
    return 0;
}

//Mount a virtual disk from file, NULL for failed.
block* mount(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    int length = (int)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    block *disk = (block *)malloc(length);
    if (fread(disk, length, 1, fp) == 0) {
        free(disk);
        disk = NULL;
        return NULL;
    }
    fclose(fp);
    return disk;
}


