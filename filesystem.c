#include "filesystem.h"

int initRootDir(block *disk, const unsigned int root_ino);

//Format the virtual disk file, 0 for success, -1 for failed.
int format(const int blocknum, const char *path) {
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
    ino->i_size = 1 * BLKSIZE; // './'
    bool mode[9] = {1,1,1,1,1,1,1,0,1};
    chmod(disk, root_ino, mode);
    unsigned int bno = balloc(disk, ino);
    if (bno == 0) return -1;
    disk[bno].dirblk.fileitemTable[0].valid = true;
    dinode *dno = &(disk[bno].dirblk.fileitemTable[0].d_inode);
    dno->di_ino = ino->i_ino;
    // dno->di_blkcount = ino->di_blkcount;
    // dno->di_gid = ino->di_gid;
    // dno->di_uid = ino->di_uid;
    // dno->di_size = ino->i_size;
    // for (int i=0; i<9; ++i) {
    //     dno->di_mode[i] = ino->di_mode[i];
    // }
    strcpy(disk[bno].dirblk.fileitemTable[0].filename, ".");
    disk[bno].dirblk.fileitemTable[1].valid = true;
    dno = &(disk[bno].dirblk.fileitemTable[1].d_inode);
    dno->di_ino = ino->i_ino;
    // dno->di_blkcount = ino->di_blkcount;
    // dno->di_gid = ino->di_gid;
    // dno->di_uid = ino->di_uid;
    // dno->di_size = ino->i_size;
    // for (int i=0; i<9; ++i) {
    //     dno->di_mode[i] = ino->di_mode[i];
    // }
    strcpy(disk[bno].dirblk.fileitemTable[1].filename, "..");
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

// Change current work directory through relative path.
// cwd = Current Work Dir, NULL for failed.
inode* chdir_rel(block *disk, inode *cwd, const char *vpath) {
    if (vpath[0] == '\0') return cwd;
    char buffer[MAXFILENAMELEN] = {'\0'};
    int cur = 0;
    int buffcur = 0;
    if (vpath[cur] == '/') ++cur;
    for (cur; vpath[cur]!='/' && vpath[cur]!='\0'; ++cur, ++buffcur) {
        buffer[buffcur] = vpath[cur];
    }
    for (int bnoidx=0; bnoidx<cwd->di_blkcount; ++bnoidx) {
        unsigned int bno = cwd->di_addr[bnoidx];
        for (int idx=0; idx<DIRFILEMAXCOUNT; ++idx) {
            inode *ino = get_inode(disk, disk[bno].dirblk.fileitemTable[idx].d_inode.di_ino);
            if (strcmp(disk[bno].dirblk.fileitemTable[idx].filename, buffer) == 0 
                && disk[bno].dirblk.fileitemTable[idx].valid == true
                && ino->i_flag == INODE_DIR) {
                return chdir_rel(disk, ino, &(vpath[cur]));
            }
        }
    }
    // cannot find dir or not dir type.
    return NULL;
}

// Return root dir inode.
inode* chdir_to_root(block *disk) {
    return get_inode(disk, ROOTINODEID);
}

// Create new dir in the directory. Returns new dir inode, NULL for failed.
inode* mkdir(block *disk, inode *dir, const char *dirname) {
    if (!find_in_dir(disk, dir, dirname)) {
        unsigned int newinoid = icalloc(disk);
        if (newinoid == 0) return NULL;
        inode *newino = get_inode(disk, newinoid);
        // dreate new directory
        newino->i_flag = INODE_DIR;
        unsigned int newbno = balloc(disk, newino);
        if (newbno == 0) {
            free_inode(disk, newino);
            return NULL;
        }  
        newino->i_size = BLKSIZE;
        newino->i_ctime = newino->i_mtime = time(NULL);
        fileitem* newitem = get_available_fileitem(disk, dir);
        if (newitem == NULL) {
            free_inode(disk, newino);
            return NULL;
        }
        newitem->valid = 1;
        strcpy(newitem->filename, dirname);
        newitem->d_inode.di_ino = newinoid;
        init_dirblock(&disk[newbno], newino, dir);
        bool newmode[9] = { 1, 1, 1, 1, 0, 1, 1, 0, 0 };
        chmod(disk, newinoid, newmode);
        // TODO: set uid, gid

        return newino;
    }
    // find file with same filename.
    return NULL;
}

// Find file or dir inode in directory, NULL for failed. 
inode* find_in_dir(block *disk, inode *dir, const char *filename) {
    for (int bnoidx=0; bnoidx<dir->di_blkcount; ++bnoidx) {
        unsigned int bno = dir->di_addr[bnoidx];
        for (int idx=0; idx<DIRFILEMAXCOUNT; ++idx) {
            if (strcmp(disk[bno].dirblk.fileitemTable[idx].filename, filename) == 0) {
                return get_inode(disk, disk[bno].dirblk.fileitemTable[idx].d_inode.di_ino);
            }
        }
    }
    return NULL;
}

// Get dir fileitem from its parent dir; NULL for root dir or failed.
fileitem* get_dir_fileitem(block *disk, inode *dir) {
    if (dir->i_ino == ROOTINODEID) return NULL;
    inode *parent = get_inode(disk, disk[dir->di_addr[0]].dirblk.fileitemTable[1].d_inode.di_ino);
    for (int bnoidx=0; bnoidx<parent->di_blkcount; ++bnoidx) {
        unsigned int bno = parent->di_addr[bnoidx];
        for (int idx=0; idx<DIRFILEMAXCOUNT; ++idx) {
            if (disk[bno].dirblk.fileitemTable[idx].d_inode.di_ino == dir->i_ino) {
                return &(disk[bno].dirblk.fileitemTable[idx]);
            }
        }
    }
    return NULL;
}

// Create new file in the directory, NULL for failed.
inode* touch(block *disk, inode *dir, const char *filename) {
    if (!find_in_dir(disk, dir, filename)) {
        unsigned int newinoid = icalloc(disk);
        if (newinoid == 0) return NULL;
        inode *newino = get_inode(disk, newinoid);
        // dreate new file
        newino->i_flag = INODE_FILE;
        unsigned int newbno = balloc(disk, newino);
        if (newbno == 0) {
            free_inode(disk, newino);
            return NULL;
        }  
        newino->i_size = 0;
        newino->i_ctime = newino->i_mtime = time(NULL);
        fileitem* newitem = get_available_fileitem(disk, dir);
        if (newitem == NULL) {
            free_inode(disk, newino);
            return NULL;
        }
        newitem->valid = 1;
        strcpy(newitem->filename, filename);
        newitem->d_inode.di_ino = newinoid;
        clearBlock(&disk[newbno]);
        bool newmode[9] = { 1, 1, 1, 1, 0, 1, 1, 0, 0 };
        chmod(disk, newinoid, newmode);
        // TODO: set uid, gid

        return newino;
    }
    // find file with same filename.
    return NULL;
}

// Write or append string to file. NULL for failed.
inode* echo(block *disk, inode* file, const char *content, const enum echo_mode mode) {
    if (file->i_flag != INODE_FILE) return NULL;
    switch (mode)
    {
    case ECHO_W:
        {
            if (strlen(content)+1 < BLKSIZE) {
                // free other blocks
                for (int i=1; i<file->di_blkcount; ++i) {
                    free_block(disk, file->di_addr[i]);
                }
                file->di_blkcount = 1;
                // write bytes
                strcpy(disk[file->di_addr[0]].normalblk.data, content);
                int fin = strlen(content);
                disk[file->di_addr[0]].normalblk.data[fin+1] = EOF;
            } else {
                //TODO: extend block
            }
        }
        break;
    
    case ECHO_A:

        break;
    default:
        return NULL;
        break;
    }
    return 0;
}

// Read file content to buffer. -1 for failed.
int cat(block *disk, inode* file, char *buffer, const unsigned int buffer_size) {
    if (file->i_size > buffer_size) return -1;
    unsigned int cur = 0;
    if (file->i_size <= BLKSIZE) {
        while (disk[file->di_addr[0]].normalblk.data[cur] != EOF) {
            buffer[cur] = disk[file->di_addr[0]].normalblk.data[cur];
            ++cur;
        }
    } else {
        // TODO: deal with multi-block file.

    }
    return cur+1;
}

