#include "ui.h"

using namespace std;

block *disk = NULL;
inode *cwd = NULL;

int main() {
    wait_for_init_command();
    login_process(disk);
    while(1) {
        wait_for_command();
    }
    return 0;
}

void wait_for_command() {
    cout << "\033[1m" << "\033[32m" << current_user.username << "@THIS-PC" << "\033[37m" << "\033[0m" << ":";
    cout << "\033[1m" << "\033[34m" << get_dir_absolute_path(disk, cwd) << "\033[37m" << "\033[0m";
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
    } else if (buffer == "chown") {
        string uname, path;
        cin >> uname >> path;
        user_t *u = get_user_s(userTable, uname.c_str());
        inode *fileino = (path[0] == '/')? get_abspath_inode(disk, path.c_str()) : get_relpath_inode(disk, cwd, path.c_str());
        if (!u) {
            cout << "command: chown: " << uname << ": No such user" << endl;
        } else if (!fileino) {
            cout << "command: chown: " << uname << ": No such file" << endl;
        } else {
            if (chown(disk, fileino->i_ino, u->uid) == -1) {
                cout << "command: chown: " << uname << ": Permission denied" << endl;
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
            if (!file || (file->i_flag != INODE_FILE && file->i_flag != INODE_SYMLINK)) {
                cout << "command: echo: " << buffer << ": Write failed! Unexisting file!" << endl; 
            } else {
                ret = echo(disk, file, content.c_str(), ECHO_W);
            }
        } else if (buffer == ">>") {
            cin >> buffer;
            inode *file = find_in_dir(disk, cwd, buffer.c_str());
            if (!file || (file->i_flag != INODE_FILE && file->i_flag != INODE_SYMLINK)) {
                cout << "command: echo: " << buffer << ": Write failed! Unexisting file!" << endl; 
            } else {
                ret = echo(disk, file, content.c_str(), ECHO_A);
            }
            switch (ret)
            {
            case -1:
                cout << "command: echo: " << buffer << ": Write failed! File not supported" << endl; 
                break;
            case -2:
                cout << "command: echo: " << buffer << ": Write failed! File size overflow" << endl; 
                break;    
            case -3:
                cout << "command: echo: " << buffer << ": Write failed! Disk full" << endl; 
                break;             
            case -4:
                cout << "command: echo: " << buffer << ": Permission denied" << endl; 
                break;             
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
        if (!file || (file->i_flag != INODE_FILE && file->i_flag != INODE_SYMLINK)) {
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
    } else if (buffer == "mv") {
        string src, dest;
        cin >> src >> dest;
        char path[dest.size()+1];
        strcpy(path, dest.c_str());
        char *dstname = strrchr(path, '/');
        fileitem *ret = NULL;
        if (!dstname) {
            dstname = path;
            ret = mv(disk, cwd, src.c_str(), ".", dstname);
        } else {
            *dstname = '\0';
            ++dstname;
            ret = mv(disk, cwd, src.c_str(), path, dstname);
        }
        if (!ret) {
            switch (get_errno())
            {
            case -1:
                cout << "command: mv: " << src << ": Permission denied" << endl;
                break;
            case -2:
                cout << "command: mv: " << src << ": No such file" << endl;
                break;
            case -3:
                cout << "command: mv: " << path << ": Path error" << endl;
                break;
            case -4:
                cout << "command: mv: " << path << ": Dest directory full" << endl;
                break;
            case -5:
                cout << "command: mv: " << dest << ": Dest file name error" << endl;
                break;
            case -6:
                cout << "command: mv: " << dest << ": File already exists" << endl;
                break;
            default:
                cout << "command: mv: " << path << ": Move file failed" << endl;
                break;
            }
        }
    } else if (buffer == "cp") {
        string src, dest;
        cin >> src >> dest;
        char path[dest.size()+1];
        strcpy(path, dest.c_str());
        char *dstname = strrchr(path, '/');
        inode *ret = NULL;
        if (!dstname) {
            dstname = path;
            ret = cp(disk, cwd, src.c_str(), ".", dstname);
        } else {
            *dstname = '\0';
            ++dstname;
            ret = cp(disk, cwd, src.c_str(), path, dstname);
        }
        if (!ret) {
            switch (get_errno())
            {
            case -1:
                cout << "command: cp: " << src << ": Permission denied" << endl;
                break;
            case -2:
                cout << "command: cp: " << src << ": No such file" << endl;
                break;
            case -3:
                cout << "command: cp: " << path << ": Path error" << endl;
                break;
            case -4:
                cout << "command: cp: " << path << ": Dest directory full" << endl;
                break;
            case -5:
                cout << "command: cp: " << dest << ": Dest file name error" << endl;
                break;
            case -6:
                cout << "command: cp: " << dest << ": File already exists" << endl;
                break;
            case -7:
                cout << "command: cp: " << dest << ": Disk inode full" << endl;
                break;
            case -8:
                cout << "command: cp: " << dest << ": Read or write error" << endl;
                break;
            default:
                cout << "command: cp: " << src << ": Move file failed" << endl;
                break;
            }
        }
    } else if (buffer == "find") {
        cin >> buffer;
        char path[NADDR * BLKSIZE] = {0};
        int ret = find_r_in_dir(disk, cwd, ".", buffer.c_str(), path);
        if (ret > 0) cout << path;
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
            if (!link) {
                switch (get_errno())
                {
                case -1:
                    cout << "command: ln: " << filename << ": Permission denied" << endl;
                    break;
                case -2:
                    cout << "command: ln: " << filename << ": Already existing file" << endl;
                    break;
                case -3:
                    cout << "command: ln: " << filename << ": Source path is not an absolute path" << endl;
                    break;
                default:
                    cout << "command: ln: " << filename << ": No such file" << endl;
                    break;
                }
                return;
            }
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
            if (!link) {
                switch (get_errno())
                {
                case -1:
                    cout << "command: ln: " << filename << ": Permission denied" << endl;
                    break;
                case -2:
                    cout << "command: ln: " << filename << ": Already existing file" << endl;
                    break;
                case -3:
                    cout << "command: ln: " << filename << ": Source path is not an absolute path" << endl;
                    break;
                default:
                    cout << "command: ln: " << filename << ": No such file" << endl;
                    break;
                }
                return;
            }
        } else {
            // hard link
            string filename = buffer;
            string dest;
            cin >> dest;
            inode *destino = NULL;
            if (dest[0] == '/') {
                destino = get_abspath_inode(disk, dest.c_str());
            } else {
                destino = get_relpath_inode(disk, cwd, dest.c_str());
            }
            fileitem *linkfile = create_hardlink(disk, cwd, filename.c_str(), destino);
            if (!linkfile) {
                switch (get_errno())
                {
                case -1:
                    cout << "command: ln: " << filename << ": Permission denied" << endl;
                    break;
                case -2:
                    cout << "command: ln: " << filename << ": Create failed! Existing same filename" << endl;
                    break;
                case -3:
                    cout << "command: ln: " << filename << ": Parent directory full" << endl;
                    break;
                case -4:
                    cout << "command: ln: " << filename << ": Source path is not a file" << endl;
                    break;
                }
            }
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
    } else if (buffer == "export") {
        cin >> buffer;
        if (savetofile(disk, buffer.c_str()) == -1) {
            cout << "command: export: " << buffer << ": Save failed" << endl;
        }
    } else if (buffer == "quit") {
        wait_for_init_command();
        login_process(disk);
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
                cout << ino->i_ino << '\t' << ino->di_mode << ' ' << setw(3) << left << ino->i_count << ' '; 
                user_t *owner = get_user(userTable, ino->di_uid);
                if (!owner) {
                    cout << setw(8) << left << "none" << ' ';
                } else {
                    cout << setw(8) << left << owner->username << ' ';
                }
                cout << ino->di_gid << ' ' << ino->i_size << '\t' << mtime << ' ';
                if (ino->i_flag == INODE_DIR) {
                    cout << "\033[1m" << "\033[34m" << item->filename << "\033[0m" << "\033[37m";
                    cout << '/';
                } else if (ino->i_flag == INODE_SYMLINK) {
                    cout << "\033[1m" << "\033[36m" << item->filename << "\033[0m" << "\033[37m";
                    char buffer[NADDR * BLKSIZE];
                    ino->i_flag = INODE_FILE;
                    cat(disk, ino, buffer, NADDR * BLKSIZE);
                    ino->i_flag = INODE_SYMLINK;
                    cout << " -> \'" << buffer << "\'";
                } else {
                    cout << item->filename;
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

void login_process(block *disk) {
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
}

void wait_for_init_command() {
    while (1) {
        cout << "$ ";
        string buffer;
        cin >> buffer;
        if (buffer == "import") {
            cin >> buffer;
            block *newdisk = mount(buffer.c_str());
            if (newdisk != NULL) {
                free(disk);
                disk = newdisk;
                current_user.valid = 1;
                current_user.uid = ROOT_UID;
                inode *uinfodir = chdir_abs(disk, "/etc/userinfo");
                if (!uinfodir || !find_in_dir(disk, uinfodir, "uinfo")) {
                    init_userinfo(disk);
                    string rootpwd = "";
                    cout << "Enter new root password: ";
                    cin >> rootpwd;
                    cwd = chdir_to_root(disk);
                    init_rootuser(disk, rootpwd.c_str());
                    savetofile(disk, buffer.c_str());
                }
                break;
            } else {
                cout << "command: import: " << buffer << ": Read failed" << endl;
            }
        } else if (buffer == "format") {
            unsigned int blknum;
            cin >> blknum;
            cin >> buffer;
            if (format(blknum, buffer.c_str()) == -1) {
                cout << "command: format: " << buffer << ": Write failed" << endl;
            }
        } else {
            cout << buffer << ": command not found" << endl;
        }
    }
}
