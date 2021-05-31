#include "filesystem.h"

int errno = 0;

int initRootDir(block *disk, const unsigned int root_ino);

int get_errno() {
    int err = errno;
    errno = 0;
    return err;
}

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

// Save vdisk data to file. Return written bytes.
int savetofile(block *disk, const char *path) {
    FILE *fp = fopen(path, "wb+");
    if (!fp) return -1;
    int size = fwrite(disk, BLKSIZE, disk->superblk.n_block, fp);
    fclose(fp);
    return size;
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
    chmod(disk, root_ino, "111111101");
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

//Change the inode permission mode, -1 for no permission, -2 for symlink inode. 
int chmod(block *disk, const unsigned int i_ino, const char mode[9]) {
    inode *ino = get_inode(disk, i_ino);
    // check user's permission
    if (current_user.uid != ROOT_UID && ino->di_uid != current_user.uid) {
        return -1;
    }
    if (ino->i_flag == INODE_SYMLINK) {
        return -2;
    }
    for (int i=0; i<9; ++i) {
        ino->di_mode[i] = (mode[i] == '1'? true: false); 
    }
    return 0;
}

//Change file or dir's owner. -1 for no permission.
int chown(block *disk, const unsigned int i_ino, const unsigned short uid) {
    inode *ino = get_inode(disk, i_ino);
    // check user's permission
    if (current_user.uid != ROOT_UID && ino->di_uid != current_user.uid) {
        return -1;
    }
    ino->di_uid = uid;
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
// cwd = Current Work Dir, NULL for failed, meanwhile, errno=-1 means no permission.
inode* chdir_rel(block *disk, inode *cwd, const char *vpath) {
    if (vpath[0] == '\0') return cwd;
    char buffer[MAXFILENAMELEN] = {'\0'};
    int cur = 0;
    int buffcur = 0;
    if (vpath[cur] == '/') ++cur;
    if (vpath[cur] == '\0') return cwd;
    for (cur; vpath[cur]!='/' && vpath[cur]!='\0'; ++cur, ++buffcur) {
        buffer[buffcur] = vpath[cur];
    }
    for (int bnoidx=0; bnoidx<cwd->di_blkcount; ++bnoidx) {
        unsigned int bno = cwd->di_addr[bnoidx];
        for (int idx=0; idx<DIRFILEMAXCOUNT; ++idx) {
            inode *ino = get_inode(disk, disk[bno].dirblk.fileitemTable[idx].d_inode.di_ino);
            if (strcmp(disk[bno].dirblk.fileitemTable[idx].filename, buffer) == 0 
                && disk[bno].dirblk.fileitemTable[idx].valid == true) {
                if (ino->i_flag == INODE_DIR) {
                    if(check_permission(ino, &current_user, ugroupTable, PERM_X))
                        return chdir_rel(disk, ino, &(vpath[cur]));
                    else {
                        set_err(-1);     //no permission
                        return NULL;
                    }
                } else if (ino->i_flag == INODE_SYMLINK) {
                    inode *convertedino = get_symlink_dest(disk, ino);
                    if (!convertedino || convertedino->i_flag != INODE_DIR) {
                        return NULL;
                    } else {
                        if(check_permission(convertedino, &current_user, ugroupTable, PERM_X))
                            return chdir_rel(disk, convertedino, &(vpath[cur]));
                        else {
                            set_err(-1);     //no permission
                            return NULL;
                        } 
                    }
                }
            }
        }
    }
    // cannot find dir or not dir type.
    return NULL;
}

// Change current work directory through absolute path. NULL for failed, while errno=-1 means no permission, errno=-3 means not an abspath.
inode* chdir_abs(block *disk, const char *vpath) {
    if (vpath[0] != '/') {
        set_err(-3);
        return NULL;
    }
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
                && disk[bno].dirblk.fileitemTable[idx].valid == true) {
                if (ino->i_flag == INODE_DIR) {
                    if(check_permission(ino, &current_user, ugroupTable, PERM_X))
                        return chdir_rel(disk, ino, &(vpath[cur]));
                    else {
                        set_err(-1);
                        return NULL;
                    }
                } else if (ino->i_flag == INODE_SYMLINK) {
                    inode *convertedino = get_symlink_dest(disk, ino);
                    if (!convertedino || convertedino->i_flag != INODE_DIR) {
                        return NULL;
                    } else {
                        if(check_permission(convertedino, &current_user, ugroupTable, PERM_X))
                            return chdir_rel(disk, convertedino, &(vpath[cur]));
                        else {
                            set_err(-1);     //no permission
                            return NULL;
                        } 
                    }       
                }
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

// Create new dir in the directory. Returns new dir inode, NULL for failed, while errno=1 means no permission.
inode* mkdir(block *disk, inode *dir, const char *dirname) {
    if (!check_permission(dir, &current_user, ugroupTable, PERM_W)) {
        set_err(-1);
        return NULL;
    }
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
        chmod(disk, newinoid, "111111101");
        // TODO: set gid
        newino->di_uid = current_user.uid;
        return newino;
    }
    // find file with same filename.
    return NULL;
}

// Find file or dir inode in directory, NULL for failed, while errno=-1 means no permission. 
inode* find_in_dir(block *disk, inode *dir, const char *filename) {
    if (current_user.uid != ROOT_UID && !check_permission(dir, &current_user, ugroupTable, PERM_R)) {
        set_err(-1);
        return NULL;
    }
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

// Find file or dir recursively in current directory. The file path will be put into buffer. Returns total path count.
int find_r_in_dir(block *disk, inode *dir, const char *currentpath, const char *filename, char *buffer) {
    if (!check_permission(dir, &current_user, ugroupTable, PERM_R)) {
        set_err(-1);
        return -1;
    }
    int count = 0;
    int cur = 0;
    for (int bnoidx=0; bnoidx<dir->di_blkcount; ++bnoidx) {
        unsigned int bno = dir->di_addr[bnoidx];
        for (int idx=2; idx<DIRFILEMAXCOUNT; ++idx) {   //skip '.' and '..'
            if (disk[bno].dirblk.fileitemTable[idx].valid) {
                if (strcmp(disk[bno].dirblk.fileitemTable[idx].filename, filename) == 0) {
                    cur += sprintf(&buffer[cur], "%s/", currentpath);
                    cur += sprintf(&buffer[cur], "%s\n", disk[bno].dirblk.fileitemTable[idx].filename);
                    ++count;
                }
                inode *ino = get_inode(disk, disk[bno].dirblk.fileitemTable[idx].d_inode.di_ino);
                if (ino->i_flag == INODE_DIR) {
                    if (!check_permission(ino, &current_user, ugroupTable, PERM_R)) {
                        continue;
                    }
                    char newpath[strlen(currentpath) + strlen(disk[bno].dirblk.fileitemTable[idx].filename) + 2];
                    newpath[0] = 0;
                    strcat(newpath, currentpath);
                    strcat(newpath, "/");
                    strcat(newpath, disk[bno].dirblk.fileitemTable[idx].filename);
                    count += find_r_in_dir(disk, ino, newpath, filename, buffer + cur);
                    cur = strlen(buffer);
                }
            }
        }
    }
    return count; 
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

// Create new file in the directory, NULL for failed, while errno=-1 means no permission, errno=-2 means existing file.
inode* touch(block *disk, inode *dir, const char *filename) {
    if (current_user.uid != ROOT_UID && !check_permission(dir, &current_user, ugroupTable, PERM_W)) {
        set_err(-1);
        return NULL;
    }
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
        chmod(disk, newinoid, "111111101");
        newino->di_uid = current_user.uid;
        // TODO: set gid
        return newino;
    }
    // find file with same filename.
    set_err(-2);
    return NULL;
}

// Write or append string to file. -1 for failed, -2 for beyond largest size, -3 for no available block, -4 for no permission.
int echo(block *disk, inode* file, const char *content, const enum echo_mode mode) {
    if (file->i_flag == INODE_SYMLINK) {
        file = get_symlink_dest(disk, file);
        if (!file) return -1;
    }
    if (file->i_flag != INODE_FILE) return -1;
    if (current_user.uid != ROOT_UID && !check_permission(file, &current_user, ugroupTable, PERM_W)) {
        return -4;
    }
    switch (mode)
    {
    case ECHO_W:
        {
            if (strlen(content) == 0) {
                // free other blocks
                for (int i=1; i<file->di_blkcount; ++i) {
                    free_block(disk, file->di_addr[i]);
                }
                file->di_blkcount = 1;
                file->i_size = 0;
                return 0;
            }
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
                file->i_size = strlen(content)+2;
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
            if (strlen(content) == 0) return 0;
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

// Read file content to buffer. -1 for failed, -2 for no permission.
int cat(block *disk, inode* file, char *buffer, const unsigned int buffer_size) {
    if (file->i_flag == INODE_SYMLINK) {
        file = get_symlink_dest(disk, file);
        if (!file) {
            if (get_errno() == -1) {
                return -2;
            }
            return -1;
        }
    }
    if (file->i_flag != INODE_FILE) return -1;
    if (current_user.uid != ROOT_UID && !check_permission(file, &current_user, ugroupTable, PERM_R)) {
        return -2;
    }
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

// Delete the empty directory. Returns free bytes, 0 for delete dir but has hard link, -1 for not found, -2 for removing self or parent dir, -3 for not empty, -4 for no permission.
int rmdir(block *disk, inode *cwd, const char *dirname) {
    if (current_user.uid != ROOT_UID && !check_permission(cwd, &current_user, ugroupTable, PERM_W)) {
        return -4;
    }
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

    free_fileitem(dirfileitem);
    //start removing
    if (--(dirinode->i_count) == 0) {
        int count = 0;
        for (int blkidx=0; blkidx<dirinode->di_blkcount; ++blkidx) {
            free_block(disk, dirinode->di_addr[blkidx]);
            ++count;
        }
        dirinode->di_blkcount = 0;
        free_inode(disk, dirinode);
        return count * BLKSIZE;
    }
    return 0;
}

// Remove file. 0 for delete inode but still has hard link, -1 for not found, -2 for no permission.
int rm(block *disk, inode* dir, const char *filename) {
    inode *fileinode = find_in_dir(disk, dir, filename);
    if (!fileinode || (fileinode->i_flag != INODE_FILE && fileinode->i_flag != INODE_SYMLINK)) return -1;
    if (current_user.uid != ROOT_UID && !check_permission(dir, &current_user, ugroupTable, PERM_W)) {
        return -2;
    }
    fileitem *delfileitem = get_file_fileitem(disk, dir, filename);
    free_fileitem(delfileitem);
    if ((--(fileinode->i_count)) == 0) {
        int count = 0;
        for (int blkidx=0; blkidx<fileinode->di_blkcount; ++blkidx) {
            free_block(disk, fileinode->di_addr[blkidx]);
            ++count;
        }
        fileinode->di_blkcount = 0;
        free_inode(disk, fileinode);
        return count * BLKSIZE;
    }
    return 0;
}

// Check user's permission. True for pass, False for no permission.
bool check_permission(inode *file, user_t *user, ugroup_t *uGrpTable, const enum perm_mode mode) {
    if (user->uid == ROOT_UID) return true;
    if (user->uid == file->di_uid) {
        // owner
        return file->di_mode[mode];
    } else if (check_user_in_group(user, uGrpTable, file->di_gid)) {
        // same group
        return file->di_mode[3 + mode];
    } else {
        // others
        return file->di_mode[6 + mode];
    }
}

// Create a symlink to path. NULL for failed, while errno=-1 means no permission, errno=-2 means existing file, errno=-3 means not an abspath.
inode *create_symlink(block *disk, inode *dir, const char *filename, const char *abspath) {
    if (!check_permission(dir, &current_user, ugroupTable, PERM_W)) {
        set_err(-1);
        return NULL;
    }
    inode *dest = get_abspath_inode(disk, abspath);
    if (!dest) {
        switch (get_errno())
        {
        case -1:
            set_err(-1);
            break;
        case -3:
            set_err(-3);
            break;
        default:
            set_err(0);
            break;
        }
        return NULL;
    }
    inode *linkinode = touch(disk, dir, filename);
    if (!linkinode) {
        switch (get_errno())
        {
        case -1:
            set_err(-1);
            break;
        case -2:
            set_err(-2);
            break;
        default:
            set_err(0);
            break;
        }
        return NULL;
    }
    echo(disk, linkinode, abspath, ECHO_W);
    if (dest->i_flag == INODE_DIR && abspath[strlen(abspath)-1] != '/') {
        echo(disk, linkinode, "/", ECHO_A);
    }
    chmod(disk, linkinode->i_ino, "111111111");
    linkinode->i_flag = INODE_SYMLINK;
    return linkinode;
}

// Modify a symlink to path. NULL for failed, while errno=-1 means no permission, errno=-3 means not an abspath.
inode *modify_symlink(block *disk, inode *symlinkinode, const char *newabspath) {
    if (!check_permission(symlinkinode, &current_user, ugroupTable, PERM_W)) {
        set_err(-1);
        return NULL;
    }
    inode *dest = get_abspath_inode(disk, newabspath);
    if (!dest) {
        switch (get_errno())
        {
        case -1:
            set_err(-1);
            break;
        case -3:
            set_err(-3);
            break;
        default:
            set_err(0);
            break;
        }
        return NULL;
    }
    symlinkinode->i_flag = INODE_FILE;
    echo(disk, symlinkinode, newabspath, ECHO_W);
    if (dest->i_flag == INODE_DIR && newabspath[strlen(newabspath)-1] != '/') {
        echo(disk, symlinkinode, "/", ECHO_A);
    }
    symlinkinode->i_flag = INODE_SYMLINK;
    return symlinkinode;
}

// Create a hardlink to dest. NULL for failed, while errno=-1 means no permission, errno=-2 means existing file, errno=-3 means no available fileitem, errno=-4 means destfile is not a file.
// TODO: need to handle path including symlink
fileitem *create_hardlink(block *disk, inode *dir, const char *filename, inode *dest) {
    if (!check_permission(dir, &current_user, ugroupTable, PERM_W)) {
        set_err(-1);
        return NULL;
    }
    if (find_in_dir(disk, dir, filename)) {
        set_err(-2);
        return NULL;
    }
    if (dest->i_flag != INODE_FILE) {
        set_err(-4);
        return NULL;
    }
    fileitem *newfileitem = get_available_fileitem(disk, dir);
    if (!newfileitem) {
        set_err(-3);
        return NULL;
    }
    newfileitem->d_inode.di_ino = dest->i_ino;
    strcpy(newfileitem->filename, filename);
    ++dest->i_count;
    newfileitem->valid = 1; 
    return newfileitem;
}

// Get file or dir inode through absolute path. NULL for failed, while errno=-1 means no permission, errno=-2 means not a directory, errno=-3 means not an abspath.
inode *get_abspath_inode(block *disk, const char *abspath) {
    if (abspath[0] != '/') {
        set_err(-3);
        return NULL;
    }
    inode *rootino = get_inode(disk, ROOTINODEID);
    if (strlen(abspath) == 1 && abspath[0] == '/') return rootino;
    char buffer[MAXFILENAMELEN] = {'\0'};
    int cur = 0;
    int buffcur = 0;
    if (abspath[cur] == '/') ++cur;
    for (cur; abspath[cur]!='/' && abspath[cur]!='\0'; ++cur, ++buffcur) {
        buffer[buffcur] = abspath[cur];
    }
    for (int bnoidx=0; bnoidx<rootino->di_blkcount; ++bnoidx) {
        unsigned int bno = rootino->di_addr[bnoidx];
        for (int idx=0; idx<DIRFILEMAXCOUNT; ++idx) {
            inode *ino = get_inode(disk, disk[bno].dirblk.fileitemTable[idx].d_inode.di_ino);
            if (strcmp(disk[bno].dirblk.fileitemTable[idx].filename, buffer) == 0 
                && disk[bno].dirblk.fileitemTable[idx].valid == true) {
                if (ino->i_flag == INODE_DIR) {
                    if(check_permission(ino, &current_user, ugroupTable, PERM_X))
                        return get_relpath_inode(disk, ino, &(abspath[cur]));
                    else {
                        set_err(-1);
                        return NULL;
                    }
                } else if (ino->i_flag == INODE_FILE) {
                    if (abspath[cur] != '\0') {
                        set_err(-2);    // not a directory
                        return NULL;
                    } else {
                        return ino;
                    }
                }
            }
        }
    }
    // cannot find dest inode.
    return NULL;
}

// Get file or dir inode through relative path. NULL for failed, while errno=-1 means no permission, errno=-2 means not a directory.
inode* get_relpath_inode(block *disk, inode *cwd, const char *relpath) {
    if (relpath[0] == '\0') return cwd;
    char buffer[MAXFILENAMELEN] = {'\0'};
    int cur = 0;
    int buffcur = 0;
    if (relpath[cur] == '/') ++cur;
    if (relpath[cur] == '\0') return cwd;
    for (cur; relpath[cur]!='/' && relpath[cur]!='\0'; ++cur, ++buffcur) {
        buffer[buffcur] = relpath[cur];
    }
    for (int bnoidx=0; bnoidx<cwd->di_blkcount; ++bnoidx) {
        unsigned int bno = cwd->di_addr[bnoidx];
        for (int idx=0; idx<DIRFILEMAXCOUNT; ++idx) {
            inode *ino = get_inode(disk, disk[bno].dirblk.fileitemTable[idx].d_inode.di_ino);
            if (strcmp(disk[bno].dirblk.fileitemTable[idx].filename, buffer) == 0 
                && disk[bno].dirblk.fileitemTable[idx].valid == true) {
                if (ino->i_flag == INODE_DIR) {
                    if(check_permission(ino, &current_user, ugroupTable, PERM_X))
                        return get_relpath_inode(disk, ino, &(relpath[cur]));
                    else {
                        set_err(-1);
                        return NULL;
                    }
                } else if (ino->i_flag == INODE_FILE) {
                    if (relpath[cur] != '\0') {
                        set_err(-2);    // not a directory
                        return NULL;
                    } else {
                        return ino;
                    }
                }
            }
        }
    }
    // cannot find dest inode.
    return NULL;
}

// Get dest file or dir of symlink file. NULL for failed, while errno=-1 means no permission, errno=-2 means not a symlink.
inode* get_symlink_dest(block *disk, inode *symlink) {
    if (symlink->i_flag != INODE_SYMLINK) {
        set_err(-2);
        return NULL;
    }
    char buffer[NADDR*BLKSIZE];
    symlink->i_flag = INODE_FILE;
    if (cat(disk, symlink, buffer, NADDR*BLKSIZE) < 0) {
        symlink->i_flag = INODE_SYMLINK;
        return NULL;
    }
    symlink->i_flag = INODE_SYMLINK;
    inode *dest = get_abspath_inode(disk, buffer);
    if (!dest && get_errno() == -1) {
        set_err(-1);
        return NULL;
    }
    return dest;
}

// Move file(not dir or symlink) to the dest path. NULL for failed, while errno=-1 means no permission, errno=-2 means no src file, errno=-3 means path error, errno=-4 means dest dir full, -5 means no destname error, -6 means dest file existing.
fileitem* mv(block *disk, inode *dir, const char *srcname, const char *destpath, const char *destname) {
    fileitem *mvfile = get_file_fileitem(disk, dir, srcname);
    if (!mvfile || get_inode(disk, mvfile->d_inode.di_ino)->i_flag != INODE_FILE) {
        set_err(-2);
        return NULL;
    }
    inode *destdir = NULL;
    if (destpath[0] == '/') {
        destdir = chdir_abs(disk, destpath);
    } else {
        destdir = chdir_rel(disk, dir, destpath);
    }
    if (destdir == NULL) {
        switch (get_errno())
        {
        case -1:
            set_err(-1);
            break;
        default:
            set_err(-3);
            break;
        }
        return NULL;
    } 
    if (!check_permission(dir, &current_user, ugroupTable, PERM_W) || !check_permission(destdir, &current_user, ugroupTable, PERM_W)) {
        set_err(-1);
        return NULL;
    }
    if (strlen(destname) == 0) {
        set_err(-5);
        return NULL;
    } else if (find_in_dir(disk, destdir, destname)) {
        set_err(-6);
        return NULL;
    }
    fileitem *newfileitem = get_available_fileitem(disk, destdir);
    if (!newfileitem) {
        set_err(-4);
        return NULL;
    }
    newfileitem->d_inode.di_ino = mvfile->d_inode.di_ino;
    strcpy(newfileitem->filename, destname);
    newfileitem->valid = 1;
    free_fileitem(mvfile);
    return newfileitem;
}

// Copy file to the dest path. NULL for failed, while errno=-1 means no permission, errno=-2 means no src file, errno=-3 means path error, errno=-4 means dest dir full, -5 means no destname error, -6 means dest file existing, -7 means inode full, -8 means read or write error.
inode* cp(block *disk, inode *dir, const char *srcname, const char *destpath, const char *destname) {
    // TODO: untested
    inode *cpfile = find_in_dir(disk, dir, srcname);
    if (!cpfile || cpfile->i_flag != INODE_FILE) {
        set_err(-2);
        return NULL;
    }
    if (!check_permission(cpfile, &current_user, ugroupTable, PERM_R)) {
        set_err(-1);
        return NULL;
    }
    inode *destdir = NULL;
    if (destpath[0] == '/') {
        destdir = chdir_abs(disk, destpath);
    } else {
        destdir = chdir_rel(disk, dir, destpath);
    }
    if (destdir == NULL) {
        switch (get_errno())
        {
        case -1:
            set_err(-1);
            break;
        default:
            set_err(-3);
            break;
        }
        return NULL;
    } 
    if (!check_permission(destdir, &current_user, ugroupTable, PERM_W)) {
        set_err(-1);
        return NULL;
    }
    if (strlen(destname) == 0) {
        set_err(-5);
        return NULL;
    } else if (find_in_dir(disk, destdir, destname)) {
        set_err(-6);
        return NULL;
    }
    int newinoid = icalloc(disk);
    if (!newinoid) {
        // alloc inode failed
        set_err(-7);
        return NULL;
    }
    inode *newino = get_inode(disk, newinoid);
    char buffer[NADDR * BLKSIZE];
    // set newinode props
    newino->di_uid = current_user.uid;
    newino->i_ctime = newino->i_mtime = time(NULL);
    newino->i_flag = INODE_FILE;
    chmod(disk, newinoid, "111111101");
    if (cat(disk, cpfile, buffer, NADDR * BLKSIZE) == -1 || echo(disk, newino, buffer, ECHO_W) != 0) {
        //free new inode
        free_inode(disk, newino);
        set_err(-8);
        //write failed
        return NULL;
    }
    // create fileitem in dest dir
    fileitem *newfileitem = get_available_fileitem(disk, destdir);
    if (!newfileitem) {
        free_inode(disk, newino);
        set_err(-4);
        return NULL;
    }
    newfileitem->d_inode.di_ino = newinoid;
    strcpy(newfileitem->filename, destname);
    newfileitem->valid = 1;
    return newino;
}