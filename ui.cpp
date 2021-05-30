#include "ui.h"

using namespace std;

block *disk = NULL;
inode *cwd = NULL;

int main() {
    // // init
    // if (format(5000, "./data.disk") == 0) {
    //     cout << "format succeeded!" << endl;
    // } else {
    //     cout << "format failed!" << endl;
    // }
    // disk = mount("./data.disk");
    // init_userinfo(disk);
    // string rootpwd = "";
    // cout << "Enter new root password: ";
    // cin >> rootpwd;
    // cwd = chdir_to_root(disk);
    // init_rootuser(disk, rootpwd.c_str());
    // savetofile(disk, "./data.disk");
    // init finished
    disk = mount("./data.disk");
    SUDO(load_uinfo(disk, userTable, userBitMap));
    user_t *loginuser;
    string username, password;
    do {
        cout << "Enter username: ";
        cin >> username;
        cout << "Enter password: ";
        cin >> password;
        loginuser = login(userTable, username.c_str(), password.c_str());
        if (!loginuser) cout << "Login failed! Try again." << endl;
    } while (loginuser == NULL);
    current_user = *loginuser;
    SUDO(cwd = chdir_abs(disk, "/"));
    while(1) {
        wait_for_command();
    }
    return 0;
}

void wait_for_command() {
    cout << current_user.username << "@THIS-PC:";
    cout << get_dir_absolute_path(disk, cwd);
    if (current_user.uid == 1) {
        //root
        cout << "# ";
    } else {
        cout << "$ ";
    }
    string buffer;
    cin >> buffer;
    if (buffer == "ls") {
        if (list_dir(disk, cwd) == -1) {
            cout << "command: ls: " << buffer << ": Permission denied" << endl;
        }
    } else if (buffer == "chmod") {
        string filename, modestr;
        cin >> filename;
        cin >> modestr;    //input 1 or 0 for 9 times.
        inode *ino = find_in_dir(disk, cwd, filename.c_str());
        if (!ino) {
            cout << "command: chmod: " << filename << ": No such file or directory" << endl;
        } else {
            if (chmod(disk, ino->i_ino, modestr.c_str()) == -1) {
                cout << "command: chmod: " << filename << ": Permission denied" << endl;
            }
        }
    } else if (buffer == "cd") {
        cin >> buffer;
        inode *newcwd = NULL;
        if (buffer[0] == '/') {
            newcwd = chdir_abs(disk, buffer.c_str());
        } else {
            newcwd = chdir_rel(disk, cwd, buffer.c_str());
        }
        if (newcwd == NULL) {
            if (get_errno() == -1) {
                cout << "command: cd: " << buffer << ": Permission denied" << endl;
            } else {
                cout << "command: cd: " << buffer << ": No such directory" << endl;
            }
        } else {
            cwd = newcwd;
        }
    } else if (buffer == "mkdir") {
        cin >> buffer;
        if (mkdir(disk, cwd, buffer.c_str()) == NULL) {
            if (get_errno() == -1) {
                cout << "command: mkdir: " << buffer << ": Permission denied" << endl;
            } else {
                cout << "command: mkdir: " << buffer << ": Create failed! Existing same filename or disk full" << endl;
            }
        }
    } else if (buffer == "touch") {
        cin >> buffer;
        if (touch(disk, cwd, buffer.c_str()) == NULL) {
            if (get_errno() == -1) {
                cout << "command: mkdir: " << buffer << ": Permission denied" << endl;
            } else {
                cout << "command: mkdir: " << buffer << ": Create failed! Existing same filename or disk full" << endl;
            }
        }
    } else if (buffer == "echo") {
        string content = "";
        getline(cin, content, '\"');
        getline(cin, content, '\"');
        cin >> buffer;
        int ret = 0;
        if (buffer == ">") {
            cin >> buffer;
            inode *file = find_in_dir(disk, cwd, buffer.c_str());
            if (!file || file->i_flag != INODE_FILE) {
                cout << "command: echo: " << buffer << ": Write failed! Unexisting file!" << endl; 
            } else {
                ret = echo(disk, file, content.c_str(), ECHO_W);
            }
        } else if (buffer == ">>") {
            cin >> buffer;
            inode *file = find_in_dir(disk, cwd, buffer.c_str());
            if (!file || file->i_flag != INODE_FILE) {
                cout << "command: echo: " << buffer << ": Write failed! Unexisting file!" << endl; 
            } else {
                ret = echo(disk, file, content.c_str(), ECHO_A);
            }
        }
        if (ret != 0) {
            switch (ret)
            {
            case -1:
                cout << "command: echo: " << buffer << ": Write failed! Please check for more information" << endl; 
                break;
            case -2:
                cout << "command: echo: " << buffer << ": Write failed! File beyond largest size" << endl; 
                break;
            case -3:
                cout << "command: echo: " << buffer << ": Write failed! Disk full" << endl; 
                break;
            case -4:
                cout << "command: echo: " << buffer << ": Permission Denied" << endl; 
                break;
            }
        }
    } else if (buffer == "cat") {
        cin >> buffer;
        inode *file = find_in_dir(disk, cwd, buffer.c_str());
        if (!file || file->i_flag != INODE_FILE) {
            cout << "command: cat: " << buffer << ": Read failed! Unexisting file!" << endl;
        } else {
            char filebuffer[NADDR * BLKSIZE];
            int size = cat(disk, file, filebuffer, NADDR * BLKSIZE);
            if (size == -1) {
                cout << "command: cat: " << buffer << ": Read failed! Invalid file!" << endl;
            } else if (size == -2) {
                cout << "command: cat: " << buffer << ": Permission denied" << endl;
            } else {
                int count = 0;
                for (int i=0; i<size; ++i) {
                    cout << filebuffer[i];
                    ++count;
                }
                cout << endl;
                // cout << "total bytes: " << count << endl;
            }
        }
    } else if (buffer == "rmdir") {
        cin >> buffer;
        int size = rmdir(disk, cwd, buffer.c_str());
        switch (size)
        {
        case -1:
            cout << "command: rmdir: " << buffer << ": No such directory" << endl;
            break;
        case -2:
            cout << "command: rmdir: " << buffer << ": Cannot remove self or parent directory" << endl;
            break;
        case -3:
            cout << "command: rmdir: " << buffer << ": Directory is not empty" << endl;
            break;
        case -4:
            cout << "command: rmdir: " << buffer << ": Permission denied" << endl;
            break;
        } 
    } else if (buffer == "rm") {
        cin >> buffer;
        int size = rm(disk, cwd, buffer.c_str());
        switch (size)
        {
        case -1:
            cout << "command: rm: " << buffer << ": No such file" << endl;
            break;
        case -2:
            cout << "command: rm: " << buffer << ": Permission denied" << endl;
            break;
        }      
    } else if (buffer == "adduser") {
        string username, password;
        cin >> username >> password;
        user_t newuser;
        strcpy(newuser.username, username.c_str());
        strcpy(newuser.password, password.c_str());
        newuser.valid = 1;
        int ret = create_user(disk, userTable, userBitMap, &newuser);
        if (ret == 0) {
            cout << "command: adduser: " << username << ": Cannot create user. Please check for information" << endl;
        } else if (ret == -1) {
            cout << "command: adduser: " << username << ": Cannot create user. Existing duplicate user" << endl;
        } else if (ret == -2) {
            cout << "command: adduser: " << username << ": Permission denied" << endl;
        }
    } else if (buffer == "ln") {
        cin >> buffer;
        inode *link = NULL;
        if (buffer == "-s") {
            string filename, destpath;
            cin >> filename >> destpath;
            link = create_symlink(disk, cwd, filename.c_str(), destpath.c_str());
        } else if (buffer == "-snf") {
            string filename, destpath;
            cin >> filename >> destpath;
            inode *linkfile = find_in_dir(disk, cwd, filename.c_str());
            if (!linkfile) {
                if (get_errno() == -1) {
                    cout << "command: ln: " << filename << ": Permission denied" << endl;
                } else {
                    cout << "command: ln: " << filename << ": No such file" << endl;
                }
                return;
            } else if (linkfile->i_flag != INODE_SYMLINK) {
                cout << "command: ln: " << filename << ": No such file" << endl;
                return;
            }
            link = modify_symlink(disk, linkfile, destpath.c_str());
        } else {
            // hard link
            string filename = buffer;
            string dest;
            cin >> dest;
            // TODO: unfinished
            return;
        }

    } else if (buffer == "su") {
        cin >> buffer;
        int count = 0;
        string password;
        user_t *newuser = NULL;
        while (count < 3) {
            cout << "Enter password: ";
            cin >> password;
            newuser = login(userTable, buffer.c_str(), password.c_str());
            if (newuser != NULL) {
                current_user = *newuser;
                if (current_user.uid != ROOT_UID) {
                    string path = "/home/" + string(current_user.username);
                    cwd = chdir_abs(disk, path.c_str());
                }
                break;
            }
            ++count;
            cout << "Login failed. Try again" << endl;
        }
        if (newuser == NULL) cout << "command: su: " << buffer << ": Login failed. Incorrect password or unexisting user" << endl;
    } else {
        cout << buffer << ": command not found" << endl;
    }
}

