#pragma once
#include <iostream>
#include <stack>
#include "filesystem.h"

using namespace std;

extern int list_dir(block *disk, inode *ino);
extern void wait_for_command();
extern string get_dir_absolute_path(block *disk, inode *dir);