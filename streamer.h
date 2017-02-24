#ifndef _STREAMER_H
#define _STREAMER_H

#define TRACEMQ_MSG_FIN_REP       0x00000000
#define TRACEMQ_MSG_DATAINFO_REQ  0x00000001
#define TRACEMQ_MSG_DATAINFO_REP  0x00000002

#define TRACEMQ_MSG_DATA_REQ      0x00000010
#define TRACEMQ_MSG_DATA_REP      0x00000020


struct _tomo_msg_h_str {
  uint64_t seq_n;
  uint64_t type;
  uint64_t size;
  char data[];
};

struct _tomo_msg_data_info_rep_str {
  uint32_t tn_sinograms; 
	uint32_t beg_sinogram;
  uint32_t n_sinograms;
  uint32_t n_rays_per_proj_row;
};

struct _tomo_msg_data_info_req_str {
  uint32_t comm_rank; 
  uint32_t comm_size;
};

/* Center, tn_sinogram, n_rays_per_proj_row are all global and can be sent only once.
 */
struct _tomo_msg_data_str {
	int projection_id;        // projection id
	float theta;              // theta value of this projection
	float center;               // center of the projecion
	float data[];             // real projection data
	// number of rays in data=n_sinogram*n_rays_per_proj_row (n_sinogram*n_rays_per_proj_row were given in req msg.)
};

typedef struct _tomo_msg_h_str tomo_msg_t;
typedef struct _tomo_msg_data_str tomo_msg_data_t;
typedef struct _tomo_msg_data_info_req_str tomo_msg_data_info_req_t;
typedef struct _tomo_msg_data_info_rep_str tomo_msg_data_info_rep_t;

void tracemq_free_msg(tomo_msg_t *msg) {
  free(msg);
  msg=NULL;
}

void tracemq_setup_msg_header(
  tomo_msg_t *msg_h, 
  uint64_t seq_n, uint64_t type, uint64_t size)
{
  msg_h->seq_n = seq_n;
  msg_h->type = type;
  msg_h->size = size;
}

tomo_msg_t* tracemq_prepare_data_req_msg(uint64_t seq_n)
{
  size_t tot_msg_size = sizeof(tomo_msg_t);
  tomo_msg_t *msg = malloc(tot_msg_size);
  tracemq_setup_msg_header(msg, seq_n, TRACEMQ_MSG_DATA_REQ, tot_msg_size);

  return msg;
}

tomo_msg_t* tracemq_prepare_data_rep_msg(uint64_t seq_n,
                                         int projection_id,        
                                         float theta,              
                                         float center,
                                         uint64_t data_size,
                                         float *data)
{
  uint64_t tot_msg_size=sizeof(tomo_msg_t)+sizeof(tomo_msg_data_t)+data_size;
  tomo_msg_t *msg_h = malloc(tot_msg_size);
  tomo_msg_data_t *msg = (tomo_msg_data_t *) msg_h->data;
  tracemq_setup_msg_header(msg_h, seq_n, TRACEMQ_MSG_DATA_REP, tot_msg_size);

  msg->projection_id = projection_id;
  msg->theta = theta;
  msg->center = center;
  memcpy(msg->data, data, data_size);

  return msg_h;
}

tomo_msg_data_t* tracemq_read_data(tomo_msg_t *msg){
  return (tomo_msg_data_t *) msg->data;
}

void tracemq_print_data(tomo_msg_data_t *msg, size_t data_count){
  printf("projection_id=%u; theta=%f; center=%f\n", 
    msg->projection_id, msg->theta, msg->center);
  for(size_t i=0; i<data_count; ++i)
    printf("%f", msg->data[i]);
}



