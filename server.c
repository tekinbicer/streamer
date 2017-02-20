//   Reply server in C
#include "zhelpers.h"

#define N 1024
#define P 180

typedef struct {
  float theta; // theta value of this projection
  int beg_sinogram; // beginning sinogram id
  int n_sinogram; // # Sinograms
  // int nprojs=1; // Always projection by projection
  int n_rays_per_proj_col; // # rays per projection column
  int16_t *data;  // real projection data
                  // number of rays in data=n_sinogram*n_rays_per_proj_col
} tomo_msg_t;

/*
 * data: pointer to generated data [projection][column][column]
 * dims: dimensions of the data, consists of 3 int values
 * theta: pointer to theta values of projections
 * n_procs: number of processes
 *
 * returns msgs to be sent to the processes. tomo_msg_t[P][ranks_msg]
 * */
tomo_msg_t** generate_msgs(int16_t *data, int *dims, float *theta, int n_ranks){
  int nsin = dims[1]/n_ranks;
  int c_remaining = dims[1]%n_ranks;

  tomo_msg_t **msgs = malloc(P*sizeof(*msgs));
  for(int i=0; i<P; ++i) msgs[i] = malloc(n_ranks*sizeof(*msgs));

  int curr_sinogram_id = 0;
  for(int i=0; i<P; ++i){
    int remaining = c_remaining;
    curr_sinogram_id = 0;

    for(int j=0; j<n_ranks; ++j){
      msgs[i][j].theta = theta[i];
      msgs[i][j].beg_sinogram = curr_sinogram_id;
      int r = ((remaining--) > 0) ? 1 : 0;
      msgs[i][j].n_sinogram = nsin+r;
      msgs[i][j].n_rays_per_proj_col = dims[2];
      size_t msg_size = sizeof(*data)*(nsin+r)*dims[2];
      msgs[i][j].data = malloc(msg_size);
      memcpy(msgs[i][j].data, (data+i*dims[2]*dims[2]) + // Current projection
                                     curr_sinogram_id*dims[2], 
                                     msg_size);  // Current sinogram
      curr_sinogram_id += nsin+r;
    }
  }
  return msgs;
}

int main (int argc, char *argv[])
{
  if(argc!=4) {
    printf("Usage: %s <ip-to-bind=164.54.143.3> <starting-port=5560> number-of-workers\n", argv[0]);
    exit(0);
  }

  int port = atoi(argv[2]);
  int n_workers = atoi(argv[3]);

  void *context = zmq_ctx_new();
  void **workers = malloc(n_workers*sizeof(workers)); assert(workers!=NULL);

  int dims[3]= {P, N, N};
  int16_t *data = malloc(sizeof(*data)*P*N*N); assert(data!=NULL);
  float *theta = malloc(P*sizeof(*theta)); assert(theta!=NULL);
  for(int i=0; i<P; ++i)
    for(int j=0; j<N; ++j)
      for(int k=0; k<N; ++k) data[i*N*N+j*N+k]=k;

  for(int i=0; i<P; ++i) theta[i]=(360./P)*i;
  
  // Prepare messages
  tomo_msg_t **msgs = generate_msgs(data, dims, theta, n_workers); assert(msgs!=NULL);
  free(data);
  free(theta);

  // Check workers
  for(int i=0; i<n_workers; ++i){
    workers[i] = zmq_socket(context, ZMQ_REP);
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

  // TODO: Below two loop is not implemented at client side yet!!
  // At this point I know every one is alive
  // Receive/Send msg/projections
  for(int i=0; i<dims[0]; ++i) {  // For each incoming projection
    for(int j=0; j<n_workers; ++j){ // For each partitioned projection sinogram
      // Assume data is ready
      // Prepare zmq message
      zmq_msg_t msg;
      size_t msg_size = sizeof(msgs[i][j]) +            // struct size
                        msgs[i][j].n_sinogram*          // remaining data size
                        msgs[i][j].n_rays_per_proj_col*
                        sizeof(*(msgs[i][j].data));
      int rc = zmq_msg_init_size(&msg, msg_size); assert(rc==0);
      memcpy((void*)zmq_msg_data(&msg), (void*)&msgs[i][j], msg_size);
      rc = zmq_msg_send(&msg, workers[i], 0); assert(rc==0);
      // char msg[64];
      // snprintf(msg, 64, "socket=%d worker_rank=%d; some projections!!", i, worker_ids[i]);
      // s_send(workers[i], msg);
    }

    // Check if workers received their corresponding projection rows
    for(int i=0; i<n_workers; ++i){
     char *str = s_recv(workers[i]);
     printf("worker rank=%d on socket=%d replied: %s\n", worker_ids[i], i, str);
     free(str);
    }
    // Move on to next projection
  }
  // All the projections are finished
  // TODO: Send a finished signal to the workers before closing the sockets

  for(int i=0; i<n_workers; ++i){
    zmq_close (workers[i]);
  }
  
  zmq_ctx_destroy (context);
  free(workers);

  return 0;
}
