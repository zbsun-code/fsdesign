#pragma once
#include <iostream>
#include <stack>
#include <time.h>
#include <iomanip>
#include "filesystem.h"
#include "user.h"

using namespace std;

extern int list_dir(block *disk, inode *ino);
extern void wait_for_command();
extern void wait_for_init_command();
extern string get_dir_absolute_path(block *disk, inode *dir);
extern void login_process(block *disk);