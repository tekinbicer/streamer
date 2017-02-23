#ifndef _STREAMER_H
#define _STREAMER_H

#define TRACE_DATA 0x00000001
#define TRACE_FIN  0x00000000

/* Center, tn_sinogram, n_rays_per_proj_row are all global and can be sent only once.
 *  */
struct _tomo_msg_str {
	uint32_t type;
	int projection_id;        // projection id
	float theta;              // theta value of this projection
	int beg_sinogram;         // beginning sinogram id
	int tn_sinogram;          // Total # Sinograms
	int n_sinogram;           // # Sinograms
	// int nprojs=1;          // Always projection by projection
	int n_rays_per_proj_row;  // # rays per projection row 
	int center;               // center of the projecion
	float data[];             // real projection data
	// number of rays in data=n_sinogram*n_rays_per_proj_row
};
typedef struct _tomo_msg_str tomo_msg_t;

#endif  // _STREAMER_H
