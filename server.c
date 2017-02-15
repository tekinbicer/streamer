//   Reply server in C


#include "zhelpers.h"

int main (int argc, char *argv[])
{
  if(argc!=4) {
    printf("Usage: %s <ip-to-bind=164.54.143.3> <starting-port=5560> number-of-workers\n", argv[0]);
    exit(0);
  }

  int port = atoi(argv[2]);
  int n_workers = atoi(argv[3]);

  void *context = zmq_ctx_new();
  void **workers = malloc(n_workers*sizeof(workers));

  // Check workers
  for(int i=0; i<n_workers; ++i){
    workers[i]=zmq_socket(context, ZMQ_REP);
    char addr[64];
    snprintf(addr, 64, "tcp://%s:%d", argv[1], port++);
    zmq_bind(workers[i], addr);
  }

  // Get worker's rank and store
  int worker_ids[n_workers];
  for(int i=0; i<n_workers; ++i){
   char *str = s_recv(workers[i]);
   printf("%s is alive.\n", str);
   worker_ids[i]=atoi(str);
   free(str);
  }

  // At this point I know every one is alive
  // Send msg/projections
  for(int i=0; i<n_workers; ++i){
    char msg[64];
    snprintf(msg, 64, "socket=%d worker_rank=%d; some projections!!", i, worker_ids[i]);
    s_send(workers[i], msg);
  }

  // Check if they received it
  for(int i=0; i<n_workers; ++i){
   char *str = s_recv(workers[i]);
   printf("worker rank=%d on socket=%d replied: %s\n", worker_ids[i], i, str);
   free(str);
  }

  for(int i=0; i<n_workers; ++i){
    zmq_close (workers[i]);
  }
  
  zmq_ctx_destroy (context);
  free(workers);

  return 0;
}
