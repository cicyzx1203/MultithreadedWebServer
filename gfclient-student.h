/*
 *  This file is for use by students to define anything they wish.  It is used by the gf client implementation
 */
 #ifndef __GF_CLIENT_STUDENT_H__
 #define __GF_CLIENT_STUDENT_H__
 
 #include <pthread.h>

 #include "gf-student.h"

 pthread_mutex_t gfcw_mutex;
 
 typedef struct gfcrcom_t {
    char *server;
    unsigned short port;
    int nrequests;
 } gfcrcom_t;

 #endif // __GF_CLIENT_STUDENT_H__
