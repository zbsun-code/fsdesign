#pragma once
#include <stdbool.h>
#include "filesystem.h"
#include "user_struct.h"

#define SUDO(unit)      {    user_t tmpsaveuser = current_user; \
                            strcpy(current_user.username, "root"); \
                            current_user.valid = 1; \
                            current_user.uid = 1; \
                            unit; \
                            current_user = tmpsaveuser; \
                        }

extern int init_userinfo(block *disk);
extern int init_rootuser(block *disk, const char *pwd);
extern int create_user(block *disk, user_t *uTable, bool *uBitMap, user_t *newuser);
extern int load_uinfo(block *disk, user_t *uTable, bool *uBitMap);
extern int save_uinfo(block *disk, user_t *uTable);
extern user_t* login(user_t *uTable, const char *username, const char *pwd);