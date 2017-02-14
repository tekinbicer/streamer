//   Request-reply client in C
//   Connects REQ socket to tcp://localhost:5559
//   Sends "Hello" to server, expects "World" back


#include "zhelpers.h"

#define N_WORKERS 32
#define S_PORT 5560

int main (int argc, char *argv[])
{
  if(argc!=3) {
    printf("Usage: %s starting-port number-of-workers\n", argv[0]);
    exit(0);
  }

  int port = atoi(argv[1]);
  int n_workers = atoi(argv[2]);

  void *context = zmq_ctx_new();
  void **reqs = malloc(n_workers*sizeof(void*));

  // Check workers
  for(int i=0; i<n_workers; ++i){
    reqs[i]=zmq_socket(context, ZMQ_REQ);
    char addr[64];
    snprintf(addr, 64, "tcp://localhost:%d", port++);
    zmq_connect(reqs[i], addr);
    s_send(reqs[i], "Hello");
  }

  // Get worker's rank and store
  int reqs_ids[n_workers];
  for(int i=0; i<n_workers; ++i){
   char *str = s_recv(reqs[i]);
   printf("%s is alive.\n", str);
   reqs_ids[i]=atoi(str);
   free(str);
  }

  // At this point I know every one is alive
  // Send msg/projections
  for(int i=0; i<n_workers; ++i){
    char msg[64];
    snprintf(msg, 64, "hi %d", reqs_ids[i]);
    s_send(reqs[i], msg);
  }

  // Check if they received it
  for(int i=0; i<n_workers; ++i){
   char *str = s_recv(reqs[i]);
   printf("worker msg is received: %s.\n", str);
   free(str);
  }

  for(int i=0; i<n_workers; ++i){
    zmq_close (reqs[i]);
  }
  
  zmq_ctx_destroy (context);
  free(reqs);

  return 0;
}
