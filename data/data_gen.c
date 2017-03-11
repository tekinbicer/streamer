#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


#define PI 3.1415926535897932384626433832795028841971693993751058209749445923078164062

int main(int argc, char **argv)
{
  if(argc!=7){
    printf( "Usage: %s <file-name> "
            "<dim0> <dim1> <dim2> "
            "<theta-end=180> "
            "<init-val=0.0001> \n", argv[0]);
    exit(0);
  }

  /// Parse values
  char *file_name = argv[1];
  int dims[3] = {atoi(argv[2]), atoi(argv[3]), atoi(argv[4])};
  float theta_end = atof(argv[5]);
  float init_val = atof(argv[6]);

  int nrays = 1;
  for(int i=0; i<3; ++i) nrays *= dims[i];
  float *proj_data = (float*) malloc(sizeof(float)*nrays);
  for(int i=0; i<nrays; ++i) proj_data[i]=init_val;

  float *theta = (float*) malloc(sizeof(float)*dims[0]);
  float radian_end = (theta_end/180)*PI;
  float radian_step = radian_end/dims[0];
  for(int i=0; i<dims[0]; ++i) theta[i] = i*radian_step;

  /// Write to file
  FILE *fp = fopen(file_name, "wb");
  size_t rc = fwrite(dims, sizeof(int), 3, fp); assert(rc==3);
  rc = fwrite(proj_data, sizeof(float), nrays, fp); assert(rc==nrays);
  rc = fwrite(theta, sizeof(float), dims[0], fp); assert(rc==dims[0]);

  fclose(fp);
  free(proj_data);
  free(theta);
}
