//#include "zhelpers.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "mock_data_acq.h"
#include "trace_streamer.h"
#include "zmq.h"

int main (int argc, char *argv[])
{
  if(argc!=7) {
    printf("Usage: %s <ip-to-bind=164.54.143.3> "
                      "<starting-port=5560> "
                      "<data-file=data> "
                      "<interval-sec=0> "
                      "<interval-nanosec=500000000> "
                      "<subset=0> \n", argv[0]);
    exit(0);
  }

  /// Parse arguments
  int port = atoi(argv[2]);
  char *fp = argv[3];
  int interval_sec = atoi(argv[4]);
  long long interval_nanosec = atoll(argv[5]);
  int nsubsets = atoi(argv[6]);

  /// Setup zmq
  void *context = zmq_ctx_new();

  /// Setup mock data acquisition
  mock_interval_t interval = { interval_sec, interval_nanosec };
  dacq_file_t *dacq_file = mock_dacq_file(fp, interval);
  if(nsubsets>0) {
    dacq_file_t *ndacq_file = mock_dacq_interleaved(dacq_file, nsubsets);
    mock_dacq_file_delete(dacq_file);
    dacq_file = ndacq_file;
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
   tomo_msg_data_info_rep_t info = assign_data(worker_ids[i], n_workers, 
                                      dacq_file->dims[1], dacq_file->dims[2]);
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

  /// Simulate projection generation
  ts_proj_data_t *proj=NULL;
  while((proj=mock_dacq_read(dacq_file)) != NULL){
    printf("Sending proj: %d; id=%d\n", dacq_file->curr_proj_index, proj->id);
    float center = proj->dims[1]/2.;  /// Default center is middle of columns
    tomo_msg_t **worker_msgs = generate_tracemq_worker_msgs(
                                  proj->data, proj->dims, proj->id, 
                                  proj->theta, n_workers, center, seq);
    free(proj); proj=NULL;

    /// Send data to workers
    for(int i=0; i<n_workers; ++i){
      /// Prepare zmq message
      tomo_msg_t *curr_msg = worker_msgs[i];
                        
      zmq_msg_t msg;
      int rc = zmq_msg_init_size(&msg, curr_msg->size); assert(rc==0);
      memcpy((void*)zmq_msg_data(&msg), (void*)curr_msg, curr_msg->size);
      rc = zmq_msg_send(&msg, workers[i], 0); assert(rc==(int)curr_msg->size);
    }
    ++seq;

    /// Check if workers received their corresponding data chunks
    for(int i=0; i<n_workers; ++i){
      msg = tracemq_recv_msg(workers[i]);
      assert(msg->type==TRACEMQ_MSG_DATA_REQ);
      assert(msg->seq_n==seq);
      tracemq_free_msg(msg);
    }
    ++seq;

    /// Clean-up data chunks
    for(int i=0; i<n_workers; ++i)
      free(worker_msgs[i]);
    free(worker_msgs);
  }

  /// Done with the data, clean-up resources
  mock_dacq_file_delete(dacq_file);

  /// All the projections are finished
  for(int i=0; i<n_workers; ++i){
    zmq_msg_t msg;
    tomo_msg_t msg_fin = {.seq_n=seq, .type = TRACEMQ_MSG_FIN_REP, 
                          .size=sizeof(tomo_msg_t) };
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

  return 0;
}