tomo_msg_t* tracemq_prepare_data_info_rep_msg(uint64_t seq_n, 
                                              int beg_sinogram, int n_sinograms,
                                              int n_rays_per_proj_row,
                                              uint64_t tn_sinograms)
{
  uint64_t tot_msg_size = sizeof(tomo_msg_t)+sizeof(tomo_msg_data_info_rep_t);
  tomo_msg_t *msg = malloc(tot_msg_size);
  tracemq_setup_msg_header(msg, seq_n, TRACEMQ_MSG_DATAINFO_REP, tot_msg_size);

  tomo_msg_data_info_rep_t *info = (tomo_msg_data_info_rep_t *) msg->data;
  info->tn_sinograms = tn_sinograms;
  info->beg_sinogram = beg_sinogram;
  info->n_sinograms = n_sinograms;
  info->n_rays_per_proj_row = n_rays_per_proj_row;

  return msg;
}
tomo_msg_data_info_rep_t* tracemq_read_data_info_rep(tomo_msg_t *msg){
  return (tomo_msg_data_info_rep_t *) msg->data;
}
void tracemq_print_data_info_rep_msg(tomo_msg_data_info_rep_t *msg){
  printf("Total # sinograms=%u; Beginning sinogram id=%u;"
          "# assigned sinograms=%u; # rays per projection row=%u\n", 
          msg->tn_sinograms, msg->beg_sinogram, msg->n_sinograms, msg->n_rays_per_proj_row);
}

tomo_msg_t* tracemq_prepare_data_info_req_msg(uint64_t seq_n, uint32_t comm_rank, uint32_t comm_size){
  uint64_t tot_msg_size = sizeof(tomo_msg_t)+sizeof(tomo_msg_data_info_req_t);
  printf("total message size=%zu\n", tot_msg_size);
  tomo_msg_t *msg = malloc(tot_msg_size);
  tracemq_setup_msg_header(msg, seq_n, TRACEMQ_MSG_DATAINFO_REQ, tot_msg_size);

  tomo_msg_data_info_req_t *info = (tomo_msg_data_info_req_t *) msg->data;
  info->comm_rank = comm_rank;
  info->comm_size = comm_size;

  return msg;
}
tomo_msg_data_info_req_t* tracemq_read_data_info_req(tomo_msg_t *msg){
  return (tomo_msg_data_info_req_t *) msg->data;
}

tomo_msg_t* tracemq_prepare_fin_msg(uint64_t seq_n){
  uint64_t tot_msg_size = sizeof(tomo_msg_t);
  tomo_msg_t *msg = malloc(tot_msg_size);
  tracemq_setup_msg_header(msg, seq_n, TRACEMQ_MSG_FIN_REP, tot_msg_size);

  return msg;
}

void tracemq_send_msg(void *server, tomo_msg_t* msg){
  zmq_msg_t zmsg;
  int rc = zmq_msg_init_size(&zmsg, msg->size); assert(rc==0);
  memcpy((void*)zmq_msg_data(&zmsg), (void*)msg, msg->size);
  rc = zmq_msg_send(&zmsg, server, 0); assert(rc==(int)msg->size);
}

tomo_msg_t* tracemq_recv_msg(void *server){
  zmq_msg_t zmsg;
  int rc = zmq_msg_init(&zmsg); assert(rc==0);
  rc = zmq_msg_recv(&zmsg, server, 0); assert(rc!=-1);
  /// Message size and calculated total message size needst to be the same
  /// FIXME?: We put tomo_msg_t.size to calculate zmq message size before it is
  /// being sent. It is being only being used for sanity check at the receiver
  /// side. 
  //printf("zmq_msg_size(&zmsg)=%zu; ((tomo_msg_t*)&zmsg)->size=%zu", zmq_msg_size(&zmsg), ((tomo_msg_t*)&zmsg)->size);
  //assert(zmq_msg_size(&zmsg)==((tomo_msg_t*)&zmsg)->size);

  tomo_msg_t *msg = malloc(((tomo_msg_t*)&zmsg)->size);
  /// Zero-copy would have been better
  memcpy(msg, zmq_msg_data(&zmsg), zmq_msg_size(&zmsg));
  zmq_msg_close(&zmsg);

  return msg;
}



#endif  // _STREAMER_H
