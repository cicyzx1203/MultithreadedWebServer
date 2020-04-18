#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>
#include <sys/signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <pthread.h>

#include "gfserver.h"
#include "content.h"

#include "gfserver-student.h"

#define BUFSIZE 4096

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  gfserver_main [options]\n"                                                 \
"options:\n"                                                                  \
"  -t [nthreads]       Number of threads (Default: 8)\n"                      \
"  -p [listen_port]    Listen port (Default: 6200)\n"                         \
"  -m [content_file]   Content file mapping keys to content files\n"          \
"  -h                  Show this help message.\n"                             \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"port",          required_argument,      NULL,           'p'},
  {"nthreads",      required_argument,      NULL,           't'},
  {"content",       required_argument,      NULL,           'm'},
  {"help",          no_argument,            NULL,           'h'},
  {NULL,            0,                      NULL,             0}
};


extern ssize_t getfile_handler(gfcontext_t *ctx, char *path, void* arg);

static void _sig_handler(int signo){
  if (signo == SIGINT || signo == SIGTERM){
    exit(signo);
  }
}

void *gft_process()
{
    for (;;) 
    {
        struct stat file_stat;
        char buf[BUFSIZE];

        pthread_mutex_lock(&req_mutex);
        if (steque_isempty(&reqQ))
        {
            pthread_mutex_unlock(&req_mutex);
            continue;
        }

        gfrequest_t *gfr = steque_pop(&reqQ); 
        pthread_mutex_unlock(&req_mutex);

        if (gfr == NULL)
            continue;

        int fd = content_get(gfr->requestedPath);
        if (fstat(fd, &file_stat) < -1)
        {
           fprintf(stderr, "Error fstat --> %s", strerror(errno));
           exit(EXIT_FAILURE);
        }
        fprintf(stdout, "File Size: %u bytes\n", (unsigned)file_stat.st_size);

        gfs_sendheader(gfr->context, GF_OK, file_stat.st_size);

        int remain_data = file_stat.st_size;
        if (remain_data == 0)
            gfs_abort(gfr->context);
   
        off_t offset = 0;

        while (remain_data > 0)
        {
            pthread_mutex_lock(&fa_mutex);
            ssize_t len = pread(fd, buf, BUFSIZE, offset);
            pthread_mutex_unlock(&fa_mutex);

            if (len <= 0)
                break;

            char *ptr = buf;
            while (len > 0)
            {
                int sent_bytes = gfs_send(gfr->context, ptr, len);
                remain_data -= sent_bytes;
                len -= sent_bytes;
                ptr += len;
                offset += sent_bytes;
                printf("Server sent %d bytes, offset %lu remaining %d\n",
                       sent_bytes, offset, remain_data);
            }
        }
        free(gfr);
    }
}

/* Main ========================================================= */
int main(int argc, char **argv) {
  int option_char = 0;
  unsigned short port = 6200;
  char *content_map = "content.txt";
  gfserver_t *gfs = NULL;
  int nthreads = 8;

  setbuf(stdout, NULL);

  if (signal(SIGINT, _sig_handler) == SIG_ERR){
    fprintf(stderr,"Can't catch SIGINT...exiting.\n");
    exit(EXIT_FAILURE);
  }

  if (signal(SIGTERM, _sig_handler) == SIG_ERR){
    fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
    exit(EXIT_FAILURE);
  }

  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "m:p:t:hx", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      case 'p': // listen-port
        port = atoi(optarg);
        break;
      case 't': // nthreads
        nthreads = atoi(optarg);
        break;
      default:
        fprintf(stderr, "%s", USAGE);
        exit(1);
      case 'h': // help
        fprintf(stdout, "%s", USAGE);
        exit(0);
        break;       
      case 'm': // file-path
        content_map = optarg;
        break;                                          
    }
  }

  /* not useful, but it ensures the initial code builds without warnings */
  if (nthreads < 1) {
    nthreads = 1;
  }
  
  content_init(content_map);

  /*Initializing server*/
  gfs = gfserver_create();

  /*Setting options*/
  gfserver_set_port(gfs, port);
  gfserver_set_maxpending(gfs, 64);
  gfserver_set_handler(gfs, getfile_handler);
  gfserver_set_handlerarg(gfs, NULL); // doesn't have to be NULL!

  //rq_init();
  steque_init(&reqQ);

  if (pthread_mutex_init(&req_mutex, NULL) != 0)
  {
      printf("\n req_mutex init failed.\n");
      return -1;
  }

  if (pthread_mutex_init(&fa_mutex, NULL) != 0)
  {
      printf("\n fa_mutex init failed.\n");
      return -1;
  }

  pthread_t gfthread;
  for (int i = 0; i < nthreads; i++) {
      if (pthread_create(&gfthread, NULL, gft_process, NULL)) {
          fprintf(stderr, "Error creating thread\n");
          return 1;
      }
  }
   
  /*Loops forever*/
  gfserver_serve(gfs);
}
