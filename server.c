//   Reply server in C
#include "zhelpers.h"
#include "streamer.h"

#define N 16
#define P 18

#define NX 256
#define NY 1
#define NZ 512

float* read_file(char *fn, size_t s){
  float *data = malloc(sizeof(float)*s);
  FILE *f;
  f = fopen(fn, "rb");
  if(!f){
    printf("Unable to open file!\n");
    return NULL;
  }
  fread(data, sizeof(float), s, f);

  fclose(f);
  return data;
}


tomo_msg_t** generate_tracemq_worker_msgs(
  float *data, int dims[], 
  int data_id, float theta, 
  int n_ranks, uint64_t seq)
{
  int nsin = dims[0]/n_ranks;
  int remaining = dims[0]%n_ranks;

  tomo_msg_t **msgs = malloc(n_ranks*sizeof(tomo_msg_t*));

  int curr_sinogram_id = 0;
  for(int i=0; i<n_ranks; ++i){
    int r = ((remaining--) > 0) ? 1 : 0;
    size_t data_size = sizeof(*data)*(nsin+r)*dims[1];
    tomo_msg_t *msg = tracemq_prepare_data_rep_msg(seq, 
                              data_id, theta, dims[1]/2., data_size, 
                              data+curr_sinogram_id*dims[1]);
    msgs[i] = msg;
    curr_sinogram_id += (nsin+r);
  }
  return msgs;
}

tomo_msg_data_info_rep_t assign_data(
  uint32_t comm_rank, int comm_size, 
  int tot_sino, int tot_cols)
{
  uint32_t nsino = tot_sino/comm_size;
  uint32_t remaining = tot_sino%comm_size;

  int r = (comm_rank<remaining) ? 1 : 0;
  int my_nsino = r+nsino;
  int beg_sino = (comm_rank<remaining) ? (1+nsino)*comm_rank : 
                                        (1+nsino)*remaining + nsino*(comm_rank-remaining);
  
  tomo_msg_data_info_rep_t info_rep;
  info_rep.tn_sinograms = tot_sino;
  info_rep.beg_sinogram = beg_sino;
  info_rep.n_sinograms = my_nsino;
  info_rep.n_rays_per_proj_row = tot_cols;

  return info_rep;
}

