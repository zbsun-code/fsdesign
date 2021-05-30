#pragma once
#define MAXUSERNAMELEN 8
#define MAXGROUPNAMELEN 8
#define MAXUSERPWDLEN  128
#define MAXGROUPNUM     32
#define MAXUSERNUM     100
#define MAXGIDLEN      5
#define ROOT_UID       1

typedef struct user {
    bool valid;
    unsigned short uid; //0 for no user
    char username[MAXUSERNAMELEN];
    char password[MAXUSERPWDLEN];
} user_t;

typedef struct ugroup {
    bool valid;
    unsigned short gid; //0 for no group
    char groupname[MAXGROUPNAMELEN];
    // user list

} ugroup_t;

extern user_t current_user;
extern user_t userTable[MAXUSERNUM];
extern ugroup_t ugroupTable[MAXGROUPNUM];
extern bool userBitMap[MAXUSERNUM+1];  // uid = 0 is never gonna happen 

extern bool check_user_in_group(user_t *user, ugroup_t *uGrpTable, int gid);