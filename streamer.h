#ifndef _STREAMER_H
#define _STREAMER_H

struct _tomo_msg_str {
  float theta; // theta value of this projection
  int beg_sinogram; // beginning sinogram id
  int n_sinogram; // # Sinograms
  // int nprojs=1; // Always projection by projection
  int n_rays_per_proj_col; // # rays per projection column
  int16_t *data;  // real projection data
                  // number of rays in data=n_sinogram*n_rays_per_proj_col
};

typedef struct tomo_msg_str tomo_msg_t;

#endif  // _STREAMER_H
