/*
 *  This file is for use by students to define anything they wish.  It is used by the gf server implementation
 */
#ifndef __GF_SERVER_STUDENT_H__
#define __GF_SERVER_STUDENT_H__

#include "gf-student.h"
#include "steque.h"

struct gfrequest_t{
  gfcontext_t *context;
  char *requestedPath;
};
typedef struct gfrequest_t gfrequest_t;

steque_t reqQ;

pthread_mutex_t req_mutex; /* request enque/deque mutex */
pthread_mutex_t fa_mutex;  /* file access mutex */

#endif // __GF_SERVER_STUDENT_H__
