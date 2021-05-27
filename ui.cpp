#include <time.h>
#include "ui.h"
#include "filesystem.h"

using namespace std;

block *disk = NULL;
inode *cwd = NULL;

int main() {
    // if (format(5000, "./data.disk") == 0) {
    //     cout << "format succeeded!" << endl;
    // } else {
    //     cout << "format failed!" << endl;
    // }
    disk = mount("./data.disk");
    cwd = chdir_to_root(disk);
    while(1) {
        wait_for_command();
    }
    return 0;
}

void wait_for_command() {
    cout << "root@THIS-PC:";
    cout << get_dir_absolute_path(disk, cwd) << "# ";
    string buffer;
    cin >> buffer;
    if (buffer == "ls") {
        list_dir(disk, cwd);
    } else if (buffer == "cd") {
        cin >> buffer;
        inode *newcwd = NULL;
        if (buffer[0] == '/') {
            newcwd = chdir_abs(disk, buffer.c_str());
        } else {
            newcwd = chdir_rel(disk, cwd, buffer.c_str());
        }
        if (newcwd != NULL) {
            cwd = newcwd;
        } else {
            cout << "command: cd: " << buffer << ": No such directory" << endl;
        }
    } else if (buffer == "mkdir") {
        cin >> buffer;
        if (mkdir(disk, cwd, buffer.c_str()) == NULL) {
            cout << "command: mkdir: " << buffer << ": Create failed! Existing same filename or disk full" << endl;
        }
    } else if (buffer == "touch") {
        cin >> buffer;
        if (touch(disk, cwd, buffer.c_str()) == NULL) {
            cout << "command: mkdir: " << buffer << ": Create failed! Existing same filename or disk full" << endl;
        }
    } else if (buffer == "echo") {
        string content = "";
        getline(cin, content, '\"');
        getline(cin, content, '\"');
        cin >> buffer;
        if (buffer == ">") {
            cin >> buffer;
            inode *file = find_in_dir(disk, cwd, buffer.c_str());
            if (!file || file->i_flag != INODE_FILE) {
                cout << "command: echo: " << buffer << ": Write failed! Unexisting file!" << endl; 
            } else {
                echo(disk, file, content.c_str(), ECHO_W);
            }
        } else if (buffer == ">>") {
            cin >> buffer;
            inode *file = find_in_dir(disk, cwd, buffer.c_str());
            if (!file || file->i_flag != INODE_FILE) {
                cout << "command: echo: " << buffer << ": Write failed! Unexisting file!" << endl; 
            } else {
                echo(disk, file, content.c_str(), ECHO_A);
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
            }
            int count = 0;
            for (int i=0; i<size; ++i) {
                cout << filebuffer[i];
                ++count;
            }
            cout << endl;
            // cout << "total bytes: " << count << endl;
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
        default:
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
        default:
            break;
        }      
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

// List dir items, returns item count;
int list_dir(block *disk, inode *ino) {
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
                if (ino->i_flag == INODE_DIR) cout << '/';
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

