#ifndef _STREAMER_H
#define _STREAMER_H

#define TRACE_DATA 0x00000001
#define TRACE_FIN  0x00000000

struct _tomo_msg_str {
  uint32_t type;
  int projection_id;
  float theta; // theta value of this projection
  int beg_sinogram; // beginning sinogram id
  int n_sinogram; // # Sinograms
  // int nprojs=1; // Always projection by projection
  int n_rays_per_proj_col; // # rays per projection column
  int16_t data[];  // real projection data
                  // number of rays in data=n_sinogram*n_rays_per_proj_col
};

typedef struct _tomo_msg_str tomo_msg_t;

#endif  // _STREAMER_H
