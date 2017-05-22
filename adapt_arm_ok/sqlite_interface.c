#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sqlite_interface.h"

static sqlite3 *db = NULL;

int non_null(char *str)
{
    return ((str != NULL) && (strlen(str) != 0));
}

int zk_sqlite3_open()
{
    if(sqlite3_open(INTERACTION_DB, &db) != SQLITE_OK)
    {   
        printf("%s open failed.\n", INTERACTION_DB);
        perror("");
        exit(-1);
    }   
    return 0;
}

int zk_sqlite3_close()
{
    if(sqlite3_close(db) != SQLITE_OK)
    {   
        printf("can't close datebase.\n");
        perror("");
        exit(-1);
    }   
    db = NULL;
    return 0;
}

int zk_sqlite3_exec(char *sql, int (*call_back)(void*,int,char**,char**), void *call_back_param)
{
    char *errmsg = NULL;

    if((db != NULL) && non_null(sql))
    {   
        if(sqlite3_exec(db, sql, call_back, call_back_param, &errmsg) != SQLITE_OK)
        {
            printf("%s error:%s\n",sql,errmsg);
        }
    }   
    sqlite3_free(errmsg);
    return 0;
}

int zk_sqlite3_query_mutex(char *** dbresult,char *sql,int *n_row, int *n_column)
{
    char *errmsg = NULL;
    //zk_sqlite3_open(INTERACTION_DB,&db);    
    //pthread_mutex_lock(&(csn_interface.csn_sqlite_db_mutex));

    if((db != NULL) && non_null(sql))
    {
        while(1)
        {

            if(sqlite3_get_table(db, sql, dbresult, n_row, n_column, &errmsg) != SQLITE_OK)
            {

                if(strstr(errmsg,"database is locked"))
                {
                    printf("%s error:%s,wait until unlocked\n",sql,errmsg);
                    usleep(1000);//1ms
                    continue;
                }else{
                    printf("%s:%s error:%s\n",__FUNCTION__,sql,errmsg);
                    sqlite3_free(errmsg);
                    //pthread_mutex_unlock(&(csn_interface.csn_sqlite_db_mutex));
                    return -1;
                }    
            }else {
//                printf("%s ok!!\n",sql);
                break;
            }    
        }    
    }else{
        printf("db or sql is null!!\n");
        //pthread_mutex_unlock(&(csn_interface.csn_sqlite_db_mutex));
        return -1;
    }    
    if(!errmsg)
        sqlite3_free(errmsg);

    //pthread_mutex_unlock(&(csn_interface.csn_sqlite_db_mutex));
    return 0;

}

void zk_get_value(char *key,char *value)
{
    char **dbresult = NULL;
    int n_row,n_column,ret;
    char sql[256];
    sprintf(sql,"select * from config where key=\"%s\"",key);
    ret = zk_sqlite3_query_mutex(&dbresult,sql,&n_row, &n_column);
    if((dbresult == NULL)||(ret == -1)){
        printf("#######zk_sqlite3_query_mutex malloc error\r\n");
        if(dbresult != NULL)
            sqlite3_free_table(dbresult);
        dbresult = NULL;
        return ;
    }
    if(n_row > 0){  
        //printf("%s=%s\n",dbresult[2],dbresult[3]);
        strcpy(value,dbresult[3]);
    }
}

void zk_get_user_id(char *user_id)
{
    zk_get_value("user_id",user_id);
}

void zk_get_server(char *server_ip,unsigned short *port)
{
    zk_get_value("server_ip",server_ip);
    char port_str[16];
    zk_get_value("server_port",port_str);
    *port = atoi(port_str);
}
