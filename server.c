//   Reply server in C
#include "zhelpers.h"
#include "streamer.h"

#define N 16
#define P 18

/* Generates worker msgs from a set of projections.
 *
 * data: pointer to generated data [projection][column][column]
 * dims: dimensions of the data, consists of 3 int values
 * theta: pointer to theta values of projections
 * n_procs: number of processes
 *
 * returns msgs to be sent to the processes. &tomo_msg_t[P][ranks_msg]
 * */
tomo_msg_t*** generate_msgs(int16_t *data, int *dims, float *theta, int n_ranks){
  int nsin = dims[1]/n_ranks;
  int c_remaining = dims[1]%n_ranks;

  tomo_msg_t ***msgs = malloc(P*sizeof(tomo_msg_t**));
  for(int i=0; i<P; ++i) msgs[i] = malloc(n_ranks*sizeof(tomo_msg_t*));

  int curr_sinogram_id = 0;
  for(int i=0; i<P; ++i){
    int remaining = c_remaining;
    curr_sinogram_id = 0;

    for(int j=0; j<n_ranks; ++j){
      int r = ((remaining--) > 0) ? 1 : 0;
      size_t data_size = sizeof(*data)*(nsin+r)*dims[2];
      tomo_msg_t *msg = malloc(sizeof(tomo_msg_t)+data_size);
     
      msg->type = TRACE_DATA;
      msg->projection_id = i;
      msg->theta = theta[i];
      msg->beg_sinogram = curr_sinogram_id;
      msg->n_sinogram = nsin+r;
      msg->n_rays_per_proj_col = dims[2];
      memcpy(msg->data, (data+i*dims[2]*dims[2]) + // Current projection
                         curr_sinogram_id*dims[2], // Current sinogram
                        data_size);
      msgs[i][j]=msg;
      curr_sinogram_id += (nsin+r);
    }
  }
  return msgs;
}

/* Generates worker messages from a given projection.
 *
 * data: Pointer to the beginning of projection data.
 * dims: Dimensions of the projection. Typically consists of { N, N }.
 * data_id: Projection id.
 * theta: Rotation of this projection.
 * n_ranks: Number of ranks this projection will be distributed.
 *
 * Returns
 * tomo_msg_t** msgs: Ranks' messages. E.g. msgs[rank_id] points to rank_id's msg.  
 *
 */
tomo_msg_t** generate_worker_msgs(int16_t *data, int dims[], int data_id, float theta, int n_ranks)
{
  int nsin = dims[0]/n_ranks;
  int remaining = dims[0]%n_ranks;

  tomo_msg_t **msgs = malloc(n_ranks*sizeof(tomo_msg_t*));

  int curr_sinogram_id = 0;
  for(int i=0; i<n_ranks; ++i){
    int r = ((remaining--) > 0) ? 1 : 0;
    size_t data_size = sizeof(*data)*(nsin+r)*dims[1];
    tomo_msg_t *msg = malloc(sizeof(tomo_msg_t)+data_size);

    msg->type = TRACE_DATA;
    msg->projection_id = data_id;
    msg->theta = theta;
    msg->beg_sinogram = curr_sinogram_id;
    msg->n_sinogram = nsin+r;
    msg->n_rays_per_proj_col = dims[1];
    memcpy(msg->data, data+curr_sinogram_id*dims[1], // Current sinogram
                      data_size);
    msgs[i]=msg;
    curr_sinogram_id += (nsin+r);
  }
  return msgs;
}

