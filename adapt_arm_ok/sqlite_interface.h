#ifndef _SQLITE_INTERFACE_H_
#define _SQLITE_INTERFACE_H_
#ifdef __cplusplus
extern "C"
{
#endif


#include "sqlite3.h"

#define INTERACTION_DB "/var/images/interaction.db"

int zk_sqlite3_open();
int zk_sqlite3_close();
int zk_sqlite3_exec(char *sql, int (*call_back)(void*,int,char**,char**), void *call_back_param);
void zk_get_user_id(char *user_id);
void zk_get_server(char *server_ip,unsigned short *port);

#endif
#ifdef __cplusplus
}
#endif
