#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <getopt.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>

#include "gfclient.h"
#include "workload.h"
#include "gfclient-student.h"

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  webclient [options]\n"                                                     \
"options:\n"                                                                  \
"  -h                  Show this help message\n"                              \
"  -n [num_requests]   Requests download per thread (Default: 4)\n"           \
"  -p [server_port]    Server port (Default: 6200)\n"                         \
"  -s [server_addr]    Server address (Default: 127.0.0.1)\n"                 \
"  -t [nthreads]       Number of threads (Default 4)\n"                       \
"  -w [workload_path]  Path to workload file (Default: workload.txt)\n"       \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"help",          no_argument,            NULL,           'h'},
  {"nthreads",      required_argument,      NULL,           't'},
  {"nrequests",     required_argument,      NULL,           'n'},
  {"server",        required_argument,      NULL,           's'},
  {"port",          required_argument,      NULL,           'p'},
  {"workload-path", required_argument,      NULL,           'w'},
  {NULL,            0,                      NULL,             0}
};

static void Usage() {
	fprintf(stdout, "%s", USAGE);
}

static void localPath(char *req_path, char *local_path){
  static int counter = 0;

  sprintf(local_path, "%s-%06d", &req_path[1], counter++);
}

static FILE* openFile(char *path){
  char *cur, *prev;
  FILE *ans;

  /* Make the directory if it isn't there */
  prev = path;
  while(NULL != (cur = strchr(prev+1, '/'))){
    *cur = '\0';

    if (0 > mkdir(&path[0], S_IRWXU)){
      if (errno != EEXIST){
        perror("Unable to create directory");
        exit(EXIT_FAILURE);
      }
    }

    *cur = '/';
    prev = cur;
  }

  if( NULL == (ans = fopen(&path[0], "w"))){
    perror("Unable to open file");
    exit(EXIT_FAILURE);
  }

  return ans;
}

/* Callbacks ========================================================= */
static void writecb(void* data, size_t data_len, void *arg){
  FILE *file = (FILE*) arg;

  fwrite(data, 1, data_len, file);
}

void *gfc_worker(void *args)
{
    gfcrcom_t *gfcr = args;
    int returncode = 0;
    int nrequests = gfcr->nrequests;

    while (nrequests--)
    {
        pthread_mutex_lock(&gfcw_mutex);
        char *req_path = workload_get_path();
        pthread_mutex_unlock(&gfcw_mutex);

        if(strlen(req_path) > 512){
          fprintf(stderr, "Request path exceeded maximum of 512 characters\n.");
          exit(EXIT_FAILURE);
        }

        char local_path[1024];
        localPath(req_path, local_path);
        FILE *file = openFile(local_path);

        gfcrequest_t *gfr = gfc_create();
        gfc_set_server(gfr, gfcr->server);
        gfc_set_path(gfr, req_path);
        gfc_set_port(gfr, gfcr->port);
        gfc_set_writefunc(gfr, writecb);
        gfc_set_writearg(gfr, file);

        fprintf(stdout, "Requesting %s%s\n", gfcr->server, req_path);

        if (0 > (returncode = gfc_perform(gfr)))
        {
            fprintf(stdout, "gfc_perform returned an error %d\n", returncode);
            fclose(file);
            if (0 > unlink(local_path))
                fprintf(stderr, "unlink failed on %s\n", local_path);
        }
        else
            fclose(file);

        if (gfc_get_status(gfr) != GF_OK && unlink(local_path) < 0)
            fprintf(stderr, "unlink failed on %s\n", local_path);

        fprintf(stdout, "Status: %s\n", gfc_strstatus(gfc_get_status(gfr)));
        fprintf(stdout, "Received %zu of %zu bytes\n", gfc_get_bytesreceived(gfr), gfc_get_filelen(gfr));

        gfc_cleanup(gfr);
    }
    return NULL;
}

/* Main ========================================================= */
int main(int argc, char **argv) {
/* COMMAND LINE OPTIONS ============================================= */
  char *server = "localhost";
  unsigned short port = 6200;
  char *workload_path = "workload.txt";

  int i = 0;
  int option_char = 0;
  int nthreads = 4;
  int nrequests = 4;

  setbuf(stdout, NULL); // disable caching

  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "hn:p:s:t:w:x", gLongOptions, NULL)) != -1) {
    switch (option_char) {
      case 'h': // help
        Usage();
        exit(0);
        break;                      
      case 'n': // nrequests
        nrequests = atoi(optarg);
        nrequests = nrequests > 0 ? nrequests : 4;
        break;
      case 'p': // port
        port = atoi(optarg);
        break;
      default:
        Usage();
        exit(1);
      case 's': // server
        server = optarg;
        break;
      case 't': // nthreads
        nthreads = atoi(optarg);
        break;
      case 'w': // workload-path
        workload_path = optarg;
        break;
    }
  }

  if( EXIT_SUCCESS != workload_init(workload_path)){
    fprintf(stderr, "Unable to load workload file %s.\n", workload_path);
    exit(EXIT_FAILURE);
  }

  gfc_global_init();

  pthread_t gfcthread[nthreads];
  gfcrcom_t gfcr = {server, port, nrequests};
  for(i = 0; i < nthreads; i++){
      if (pthread_create(&gfcthread[i], NULL, gfc_worker, &gfcr)) {
          fprintf(stderr, "Error creating thread\n");
          return 1;
      }
  }

  for (i = 0; i < nthreads; i++)
  {
    if (pthread_join(gfcthread[i], NULL))
    {
      fprintf(stderr, "Error joining thread\n");
      return 2;
    }
  }

  gfc_global_cleanup();

  return 0;
}
