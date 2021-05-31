#include "user.h"

user_t current_user = {0};
user_t userTable[MAXUSERNUM] = {0};
bool userBitMap[MAXUSERNUM+1] = {0};
ugroup_t ugroupTable[MAXGROUPNUM] = {0};

// Create "/etc/userinfo/user_uninitialized" for newly formatted disk.
int init_userinfo(block *disk) {
    inode *rootinode = get_inode(disk, ROOTINODEID);
    mkdir(disk, rootinode, "etc");
    inode *etc = chdir_abs(disk, "/etc");
    mkdir(disk, etc, "userinfo");
    inode *userinfodir = chdir_abs(disk, "/etc/userinfo");
    inode *uinfofile = touch(disk, userinfodir, "user_uninitialized");
    chmod(disk, userinfodir->i_ino, "111111000");
    chmod(disk, uinfofile->i_ino, "111111000");
    chmod(disk, etc->i_ino, "111111100");
    return 0;
}

// Set root password for the newly initialized uinfo.
int init_rootuser(block *disk, const char *pwd) {
    inode *userinfodir = chdir_abs(disk, "/etc/userinfo");
    if (find_in_dir(disk, userinfodir, "user_uninitialized") != NULL) {
        rm(disk, userinfodir, "user_uninitialized");
        inode *uinfo = touch(disk, userinfodir, "uinfo");
        user_t root;
        strcpy(root.username, "root");
        strcpy(root.password, pwd);
        root.uid = 1;   //0 for no user.
        userTable[0] = root;
        userBitMap[root.uid] = true;
        char uidstr[10];
        sprintf(uidstr, "%hu\t", root.uid);
        echo(disk, uinfo, uidstr, ECHO_A);
        echo(disk, uinfo, root.username, ECHO_A);
        echo(disk, uinfo, "\t", ECHO_A);
        echo(disk, uinfo, root.password, ECHO_A);
        echo(disk, uinfo, "\n", ECHO_A);
    }
    return 0;
}

// Load uinfo from the disk to the user table.
int load_uinfo(block *disk, user_t *uTable, bool *uBitMap) {
    inode *userinfodir = chdir_abs(disk, "/etc/userinfo");
    inode *uinfo = find_in_dir(disk, userinfodir, "uinfo");
    // clean uTable
    memset(uTable, 0, sizeof(user_t)*MAXUSERNUM);
    // read uinfo
    char buffer[MAXUSERNUM*sizeof(user_t)];
    int size = cat(disk, uinfo, buffer, MAXUSERNUM*sizeof(user_t));
    int offset = 0;
    int readsize = 0;
    int idx = 0;
    int count = 0;
    while (offset < size - 2) {
        user_t u;
        sscanf(buffer + offset, "%hu\t%n", &u.uid, &readsize), offset += readsize;
        sscanf(buffer + offset, "%s\t%n", u.username, &readsize), offset += readsize;
        sscanf(buffer + offset, "%s\n%n", u.password, &readsize), offset += readsize;
        u.valid = true;
        uTable[idx] = u;
        uBitMap[u.uid] = true;
        ++idx;
        ++count;
    }
    return count;
}

// Save uinfo to the disk.
int save_uinfo(block *disk, user_t *uTable) {
    inode *userinfodir = chdir_abs(disk, "/etc/userinfo");
    inode *uinfo = find_in_dir(disk, userinfodir, "uinfo");
    // clear the uinfo file
    echo(disk, uinfo, "", ECHO_W);
    int count = 0;
    char buffer[50];
    for (int idx=0; idx<MAXUSERNUM; ++idx) {
        if (uTable[idx].valid) {
            sprintf(buffer, "%hu\t", uTable[idx].uid);
            echo(disk, uinfo, buffer, ECHO_A);
            echo(disk, uinfo, uTable[idx].username, ECHO_A);
            echo(disk, uinfo, "\t", ECHO_A);
            echo(disk, uinfo, uTable[idx].password, ECHO_A);
            echo(disk, uinfo, "\n", ECHO_A);
            ++count;
        }
    }
    return count;
}


// Create new user and alloc new uid. 1 for success, 0 for failed, -1 for duplicate user, -2 for no permission.
int create_user(block *disk, user_t *uTable, bool *uBitMap, user_t *newuser) {
    if (current_user.uid != ROOT_UID) {
        return -2;
    }
    int availcur = -1;
    for (int idx=0; idx<MAXUSERNUM; ++idx) {
        if (availcur == -1 && uTable[idx].valid == false) {
            availcur = idx;
        }
        if (strcmp(uTable[idx].username, newuser->username) == 0) return -1;
    }
    if (availcur == -1) return 0;
    unsigned short uid = 0;
    for (int idx=1; idx<MAXUSERNUM+1; ++idx) {
        if (uBitMap[idx] == false) {
            uid = idx;
            newuser->uid = uid;
            break;
        }
    }
    if (uid == 0) return 0;
    for (int idx=0; idx<MAXUSERNUM; ++idx) {
        if (uTable[idx].valid == false) {
            uTable[idx] = *newuser;
            uBitMap[uid] = true;
            SUDO(
                save_uinfo(disk, uTable);
                // create home/username dir
                inode *rootdir = chdir_abs(disk, "/");
                inode *homedir = find_in_dir(disk, rootdir, "home");
                if (!homedir) {
                    homedir = mkdir(disk, rootdir, "home");
                } 
                inode *userhomedir = mkdir(disk, homedir, newuser->username);
                chmod(disk, userhomedir->i_ino, "111111000");
                chown(disk, userhomedir->i_ino, newuser->uid);
            );
            return 1;
        }
    }
    save_uinfo(disk, uTable);

    return 0;
}

// Login user. Returns ptr to user_t, NULL for failed.
user_t* login(user_t *uTable, const char *username, const char *pwd) {
    for (int idx=0; idx<MAXUSERNUM; ++idx) {
        if (uTable[idx].valid && strcmp(uTable[idx].username, username) == 0 && strcmp(uTable[idx].password, pwd) == 0) {
            return &uTable[idx];
        }
    }
    return NULL;
}

bool check_user_in_group(user_t *user, ugroup_t *uGrpTable, int gid) {
    // TODO: unfinished
    return false;
}

user_t* get_user(user_t *uTable, unsigned short uid) {
    for (int i=0; i<MAXUSERNUM; ++i) {
        if (uTable[i].valid && uTable[i].uid == uid) {
            return &uTable[i];
        }
    }
    return NULL;
}

user_t* get_user_s(user_t *uTable, const char *username) {
    for (int i=0; i<MAXUSERNUM; ++i) {
        if (uTable[i].valid && strcmp(uTable[i].username, username) == 0) {
            return &uTable[i];
        }
    }
    return NULL;
}