int main (int argc, char *argv[])
{
  if(argc!=3) {
    printf("Usage: %s <ip-to-bind=164.54.143.3> <starting-port=5560>\n", argv[0]);
    exit(0);
  }

  int port = atoi(argv[2]);


  /// Figure out how many ranks there is at the remote location
  void *context = zmq_ctx_new();
  void *main_worker = zmq_socket(context, ZMQ_REP);
  char addr[64];
  snprintf(addr, 64, "tcp://%s:%d", argv[1], port++);
  zmq_bind(main_worker, addr);
  char *world_rank_str = s_recv(main_worker);
  int n_workers = atoi(world_rank_str);
  free(world_rank_str);
  printf("Number of workers=%d\n", n_workers);
  s_send(main_worker, "received");


  /// Setup projection data
  int dims[3]= {P, N, N};
  int16_t *data = malloc(sizeof(*data)*P*N*N); assert(data!=NULL);
  float *theta = malloc(P*sizeof(*theta)); assert(theta!=NULL);
  for(int i=0; i<P; ++i)
    for(int j=0; j<N; ++j)
      for(int k=0; k<N; ++k) data[i*N*N+j*N+k]=i*N*N+j*N+k;
  for(int i=0; i<P; ++i) theta[i]=(360./P)*i;
  

  /// Setup system for other workers
  void **workers = malloc(n_workers*sizeof(void*)); assert(workers!=NULL);
  workers[0] = main_worker; /// We already talked with the main worker
  for(int i=1; i<n_workers; ++i){
    void *worker = zmq_socket(context, ZMQ_REP);
    workers[i] = worker;
    char addr[64];
    snprintf(addr, 64, "tcp://%s:%d", argv[1], port++);
    zmq_bind(workers[i], addr);
  }


  /// Get workers' ranks and store them
  int worker_ids[n_workers];
  for(int i=0; i<n_workers; ++i){
   char *str = s_recv(workers[i]);
   printf("%s is alive.\n", str);
   worker_ids[i]=atoi(str);
   free(str);
  }


  /// Distribute data to workers
  for(int i=0; i<dims[0]; ++i) {  // For each incoming projection
    /// Generate worker messages
    int16_t *curr_projection = data + i*dims[1]*dims[2];
    int projection_dims[2] = { dims[1], dims[2] };
    float proj_theta = theta[i];
    tomo_msg_t **worker_msgs = generate_worker_msgs(curr_projection, projection_dims, i, proj_theta, n_workers);

    /// Send partitioned projection data to workers 
    for(int j=0; j<n_workers; ++j){
      /// Prepare zmq message
      tomo_msg_t *curr_msg = worker_msgs[j];
      size_t msg_size = sizeof(*curr_msg) +            /// struct size
                        curr_msg->n_sinogram*          /// remaining data size
                        curr_msg->n_rays_per_proj_col*
                        sizeof(*(curr_msg->data));
      zmq_msg_t msg;
      int rc = zmq_msg_init_size(&msg, msg_size); assert(rc==0);
      memcpy((void*)zmq_msg_data(&msg), (void*)curr_msg, msg_size);
      rc = zmq_msg_send(&msg, workers[j], 0); assert(rc==msg_size);
    }

    /// Check if workers received their corresponding data chunks
    for(int j=0; j<n_workers; ++j){
     char *str = s_recv(workers[j]);
     printf("worker rank=%d on socket=%d replied: %s\n", worker_ids[j], j, str);
     free(str);
    }

    /// Clean-up data chunks
    for(int j=0; j<n_workers; ++j)
      free(worker_msgs[j]);
    free(worker_msgs);
  }

  /// All the projections are finished
  /// Send a finished signal to the workers before closing the sockets
  for(int i=0; i<n_workers; ++i){
    zmq_msg_t msg;
    tomo_msg_t msg_fin = { .type = TRACE_FIN };
    int rc = zmq_msg_init_size(&msg, sizeof(msg_fin)); assert(rc==0);
    memcpy((void*)zmq_msg_data(&msg), (void*)&msg_fin, sizeof(msg_fin));
    rc = zmq_msg_send(&msg, workers[i], 0); assert(rc==sizeof(msg_fin));
  }
  /// Check if workers received their fin messages
  for(int i=0; i<n_workers; ++i){
   char *str = s_recv(workers[i]);
   printf("worker rank=%d on socket=%d replied: %s\n", worker_ids[i], i, str);
   free(str);
  }

  /// Cleanup resources
  for(int i=0; i<n_workers; ++i){
    zmq_close (workers[i]);
  }
  zmq_ctx_destroy (context);
  free(workers);

  free(data);
  free(theta);

  return 0;
}
