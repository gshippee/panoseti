#ifndef __REMOVERFI_H__
#define __REMOVERFI_H__

#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#ifndef _RAND_NUM_GEN_
//#include "rand_num_generator.h"
#endif /* _RAND_NUM_GEN_ */

#ifndef __CRABDATABUF_H__
//#include "crab_databuf.h"
#endif /* __CRABDATABUF_H__*/

#ifndef __FILTERBANK_H__
//#include "filterbank.h"
#endif /* __FILTERBANK_H__ */

#ifndef _ACQ_UDP_INCLUDED_
#include "acq_udp.h"
#endif /* _ACQ_UDP_INCLUDED_ */

#define STRTCHAN      280       // 1800 if spectrum contains 4096 channels, 
                                // 300 (240) if 1536 channels
#define STPCHAN      1200	// 2730 if spectrum contains 4096 channels, 
                                // 1230 (1264) if 1536 channes 2270-2730
#define NCHAN        1536	// 1536 or 4096

typedef struct{
    short int *data;
    long num_samples;
    bool replace_data;
    bool stats_done;
    double *input;
    double *avg;
    double *sd;
    double *window;
    double thresh_limit;
    double curravg;
    double currsd;
    int counter;
    int window_size; 
    int channel_width;
    bool record_stats;
} DataStruct;

/* Because I need to be smarter */
float gasdev(long *idum);
float ran1(long *idum);

/* Pseudo main for filterbank and raw data files */
void process_filterbank_file(char infile[], char outfile[], int pol, 
                             int bad_channel_flag, int narrowband_rfi_flag, 
                             int broadband_rfi_flag, int zero_nonbandpass_flag);
void process_raw_data_files(char dirname[], char outfile[], int pol, 
                            int bad_channel_flag, int narrowband_rfi_flag,
                            int broadband_rfi_flag, int zero_nonbandpass_flag);

/* Data and Stat handling */
DataStruct *create_DataStruct(long num_samples);
void free_DataStruct(DataStruct *dsp);

void init_struct1(DataStruct *dsp, int channel_width, int window_size, 
                 double thresh_limit, int replace_data, int record_stats);

void update_avgsd(DataStruct *dsp, int start_channel, int end_channel, 
                  long long current_sample);

/* Helper functions for raw data handling */
int num_files_in_dir(char dirname[]);
void get_files_in_dir(int num_files, char dirname[], char **files);
void read_data(DataStruct *dsp, long long data_size, int pol, FILE *fp);


/* RFI removal functions */
void remove_rfi(DataStruct *dsp);
void remove_broadband_rfi(DataStruct *dsp);
void remove_narrowband_rfi(DataStruct *dsp);
void zero_non_bandpass(DataStruct *dsp);

void calc_first_window_stats(DataStruct *dsp);
void compute_timesum(DataStruct *dsp, int start_channel, int end_channel);
void process_first_window(DataStruct *dsp, int start_channel, int end_channel);
void process_remainder(DataStruct *dsp, int start_channel, int end_channel);
void adjust_data(DataStruct *dsp, int start_channel, int end_channel, long long current_sample);

/* Access to struct variable for Python wrapping */
short int get_data(DataStruct *dsp, long loc);
double get_input(DataStruct *dsp, long loc);
short int get_output(DataStruct *dsp, long loc);
long get_num_samples(DataStruct *dsp);
double get_thresh_limit(DataStruct *dsp);
int get_counter(DataStruct *dsp);
int get_chan_width(DataStruct *dsp);
void set_channel_width(DataStruct *dsp, int chan_width);
void set_data_array(DataStruct *dsp, long loc, short int data);
double get_avg(DataStruct *dsp, long loc);
double get_sd(DataStruct *dsp, long loc);
void set_input(DataStruct *dsp, long loc, double data);
void set_curravg(DataStruct *dsp, double avg);
void set_currsd(DataStruct *dsp, double sd);

# endif /* __REMOVERFI_H__ */
