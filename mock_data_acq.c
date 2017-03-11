#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mock_data_acq.h"

dacq_file_t* mock_dacq_file(char *fp, 
                            mock_interval_t interval)
{
  int ndims=3;

  FILE *f;
  f = fopen(fp, "rb");
  if(!f){
    printf("Unable to open file!\n");
    return NULL;
  }

  /// Read dims[3]: first three integers
  int *dims = (int*) malloc(ndims*sizeof(*dims));
  int rc = fread(dims, sizeof(*dims), ndims, f);
  if(rc!=ndims){
    printf("Unable to read dimensions!\n");
    return NULL;
  }

  for(int i=0; i<ndims; ++i) printf("dim[%d]=%d\n", i, dims[i]);

  int pcount = 1;
  for(int i=0; i<ndims; ++i) pcount *= dims[i];
  float *pdata = (float*) malloc(pcount*sizeof(*pdata));
  rc = fread(pdata, sizeof(*pdata), pcount, f);
  if(rc!=pcount){
    printf("Unable to read projection data from file!\n");
    return NULL;
  }

  /// Read theta values
  int tcount = dims[0]; 
  float *tdata = (float*) malloc(tcount*sizeof(*tdata));
  rc = fread(tdata, sizeof(*tdata), tcount, f);
  if(rc!=tcount){
    printf("Unable to read theta data from file!\n");
    return NULL;
  }
  /// This should be the end of file, check this later

  /// Set projection ids, these change when interleaved data acquisition
  /// is simulated.
  int *proj_ids = (int *) malloc(sizeof(*proj_ids)*dims[0]);
  for(int i=0; i<dims[0]; ++i) proj_ids[i]=i;

  /// Prepare data structure to be returned
  dacq_file_t *dacq = (dacq_file_t*) malloc(sizeof(*dacq));
  dacq->projs = pdata;
  dacq->thetas = tdata;
  dacq->dims = dims;
  dacq->interval = interval;
  dacq->curr_proj_index = 0;
  dacq->proj_ids = proj_ids;

  fclose(f);
  return dacq;
}

ts_proj_data_t* mock_dacq_read(dacq_file_t *df)
{
  /// Check if this is the end of data acquisition
  if(df->curr_proj_index==df->dims[0]) return NULL;

  /// Sleep as much as time interval
  nanosleep(&(df->interval), NULL);  // No interrupt is expected

  ts_proj_data_t *proj = (ts_proj_data_t*) malloc(sizeof(*proj));
  proj->dims = &(df->dims[1]);
  proj->data = df->projs + 
                df->curr_proj_index * proj->dims[0] * proj->dims[1]; 
  proj->theta = df->thetas[df->curr_proj_index];
  proj->id = df->proj_ids[df->curr_proj_index++];

  return proj;
}

void mock_dacq_file_delete(dacq_file_t *df)
{
  free(df->projs);
  free(df->thetas);
  free(df->dims);
  free(df->proj_ids);
  free(df);
}

/// Copies n data pointed by src to dest according to index ortanization ix 
void mock_gen_interleaved(int *ix, int n, size_t nitem, 
                          float *src, float *dest)
{
  for(int i=0; i<n; ++i){
    float *curr_src = src + ix[i]*nitem;
    float *curr_dest = dest + i*nitem;
    memcpy((void*)(curr_dest), (void*)curr_src, nitem*sizeof(float));
  }
}

dacq_file_t* mock_dacq_interleaved( dacq_file_t *dacqd, 
                                    int nsubsets)
{
  dacq_file_t *new_dacqd = (dacq_file_t*) malloc(sizeof(dacq_file_t));
  int *dims = dacqd->dims;

  /// Copy dimensions
  new_dacqd->dims = (int*) malloc(sizeof(int)*3);
  memcpy((void*)new_dacqd->dims, (void*)dims, 3*sizeof(*dims));

  /// Calculate new indices
  int num_projs = dims[0];
  size_t rcp = dims[1]*dims[2];
  int rem = num_projs%nsubsets;

  int *proj_ix = (int*) malloc(sizeof(int)*num_projs);
  for(int i=0, index=0; i<nsubsets; ++i){
    int count = (num_projs/nsubsets)+(i<rem);
    for(int j=0; j<count; ++j){
      proj_ix[index++]=(j*nsubsets)+i;
    }
  }

  /// Copy data
  new_dacqd->projs = (float*) malloc(sizeof(float)*num_projs*rcp);
  mock_gen_interleaved(proj_ix, num_projs, rcp, dacqd->projs, new_dacqd->projs);
  /// Copy theta
  new_dacqd->thetas = (float*) malloc(sizeof(float)*num_projs);
  mock_gen_interleaved(proj_ix, num_projs, 1, dacqd->thetas, new_dacqd->thetas);

  new_dacqd->curr_proj_index = 0;
  new_dacqd->interval = dacqd->interval;
  new_dacqd->proj_ids = proj_ix;

  return new_dacqd;
}
