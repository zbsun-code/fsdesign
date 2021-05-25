#include "filesystem.h"

//Init inodes and inode stack in the unformatted disk and skip pushing the root dir inode, 0 for success, -1 for failed.
int _initInodes(block *disk, const unsigned int rootino) {
    unsigned int needed_blocknum = INODESTSIZE / INODEPERBLK + (INODESTSIZE % INODEPERBLK != 0); 
    if (disk[0].superblk.n_block - 1 < needed_blocknum) {
        return -1;
    }
    for (int blkidx=1; blkidx <= needed_blocknum; ++blkidx) {
        for (int i=0; i<INODEPERBLK; ++i) {
            unsigned ino = (blkidx - 1) * INODEPERBLK + i + 1;
            if (ino >= INODESTSIZE) break;
            disk[blkidx].inodeblk.inodeTable[i].i_ino = ino;
            disk[blkidx].inodeblk.inodeTable[i].di_blksize = BLKSIZE;
            //push into the free_inode stack
            if (ino == rootino) continue;   //Skip inode for root dir
            if (_pushInoSt(disk, ino) == -1) {
                return -1;
            }
        }
    }
    return 0;
}

int _initBlocks(block *disk) {
    unsigned int inodeblk_num = INODESTSIZE / INODEPERBLK + (INODESTSIZE % INODEPERBLK != 0); 
    disk[0].superblk.s_freeblock[0] = 0;
    disk[0].superblk.sp_freeblock = 1;
    for (int blkidx = inodeblk_num; blkidx < disk[0].superblk.n_block; ++blkidx) {
        if (_pushBlkSt(disk, blkidx) == -1) return -1;
    }
    return 0;
}

// 0 for success, -1 for failed. 
// Note that the base block of the stack is usually filled with old stack data, please remember to pop out the blocks before modifying them.
int _popBlkSt(block *disk) {
    unsigned int *sp = &(disk->superblk.sp_freeblock);
    unsigned int *st = disk[0].superblk.s_freeblock;
    if (*sp == 1) {
        if (st[*sp-1] == 0) return -1;    //stack empty
        unsigned int bno = st[*sp-1];
        *sp = BLKSTSIZE;
        for (int i=0; i<BLKSTSIZE; ++i) {
            st[i] = disk[bno].blkstblk.s_freeblock[i];
        }
    } else {
        --(*sp);
    }
    return 0;
}

//0 for success, -1 for failed.
int _pushBlkSt(block *disk, const unsigned int bno) {
    unsigned int *sp = &(disk->superblk.sp_freeblock);
    unsigned int *st = disk[0].superblk.s_freeblock;
    if (*sp >= BLKSTSIZE) {
        //stack full -> move stack
        for (int i=0; i<BLKSTSIZE; ++i) {
            disk[bno].blkstblk.s_freeblock[i] = st[i];
        }
        //make new block to be the new stack top(also base).
        st[0] = bno;
        disk[0].superblk.sp_freeblock = 1;
    } else {
        ++(*sp);
        st[*sp-1] = bno;
    }
    return 0;
}

//0 for success, -1 for failed.
int _popInoSt(block *disk) {
    unsigned int *sp = &(disk->superblk.sp_freeinode);
    if (*sp == 0) {
        return -1;
    } else {
        --(*sp);
    }
    return 0;
}

//0 for success, -1 for failed.
int _pushInoSt(block *disk, const unsigned int ino) {
    unsigned int *sp = &(disk[0].superblk.sp_freeinode);
    if (*sp >= INODESTSIZE) {
        //push failed
        return -1;
    }
    ++(*sp);
    disk[0].superblk.s_freeinode[*sp-1] = ino;
    return 0;
}

//Allocate inode, returns inode no., 0 for fail.
unsigned int ialloc(block *disk) {
    unsigned int ino = inodest_top(disk);
    if (_popInoSt(disk) == -1) return 0;
    return ino;
}

//Allocate block for inode, returns block no., 0 for fail.
unsigned int balloc(block *disk, inode *ino) {
    unsigned int bno = blkst_top(disk);
    if (ino->di_blkcount < NADDR) {
        if (_popBlkSt(disk) != -1) {
            ino->di_addr[ino->di_blkcount] = bno;
            ++ino->di_blkcount;
        }
    } else {
        return 0;
    } 
    return bno;
}