int main (int argc, char *argv[])
{
  if(argc!=3) {
    printf("Usage: %s <ip-to-bind=164.54.143.3> <starting-port=5560>\n", argv[0]);
    exit(0);
  }

  int port = atoi(argv[2]);


  /// Setup zmq
  void *context = zmq_ctx_new();

  /// Setup projection data
  //int dims[3]= {P, N, N};
  //float *data = malloc(sizeof(*data)*P*N*N); assert(data!=NULL);
  //float *theta = malloc(P*sizeof(*theta)); assert(theta!=NULL);
  //for(int i=0; i<P; ++i)
  //  for(int j=0; j<N; ++j)
  //    for(int k=0; k<N; ++k) data[i*N*N+j*N+k]=1.*i*N*N+j*N+k;
  //for(int i=0; i<P; ++i) theta[i]=(180./P)*i;
  int dims[3]= {NZ, NY, NX};
  float *data = read_file("projs", NZ*NY*NX); 
  float *theta = read_file("theta", NZ);
  for(int i=0; i<NZ; ++i){
    printf("proj=%d theta=%.3f; \n",i, theta[i]);
  }
  
  /// Figure out how many ranks there is at the remote location
  void *main_worker = zmq_socket(context, ZMQ_REP);
  char addr[64];
  snprintf(addr, 64, "tcp://%s:%d", argv[1], port++);
  zmq_bind(main_worker, addr);
  tomo_msg_t *msg = tracemq_recv_msg(main_worker);
  tomo_msg_data_info_req_t* info = tracemq_read_data_info_req(msg);
  /// Setup worker data structures
  int n_workers = info->comm_size;
  printf("n_workers=%d\n",n_workers);
  int worker_ids[n_workers];
  void **workers = malloc(n_workers*sizeof(void*)); assert(workers!=NULL);
  worker_ids[0] = info->comm_rank;
  tracemq_free_msg(msg);

  /// Setup remaining workers' sockets 
  workers[0] = main_worker; /// We already know main worker
  for(int i=1; i<n_workers; ++i){
    void *worker = zmq_socket(context, ZMQ_REP);
    workers[i] = worker;
    char addr[64];
    snprintf(addr, 64, "tcp://%s:%d", argv[1], port++);
    zmq_bind(workers[i], addr);
  }

  /// Handshake with other workers
  uint64_t seq=0;
  for(int i=1; i<n_workers; ++i){
   msg = tracemq_recv_msg(workers[i]); assert(seq==msg->seq_n);
   tomo_msg_data_info_req_t* info = tracemq_read_data_info_req(msg);
   worker_ids[i]=info->comm_rank;
   tracemq_free_msg(msg);
  }
  ++seq;

  /// Distribute data info
  for(int i=0; i<n_workers; ++i){
   tomo_msg_data_info_rep_t info = 
      assign_data(worker_ids[i], n_workers, dims[1], dims[2]);
   tomo_msg_t *msg = tracemq_prepare_data_info_rep_msg(seq, 
     info.beg_sinogram, info.n_sinograms, 
     info.n_rays_per_proj_row, info.tn_sinograms);
   tracemq_send_msg(workers[i], msg);
   tracemq_free_msg(msg);
  }
  ++seq;

  /// recieve ready message
  for(int i=0; i<n_workers; ++i){
   msg = tracemq_recv_msg(workers[i]);
   assert(msg->type==TRACEMQ_MSG_DATA_REQ);
   assert(seq==msg->seq_n);
   tracemq_free_msg(msg);
  }
  ++seq;


  /// Start distributing data to workers
  for(int i=0; i<dims[0]; ++i) {  // For each incoming projection
    float *curr_projection = data + i*dims[1]*dims[2];
    int projection_dims[2] = { dims[1], dims[2] };
    float proj_theta = theta[i];
    tomo_msg_t **worker_msgs = generate_tracemq_worker_msgs(
      curr_projection, projection_dims, 
      i, proj_theta, n_workers, seq);

    /// Send partitioned projection data to workers 
    for(int j=0; j<n_workers; ++j){
      /// Prepare zmq message
      tomo_msg_t *curr_msg = worker_msgs[j];
                        
      zmq_msg_t msg;
      int rc = zmq_msg_init_size(&msg, curr_msg->size); assert(rc==0);
      memcpy((void*)zmq_msg_data(&msg), (void*)curr_msg, curr_msg->size);
      rc = zmq_msg_send(&msg, workers[j], 0); assert(rc==(int)curr_msg->size);
    }
    ++seq;
    
    /// Check if workers received their corresponding data chunks
    for(int j=0; j<n_workers; ++j){
      msg = tracemq_recv_msg(workers[j]);
      assert(msg->type==TRACEMQ_MSG_DATA_REQ);
      assert(msg->seq_n==seq);
      tracemq_free_msg(msg);
    }
    ++seq;

    /// Clean-up data chunks
    for(int j=0; j<n_workers; ++j)
      free(worker_msgs[j]);
    free(worker_msgs);
  }

  /// All the projections are finished
  for(int i=0; i<n_workers; ++i){
    zmq_msg_t msg;
    tomo_msg_t msg_fin = { .seq_n=seq, .type = TRACEMQ_MSG_FIN_REP, .size=sizeof(tomo_msg_t)};
    int rc = zmq_msg_init_size(&msg, sizeof(msg_fin)); assert(rc==0);
    memcpy((void*)zmq_msg_data(&msg), (void*)&msg_fin, sizeof(msg_fin));
    rc = zmq_msg_send(&msg, workers[i], 0); assert(rc==sizeof(msg_fin));
  }
  ++seq;
  
  
  // Receive worker fin reply
  for(int j=0; j<n_workers; ++j){
    msg = tracemq_recv_msg(workers[j]);
    assert(msg->type==TRACEMQ_MSG_FIN_REP);
    assert(msg->seq_n==seq);
    tracemq_free_msg(msg);
  }
  ++seq;
  
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
