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

struct tomo_msg_data_info_rep_str {
  uint32_t tn_sinograms; 
	uint32_t beg_sinogram;
  uint32_t n_sinograms;
  uint32_t n_rays_per_proj_row;
};

struct tomo_msg_data_info_req_str {
  uint32_t comm_rank; 
  uint32_t comm_size;
};

/* Center, tn_sinogram, n_rays_per_proj_row are all global and can be sent only once.
 */
struct _tomo_msg_data_str {
	int projection_id;        // projection id
	float theta;              // theta value of this projection
	int center;               // center of the projecion
	float data[];             // real projection data
	// number of rays in data=n_sinogram*n_rays_per_proj_row (n_sinogram*n_rays_per_proj_row were given in req msg.)
};

typedef struct _tomo_msg_h_str tomo_msg_t;
typedef struct _tomo_msg_data_str tomo_msg_data_t;
typedef struct _tomo_msg_data_info_req_str tomo_msg_data_info_req_t;
typedef struct _tomo_msg_data_info_rep_str tomo_msg_data_info_rep_t;


tomo_msg_t* tracemq_prepare_data_req_msg(uint64_t seq_n)
{
  size_t tot_msg_size = sizeof(tomo_msg_t);
  tomo_msg_t *msg = malloc(tot_msg_size);
  tracemq_setup_msg_header(msg_h, seq_n, TRACEMQ_MSG_DATA_REQ, tot_msg_size);

  return msg;
}

tomo_msg_t* tracemq_prepare_data_rep_msg(uint64_t seq_n,
                                         int projection_id,        
                                         float theta,              
                                         int center,
                                         float *data)
{
  size_t data_size = sizeof(*data)*(n_sinogram)*n_rays_per_proj_row; /// In bytes
  size_t tot_msg_size = sizeof(tomo_msg_t)+sizeof(tomo_msg_data_t)+data_size;
  tomo_msg_t *msg_h = malloc(tot_msg_size);
  tomo_msg_data_t *msg = (tomo_msg_data_t *) msg_h->data;
  tracemq_setup_msg_header(msg_h, seq_n, TRACEMQ_MSG_DATA_REP, tot_msg_size);

  msg->projection_id = projection_id;
  msg->theta = theta;
  msg->center = center
  memcpy(msg->data, data, data_size);

  return msg_h;
}
tomo_msg_data_t* tracemq_read_data(tomo_msg_t *msg){
  return (tomo_msg_data_info_t *) msg->data;
}

tomo_msg_t* tracemq_prepare_data_info_rep_msg(uint64_t seq_n, 
                                              int beg_sinogram, int n_sinogram,
                                              int n_rays_per_proj_row,
                                              uint64_t tn_sinograms)
{
  uint64_t tot_msg_size = sizeof(tomo_msg_t)+sizeof(tomo_msg_data_info_rep_t);
  tomo_msg_t *msg = malloc(tot_msg_size);
  tracemq_setup_msg_header(msg, seq_n, tot_msg_size, TRACEMQ_MSG_DATAINFO_REP);

  tomo_msg_data_rep_info_t *info = (tomo_msg_data_info_rep_t *) msg->data;
  info->tn_sinograms = tn_sinograms;
  info->beg_sinogram = beg_sinogram;
  info->n_sinogram = n_sinogram;
  info->n_rays_per_proj_row = n_rays_per_proj_row;

  return msg;
}
tomo_msg_data_info_rep_t* tracemq_read_data_info_rep(tomo_msg_t *msg){
  return (tomo_msg_data_info_rep_t *) msg->data;
}

tomo_msg_t* tracemq_prepare_data_info_req_msg(uint64_t seq_n, uint32_t comm_rank, uint32_t comm_size){
  uint64_t tot_msg_size = sizeof(tomo_msg_t)+sizeof(tomo_msg_data_info_req_t);
  tomo_msg_t *msg = malloc(tot_msg_size);
  tracemq_setup_msg_header(msg, seq_n, tot_msg_size, TRACEMQ_MSG_DATAINFO_REQ);

  tomo_msg_data_req_info_t *info = (tomo_msg_data_info_req_t *) msg->data;
  info->comm_rank = comm_rank;
  info->comm_size = comm_size;

  return msg;
}
tomo_msg_data_info_req_t* tracemq_read_data_info_rep(tomo_msg_t *msg){
  return (tomo_msg_data_info_req_t *) msg->data;
}

tomo_msg_t* tracemq_prepare_fin_msg(uint64_t seq_n){
  uint64_t tot_msg_size = sizeof(tomo_msg_t);
  tomo_msg_t *msg = malloc(tot_msg_size);
  tracemq_setup_msg_header(msg, seq_n, tot_msg_size, TRACEMQ_MSG_FIN_REP);

  return msg;
}


void tracemq_free_msg(tomo_msg_t *msg) {
  free(msg);
}

void tracemq_setup_msg_header(
  tomo_msg_t *msg_h, 
  uint64_t seq_n, uint64_t type, uint_64_t size)
{
  msg->seq_n = seq_n;
  msg->type = type;
  msg->size = tot_msg_size;
}



#endif  // _STREAMER_H