ostream& operator<<(ostream& os, bool* mode) {
    for (int i=0; i<9; ++i) {
        if (i % 3 == 0) {
            mode[i]? os << 'r' : os << '-';
        } else if (i % 3 == 1) {
            mode[i]? os << 'w' : os << '-';
        } else {
            mode[i]? os << 'x' : os << '-';
        }
    }
    return os;
}

// List dir items, returns item count; -1 for no permission.
int list_dir(block *disk, inode *ino) {
    if (!check_permission(ino, &current_user, ugroupTable, PERM_R)) {
        return -1;
    }
    int count = 0;
    for (int bnoidx=0; bnoidx<ino->di_blkcount; ++bnoidx) {
        unsigned int bno = ino->di_addr[bnoidx];
        for (int idx=0; idx<DIRFILEMAXCOUNT; ++idx) {
            fileitem *item = &(disk[bno].dirblk.fileitemTable[idx]);
            inode *ino = get_inode(disk, item->d_inode.di_ino);
            if (item->valid) {
                ++count;
                char mtime[128] = {0};
                strcpy(mtime, ctime(&(ino->i_mtime)));
                mtime[strlen(mtime)-1] = '\0';
                cout << ino->i_ino << '\t' << ino->di_mode << ' ' 
                    << ino->i_count << ' ' << ino->di_uid << ' ' 
                    << ino->di_gid << ' ' << ino->i_size << '\t' 
                    << mtime << ' ' << item->filename;
                if (ino->i_flag == INODE_DIR) {
                    cout << '/';
                } else if (ino->i_flag == INODE_SYMLINK) {
                    char buffer[NADDR * BLKSIZE];
                    cat(disk, ino, buffer, NADDR * BLKSIZE);
                    cout << " -> \'" << buffer << "\'";
                }
                cout << endl;
            }
        }
    }
    return count;
}

// Get absolute path of the dir.
string get_dir_absolute_path(block *disk, inode *dir) {
    stack<string> dirnames;
    inode *pinode = dir;
    while (pinode->i_ino != ROOTINODEID) {
        fileitem *item = get_dir_fileitem(disk, pinode);
        dirnames.push(item->filename);
        pinode = get_inode(disk, disk[pinode->di_addr[0]].dirblk.fileitemTable[1].d_inode.di_ino);
    }
    string str = "";
    if (dirnames.empty()) str.append("/");
    while (!dirnames.empty()) {
        str.append("/");
        str.append(dirnames.top());
        dirnames.pop();
    }
    return str;
}

