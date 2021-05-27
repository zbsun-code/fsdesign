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

// Change current work directory through absolute path.
inode* chdir_abs(block *disk, const char *vpath) {
    inode *rootino = get_inode(disk, ROOTINODEID);
    if (strlen(vpath) == 1 && vpath[0] == '/') return rootino;
    char buffer[MAXFILENAMELEN] = {'\0'};
    int cur = 0;
    int buffcur = 0;
    if (vpath[cur] == '/') ++cur;
    for (cur; vpath[cur]!='/' && vpath[cur]!='\0'; ++cur, ++buffcur) {
        buffer[buffcur] = vpath[cur];
    }
    for (int bnoidx=0; bnoidx<rootino->di_blkcount; ++bnoidx) {
        unsigned int bno = rootino->di_addr[bnoidx];
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
            if (disk[bno].dirblk.fileitemTable[idx].valid && strcmp(disk[bno].dirblk.fileitemTable[idx].filename, filename) == 0) {
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
            if (disk[bno].dirblk.fileitemTable[idx].valid && disk[bno].dirblk.fileitemTable[idx].d_inode.di_ino == dir->i_ino) {
                return &(disk[bno].dirblk.fileitemTable[idx]);
            }
        }
    }
    return NULL;
}

// Get file fileitem from its parent dir; NULL for failed.
fileitem* get_file_fileitem(block *disk, inode *dir, const char *filename) {
    for (int bnoidx=0; bnoidx<dir->di_blkcount; ++bnoidx) {
        unsigned int bno = dir->di_addr[bnoidx];
        for (int idx=0; idx<DIRFILEMAXCOUNT; ++idx) {
            if (disk[bno].dirblk.fileitemTable[idx].valid && strcmp(disk[bno].dirblk.fileitemTable[idx].filename, filename) == 0) {
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

// Write or append string to file. -1 for failed, -2 for beyond largest size, -3 for no available block.
int echo(block *disk, inode* file, const char *content, const enum echo_mode mode) {
    if (file->i_flag != INODE_FILE) return -1;
    if (strlen(content) == 0) return 0;
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
                file->i_size = 0;
                // write bytes
                strcpy(disk[file->di_addr[0]].normalblk.data, content);
                int fin = strlen(content);
                disk[file->di_addr[0]].normalblk.data[fin+1] = EOF;
                file->i_size = strlen(content)+1;
            } else if (strlen(content) + 2 > BLKSIZE * NADDR) {
                // beyond largest filesize
                return -2;
            } else {
                // extend block
                unsigned int remainsize = strlen(content) + 2;   //strsize + '\0' + EOF
                unsigned int blkcur = 0;
                while (remainsize > BLKSIZE) {
                    for (int i=0; i<BLKSIZE; ++i) {
                        disk[file->di_addr[blkcur]].normalblk.data[i] = content[blkcur * BLKSIZE + i];
                    }
                    file->i_size += BLKSIZE;
                    // alloc new block
                    if (balloc(disk, file) == 0) {
                        return -3;
                    }
                    clearBlock(&disk[file->di_addr[blkcur+1]]); //initialize new block
                    ++blkcur;
                    remainsize -= BLKSIZE;
                }
                //last block
                for (int i=0; i<remainsize-1; ++i) {
                    disk[file->di_addr[blkcur]].normalblk.data[i] = content[blkcur * BLKSIZE + i];
                    ++(file->i_size);
                }
                disk[file->di_addr[blkcur]].normalblk.data[remainsize-1] = EOF;
                file->i_size += 1;
            }
        }
        break;
    
    case ECHO_A:
        {
            if ((strlen(content) + file->i_size) <= (BLKSIZE * file->di_blkcount)) {
                int strcur = 0;
                if (file->i_size <= 1) {
                    file->i_size = 0;
                } else if (file->i_size % BLKSIZE == 1) {
                    // EOF in last block but '\0' in the former block.
                    disk[file->di_addr[file->di_blkcount-2]].normalblk.data[BLKSIZE - 1] = content[strcur];
                    ++strcur;
                    file->i_size -= 1;
                } else {
                    file->i_size -= 2;  //'\0' and EOF
                }
                int offset = file->i_size % BLKSIZE;
                int cur = 0;
                for (strcur; strcur<(strlen(content)+1); ++cur, ++strcur) {
                    disk[file->di_addr[file->di_blkcount-1]].normalblk.data[offset + cur] = content[strcur];
                    ++(file->i_size);
                }
                disk[file->di_addr[file->di_blkcount-1]].normalblk.data[offset + cur] = EOF;
                ++(file->i_size);
            } else if (strlen(content) + file->i_size > NADDR * BLKSIZE) {
                // beyond largest filesize
                return -2;
            } else {
                int remainsize = strlen(content) + 2;
                int strcur = 0;
                if (file->i_size <= 1) {
                    file->i_size = 0;
                } else if (file->i_size % BLKSIZE == 1) {
                    // EOF in last block but '\0' in the former block.
                    disk[file->di_addr[file->di_blkcount-2]].normalblk.data[BLKSIZE - 1] = content[strcur];
                    ++strcur;
                    --remainsize;
                    file->i_size -= 1;
                } else {
                    file->i_size -= 2;  //'\0' and EOF
                }
                // fill the not full block               
                int offset = file->i_size % BLKSIZE;
                int blkcur = file->di_blkcount - 1;
                for (int cur=offset; cur<BLKSIZE; ++cur) {
                    disk[file->di_addr[blkcur]].normalblk.data[cur] = content[strcur];
                    ++(file->i_size);
                    ++strcur;
                    --remainsize;
                }
                // alloc new block
                if (balloc(disk, file) == 0) {
                    return -3;
                }
                clearBlock(&disk[file->di_addr[blkcur+1]]); //initialize new block
                ++blkcur;
                // fill new blocks
                while (remainsize > BLKSIZE) {
                    for (int i=0; i<BLKSIZE; ++i, ++strcur) {
                        disk[file->di_addr[blkcur]].normalblk.data[i] = content[strcur];
                    }
                    file->i_size += BLKSIZE;
                    // alloc new block
                    if (balloc(disk, file) == 0) {
                        return -3;
                    }
                    clearBlock(&disk[file->di_addr[blkcur+1]]); //initialize new block
                    ++blkcur;
                    remainsize -= BLKSIZE;
                }
                for (int i=0; i<remainsize-1; ++i, ++strcur) {
                    disk[file->di_addr[blkcur]].normalblk.data[i] = content[strcur];
                }
                disk[file->di_addr[blkcur]].normalblk.data[remainsize-1] = EOF;
                ++(file->i_size);
            }
        }
        break;
    default:
        return -1;
        break;
    }
    file->i_mtime = time(NULL);
    return 0;
}

// Read file content to buffer. -1 for failed.
int cat(block *disk, inode* file, char *buffer, const unsigned int buffer_size) {
    if (file->i_size > buffer_size || buffer_size == 0) return -1;
    if (file->i_size < 2) {
        buffer[0] = '\0';
        return 0;
    }
    unsigned int cur = 0;
    if (file->i_size <= BLKSIZE) {
        while (disk[file->di_addr[0]].normalblk.data[cur] != EOF) {
            buffer[cur] = disk[file->di_addr[0]].normalblk.data[cur];
            ++cur;
        }
    } else {
        // deal with multi-block file.
        for (int blkidx=0; blkidx<file->di_blkcount; ++blkidx) {
            for (int cur=0; cur<BLKSIZE; ++cur) {
                if (disk[file->di_addr[blkidx]].normalblk.data[cur] == EOF) {
                    return blkidx * BLKSIZE + cur + 1;
                }
                buffer[blkidx * BLKSIZE + cur] = disk[file->di_addr[blkidx]].normalblk.data[cur];
            }
        }
    }
    return cur+1;
}

// Delete the empty directory. Returns free bytes, 0 for delete dir but has hard link, -1 for not found, -2 for removing self or parent dir, -3 for not empty.
int rmdir(block *disk, inode *cwd, const char *dirname) {
    inode *dirinode = find_in_dir(disk, cwd, dirname);
    if (dirinode == NULL || dirinode->i_flag != INODE_DIR) return -1;
    fileitem *dirfileitem = get_dir_fileitem(disk, dirinode);
    if (strcmp(dirfileitem->filename, ".") == 0 || strcmp(dirfileitem->filename, "..") == 0) {
        return -2;
    }
    //skip "." and ".."
    for (int cur=2; cur<DIRFILEMAXCOUNT; ++cur) {
        if (disk[dirinode->di_addr[0]].dirblk.fileitemTable[cur].valid) {
            return -3;
        }
    }
    for (int blkidx=1; blkidx<dirinode->di_blkcount; ++blkidx) {
        for (int cur=0; cur<DIRFILEMAXCOUNT; ++cur) {
            if (disk[dirinode->di_addr[blkidx]].dirblk.fileitemTable[cur].valid) {
                return -3;
            }
        }
    }

    //start removing
    if (--(dirinode->i_count) == 0) {
        int count = 0;
        for (int blkidx=0; blkidx<dirinode->di_blkcount; ++blkidx) {
            free_block(disk, dirinode->di_addr[blkidx]);
            ++count;
        }
        dirinode->di_blkcount = 0;
        free_fileitem(dirfileitem);
        free_inode(disk, dirinode);
        return count * BLKSIZE;
    }
    return 0;
}

// Remove file. 0 for delete inode but still has hard link, -1 for not found, 
int rm(block *disk, inode* dir, const char *filename) {
    inode *fileinode = find_in_dir(disk, dir, filename);
    if (!fileinode || fileinode->i_flag != INODE_FILE) return -1;
    // TODO: check user permission
    
    if ((--(fileinode->i_count)) == 0) {
        fileitem *delfileitem = get_file_fileitem(disk, dir, filename);
        int count = 0;
        for (int blkidx=0; blkidx<fileinode->di_blkcount; ++blkidx) {
            free_block(disk, fileinode->di_addr[blkidx]);
            ++count;
        }
        fileinode->di_blkcount = 0;
        free_fileitem(delfileitem);
        free_inode(disk, fileinode);
        return count * BLKSIZE;
    }
    return 0;
}
