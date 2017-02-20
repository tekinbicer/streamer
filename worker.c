//   Request worker in C

#include "zhelpers.h"
#include "mpi.h"
#include "streamer.h"

tomo_msg_t* trace_receive_msg(void *server){
    zmq_msg_t zmsg;
    int rc = zmq_msg_init(&zmsg); assert(rc==0);
    rc = zmq_msg_recv(&zmsg, server, 0); assert(rc!=-1);

    tomo_msg_t *msg = malloc(sizeof(*msg));
    assert(zmq_msg_size(&zmsg)==sizeof(*msg));
    // Zero-copy would have been better
    memcpy(msg, zmq_msg_data(&zmsg), sizeof(*msg));
    zmq_msg_close(&zmsg);

    return msg;
}

int main (int argc, char *argv[])
{
  if(argc!=3) {
    printf("Usage: %s <dest-ip-address=164.54.143.3> <dest-port=5560>\n", argv[0]);
    exit(0);
  }

  // Setup MPI
  int my_rank, world_size;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  int dest_port = atoi(argv[2]);
  char addr[64];
  snprintf(addr, 64, "tcp://%s:%d", argv[1], dest_port);
  printf("Destination address=%s\n", addr);

  // Connect to server 
  void *context = zmq_ctx_new();
  void *server = zmq_socket(context, ZMQ_REQ);
  zmq_connect(server, addr);

  // Introduce yourself
  char rank_str[16];
  snprintf(rank_str, 16, "%d", my_rank);
  s_send(server, rank_str);

  // Check if server has any projection
  tomo_msg_t *msg;
  while((msg=trace_receive_msg(server))->type){
    // Say you received it
    char ack_msg[256];
    snprintf(ack_msg, 256, "[%d]: I received the projections", my_rank);
    s_send(server, ack_msg);

    // Do something with the data
    for(int i=0; i<msg->n_sinogram; ++i){
      for(int j=0; j<msg->n_rays_per_proj_col; ++j){
        printf("%hi ", msg->data[i*msg->n_rays_per_proj_col+j]);
      }
      printf("\n");
    }

    // Clean-up message
    free(msg->data);
    free(msg);
  }

  // Tell you received fin message
  char ack_msg[256];
  snprintf(ack_msg, 256, "[%d]: I received fin message: %08x", my_rank, msg->type);
  s_send(server, ack_msg);
  free(msg);

  // Cleanup
  zmq_close(server);
  zmq_ctx_destroy(context);
  
  MPI_Finalize();
}

