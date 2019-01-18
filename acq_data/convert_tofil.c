#ifndef __REMOVE_RFI_H__
#include "converto_fil.h"
#endif

long seed =5;

int num_files_in_dir(char dirname[]){
   DIR *dirp; 
   struct dirent *dirpp;
   int num_files = 0;
   int ctr=0;

   /*Open file directory and count number of data files in directory*/
   dirp = opendir(dirname);
   fprintf(stderr,"Checking directory %s\n",dirname);
   if (dirp!= NULL){
      while ((dirpp = readdir(dirp))!=NULL){
        int length = strlen(dirpp->d_name);
        if (strncmp(dirpp->d_name, "data", 4) == 0 && 
            strncmp(dirpp->d_name+length-4,".fil", 4) != 0) num_files++;
      }
   }
   printf("Number of files in directory:  %d\n",num_files);
   return num_files;
}

void get_files_in_dir(int num_files, char dirname[], char **files){
   DIR *dirp; 
   struct dirent *dirpp;
   int ctr=0;
   struct stat info[num_files];
   fprintf(stderr,"Opening directory %s\n",dirname);
   dirp = opendir(dirname);

   /*Create an array of the filenames for future access*/
   while((dirpp = readdir(dirp))!=NULL){
     int length = strlen(dirpp->d_name);
     if (strncmp(dirpp->d_name, "data", 4) == 0 && 
         strncmp(dirpp->d_name+length-4, ".fil", 4)!= 0 && 
         strncmp(dirpp->d_name+length-4, ".txt", 4)!= 0){
             strcpy(files[ctr], dirname);
             strcat(files[ctr], dirpp->d_name); 
             //fprintf(stderr,"file: %s\n",files[ctr]);
             ctr++;
             //stat(dirpp->d_name, &info[ctr]);
     }
   }
   fprintf(stderr,"Got files. Rearranging now\n");
   (void)closedir(dirp);

    /*Reorder the files in the array by time (DIR randomly 
      accesses the files in the directory so the original 
      array of filenames is out of order */
   int i,j; char temp[1024];
   for(i=0;i< num_files;i++)
      for(j=i+1;j< num_files;j++){
         //if(info[i].st_mtime > info[j].st_mtime){
         if(strcmp(files[i],files[j])>0){
           //fprintf(stderr,"swapping %s and %s\n",files[i],files[j]);
           strcpy(temp,files[i]);
           strcpy(files[i],files[j]);
	       strcpy(files[j],temp);
	 }
   } 

   for(int i=0;i<num_files;i++)
      printf("%s\n",files[i]);
}


void output_filename(char *outfile, int fil_flag, int pol, 
                     int bad_channel_flag, int narrowband_rfi_flag, 
                     int broadband_rfi_flag, int zero_nonbandpass_flag){
   if(!pol) strcpy(outfile,"power_");
   if(pol==1) strcpy(outfile,"xx_");
   if(pol==2) strcpy(outfile,"yy_");

   if(bad_channel_flag) strcat(outfile,"bad_channels_");
   if(narrowband_rfi_flag) strcat(outfile,"narrow_");
   if(broadband_rfi_flag) strcat(outfile,"broad_");

   strcat(outfile,"cleaned.fil");
   printf("Output file: %s\n",outfile);
}

void zero_non_bandpass(DataStruct *dsp){
    int j,k;

    printf("Setting values outside bandpass to zero...\n");
    for(j = 0; j < dsp->num_samples; j++)
       for(k = 0; k< NCHAN; k++)
          if(k < STRTCHAN || k >= STPCHAN)
             dsp->data[NCHAN*j+k] = 0;
}

void sort(double *a,int *idx, int N){
   /* Sort array 'a' with double type 
   floating point numbers, and of length N */
   int tidx;
   double temp;

   for(int i=0; i<N; i++){
      for(int j=i+1; j<N; j++){
         if (a[i]>a[j]){
            tidx = idx[i]; idx[i] = idx[j]; idx[j] = tidx;
            temp = a[i]; a[i] = a[j]; a[j] = temp;
         }
      }
   }
}

void remove_bad_channel_2(DataStruct *dsp){
   /* Removes RFI that is constant in time */
   int i,j;
   int nchan = STPCHAN - STRTCHAN ; //920 
   double mean_spec[nchan];
   for (int j=0; j<nchan; j++) mean_spec[j] = 0;
   
   /* Find mean spectrum over all time */
   fprintf(stderr,"Computing mean spectrum. nchan: %d\n",nchan);
   for(j=0; j<nchan; j++){
       for (int i=0; i< dsp->num_samples; i++)
           mean_spec[j] += dsp->data[i*NCHAN + STRTCHAN+j];
       mean_spec[j] = mean_spec[j]/dsp->num_samples;
   }
   fprintf(stderr,"Mean of all spectra\n");
   for (j=0; j<nchan; j++) printf("%.2f\t",mean_spec[j]);
   printf("\n\n");
   
   /* Find median of array */
   fprintf(stderr,"Computing median of the spectrum\n");
   int *idx = (int *)malloc(nchan*sizeof(int));
   for (int i=0; i<nchan; i++) idx[i] = i;
   sort(mean_spec,idx,nchan);
   double median = (mean_spec[nchan/2] + mean_spec[nchan/2-1])/2;
   fprintf(stderr,"Median: %f\n",median);

   /* Compute standard deviation to set threshold limits*/
   fprintf(stderr,"Computing standard deviation of the spectrum\n");
   double mean = 0, var = 0, random;
   for (i=0; i<nchan; i++) mean += mean_spec[i]/nchan;
   fprintf(stderr,"Mean: %f\n",mean);
   for (i=0; i<nchan; i++) var += pow((mean_spec[i] - mean),2);
   double std = sqrt(var/nchan-1);
   fprintf(stderr,"Standard deviation: %f\n",std);
   double thresh_top = median + 1*std;
   double thresh_bottom = median - 1*std;
   fprintf(stderr,"Thresh top: %f\n",thresh_top);
   fprintf(stderr,"Thresh bottom: %f\n",thresh_bottom);

   /* Replace data that is above/below thresh with gaussian noise*/
   fprintf(stderr,"Replacing data that is outside the threshhold\n");
   int ctr = 0;
   for(j=0; j<=nchan; j++){
      if (mean_spec[j] > thresh_top || mean_spec[j] < thresh_bottom){
         fprintf(stderr,"Channel being replaced: %d\n",STRTCHAN+idx[j]);
         ctr ++;
         for(i=0; i<dsp->num_samples; i++){
             random = gasdev(&seed);
             dsp->data[i*NCHAN + STRTCHAN+idx[j]] = mean;//+ std*random;
         }
      }
   }
   fprintf(stderr,"Number of channels replaced: %d\n",ctr);
   free(idx); 
}


void remove_bad_channel(DataStruct *dsp){
    int current_index;
    double current_sample, oldM, M, S, V, SD;
    double thresh_top, thresh_bottom;
    double random;
    long seed = 5;
    int j,counter=0, index = 0;
    for(long i = 0; i< dsp->num_samples; i++){
        M = 0;
        S = 0;
	oldM = 0;
	index = 0;
	for(int k = STRTCHAN+1; k <= STPCHAN; k++){
	    index++;
	    current_index = i*NCHAN+k-1;
	    current_sample = (double)dsp->data[current_index];
	    oldM = M;
	    M = M + (current_sample-M)/index;
	    S = S + (current_sample-M)*(current_sample-oldM);
	}
	V = S/(NCHAN-1);
	SD = sqrt(V);

        thresh_top = M+SD;
        thresh_bottom = M-SD; 
        for(j = STRTCHAN; j <= STPCHAN; j++){
	        if(dsp->data[i*NCHAN+j] > thresh_top ||
	           dsp->data[i*NCHAN+j] < thresh_bottom){
	    	   counter ++;
	    	   random = gasdev(&seed);
	    	   dsp->data[i*NCHAN+j] = (short int) M;//+random*SD;
	
                }
       	
         }	
    }
}

void process_filterbank_file(char infile[], char outfile[], int pol, 
                             int bad_channel_flag, int narrowband_rfi_flag, 
                             int broadband_rfi_flag, int zero_nonbandpass_flag){
    FILE *fpin, *fpout;
    char c, str[10];
    long n_data_points, num_time_samples, size;
    long long f_size, data_size;
    int header_len, i;
    char header[500];
    
    printf("\nProcessing single filterbank file with flags:\npol: %d\t"
           "badchannel: %d\tnarrowband: %d\tbroadband: %d\tnonbandpass: %d\n\n",
            pol,bad_channel_flag,narrowband_rfi_flag,broadband_rfi_flag,
            zero_nonbandpass_flag);

    if(!(fpin = fopen(infile,"rb")))
    printf("Cannot open the input filename: %s."
        " Do you have permission?\n",infile);
    
    // Open output file and copy header
    if(!(fpout = fopen(outfile,"w")))
    printf("Could not open output file. Write permission?\n");
    
    while(fgets(&c, 1, fpin)!=NULL){
    fread (&c, 1, 1, fpin);
    if(c == 'H'){
    fseek(fpin, -1, SEEK_CUR);
    fread(str, 10, 1, fpin);
    if(strstr(str, "HEADER_END") != NULL)
        break;
    }
    }
    header_len = ftell(fpin);
    rewind(fpin);
    fseek(fpin, 0L, SEEK_END);
    f_size = ftell(fpin);
    data_size = f_size-header_len;
    
    rewind(fpin);
    if (ftell(fpout) < 100 && f_size <= (246345728*8)){
       fread(header, sizeof(char), header_len, fpin);
       fwrite(header, sizeof(char), header_len, fpout);
    }
    printf("Header size = %d bytes\n", header_len);
    printf("Data size =  %lld bytes\n", data_size);
    
    if (data_size == 0)
    printf("Empty filterbank file.\n");
    
    n_data_points = data_size/sizeof(short int);
    num_time_samples = n_data_points/NCHAN;
    printf("Number of data points: %ld\n", n_data_points);
    printf("Number of time samples: %ld\n", num_time_samples);
        
    DataStruct *dsp = create_DataStruct(num_time_samples);

    fprintf(stderr,"time samples from struct: %ld\n",dsp->num_samples);
   
    size = fread(dsp->data, sizeof(short int), n_data_points, fpin);
    if (size != n_data_points) printf("Couldn't read all data. Not sure.\n");
 
    fprintf(stderr,"Data 1 : %hu\n", dsp->data[10240]);
    if (zero_nonbandpass_flag) zero_non_bandpass(dsp);
    if (narrowband_rfi_flag) remove_narrowband_rfi(dsp);
    if (broadband_rfi_flag) remove_broadband_rfi(dsp);
    if (bad_channel_flag) remove_bad_channel(dsp);
    
    // Write cleaned data to file
    fprintf(stderr,"Done! Writing data to output file.\n");
    fwrite(dsp->data,sizeof(short int),n_data_points,fpout);
    fclose(fpin); fclose(fpout);
    free_DataStruct(dsp);
    fprintf(stderr,"Done. Returning to main()\n");

}

void process_raw_data_files(char dirname[], char outfile[], int pol, 
                            int bad_channel_flag, int narrowband_rfi_flag, 
                            int broadband_rfi_flag, int zero_nonbandpass_flag){
    printf("\nProcessing raw data files with flags:\npol: %d\t"
           "badchannel: %d\tnarrowband: %d\tbroadband: %d\tnonbandpass: %d\n\n",
            pol,bad_channel_flag,narrowband_rfi_flag,broadband_rfi_flag,
            zero_nonbandpass_flag);

    FILE *fpin, *fpout;
    long n_data_points, num_time_samples, size;
    long long data_size;
    int ncpy_buf, header_len, i, j;
    char header[363];    

    int num_files = num_files_in_dir(dirname);
    
    char **files;
    files = (char **)malloc(num_files * sizeof *files);
    for(i=0;i<num_files;i++)
    files[i] = (char *)malloc(1024*sizeof(char));
    
    get_files_in_dir(num_files,dirname,files);

    /*for(i=0;i<num_files;i++)
    fprintf(stderr,"File: %s\n",files[i]);*/
    
    fprintf(stderr,"Got file list! Opening output file\n"); 
    if(!(fpout = fopen(outfile,"a+")))
    printf("Could not open output file. Write permission?\n");
    
    //WriteHeader(outfile);
    
    fseek(fpout, 0L, SEEK_END);
    header_len = ftell(fpout);
    
    if(!(fpin = fopen(files[0],"rb")))
    fprintf(stderr,"Could not open file %s\n",files[0]);
    
    fseek(fpin, 0L, SEEK_END);
    data_size = ftell(fpin);
    rewind(fpin);
    
    ncpy_buf = data_size/(ACC_BUFSIZE+NACC);
    n_data_points = (data_size - NACC*ncpy_buf)/sizeof(short int);
    
    if(!(n_data_points == (UDP_DATA*NACC*ncpy_buf/sizeof(short int))))
     printf("Total number of data points doesn't tally. Check header!\n");
    
    int data_points_per_spectrum = NCHAN*4; 
    // size = 1536 * 4pols per chan * sizeof(short int)
    num_time_samples = n_data_points/data_points_per_spectrum;
    
    printf("Number of copy buffers: %d\n", ncpy_buf);
    printf("Number of data points: %ld\n", n_data_points);
    printf("Number of time samples: %ld\n", num_time_samples);
    
    /* For every file check RFI and 
    append cleaned data to output file
    For now assuming only raw files.
    TODO: Filterbank files */
    
    DataStruct *dsp = create_DataStruct(num_time_samples);
    
    for(i=0;i<num_files;i++){
       if(!(fpin = fopen(files[i],"rb")))
          printf ("Could not open input file: %s." 
       	   "Change read permissions?\n",files[i]);
       fprintf(stderr,"Reading file: %s\n",files[i]);
       read_data(dsp, data_size, pol, fpin);
       if (zero_nonbandpass_flag) zero_non_bandpass(dsp);
       if (bad_channel_flag) remove_bad_channel(dsp);
       if (narrowband_rfi_flag) remove_narrowband_rfi(dsp);
       if (broadband_rfi_flag) remove_broadband_rfi(dsp);
    } 

    // Write cleaned data to file    
    printf("Done! Writing data to output file.\n");
    fwrite(dsp->data,sizeof(short int),n_data_points,fpout);
    
    // free malloc-ed arrays
    free_DataStruct(dsp);
    for(i=0;i<num_files;i++) free(files[i]);
    free(files); 

}

void read_data(DataStruct *dsp, long long data_size, int pol, FILE *fp){
    long size;
    long no_cpy_buf = data_size/(ACC_BUFSIZE+NACC);
    long n_data_points = (data_size - NACC*no_cpy_buf)/sizeof(short int);
    double random;
    
    short int *data_full = (short int*)malloc(sizeof(short int)*n_data_points);
    char *flag = (char*)malloc(sizeof(char) * NACC);  
    for (int ii = 0; ii < no_cpy_buf; ii++){
        size = fread((data_full+ii*ACC_BUFSIZE/2), sizeof(short int), 
                                                   ACC_BUFSIZE/2, fp);
        size = fread(flag, sizeof(char), NACC, fp);
        for (int jj = 0; jj < NACC; jj++){
    	   if (flag[jj] == 1 && ii == 0){
    	      printf("Packet number: %d missed, replacing with zeros\n", jj);
    	      for (int kk = 0; kk < 512*4; kk++);
    		//data_full[ii*ACC_BUFSIZE/2+512*4*jj+kk] = 0;
    	   }
           if (flag[jj] == 1 && ii != 0){
              printf("Packet number: %d missed, "
                     "replacing with guassian noise\n", jj);
    	      for (int kk = 0; kk < 512*4; kk++){ 
                 random = gasdev(&seed);
                 data_full[ii*ACC_BUFSIZE/2+512*4*jj+kk] = 
                             data_full[(ii-1)*ACC_BUFSIZE/2+jj*512*4+kk]*random;
              }
           }
        }
    }
    
    for (int i = 0; i < n_data_points/4-1; i++){
       if (!pol) dsp->data[i] = sqrt(data_full[4*i]*data_full[4*i]
                                +data_full[4*i+1]*data_full[4*i+1]);
    //	printf("Data 0 : %hu\n", dsp->data[i]);
       if (pol==1) dsp->data[i] = data_full[4*i];
       if (pol==2) dsp->data[i] = data_full[4*i+1];
    }
    free(data_full);
    free(flag);
}

void free_DataStruct(DataStruct *dsp){
     fprintf(stderr,"Freeing data\n");
     if (dsp != NULL){
        // fprintf(stderr,"free(): ptr to data: %p\n",dsp->data);
        free(dsp->data);
        // fprintf(stderr,"free(): ptr to input: %p\n",dsp->input);
        free(dsp->input);
        // fprintf(stderr,"free(): ptr to avg: %p\n",dsp->avg);
        free(dsp->avg);
        // fprintf(stderr,"free(): ptr to sd: %p\n",dsp->sd);
        free(dsp->sd);
        // fprintf(stderr,"free(): ptr to window: %p\n",dsp->window);
        free(dsp->window);
        free(dsp);
     }
}

void remove_broadband_rfi(DataStruct *dsp){
	printf("Starting broadband RFI removal...\n"); 
	if (!dsp->stats_done){
            init_struct1(dsp, 1, 1000, 3, 0, 1);
	    dsp->stats_done = true; }
        init_struct1(dsp, 100, 1000, 3, 1, 0);
	remove_rfi(dsp);
	dsp->stats_done = true;
        
}

void remove_narrowband_rfi(DataStruct *dsp){
	printf("Starting narrowband RFI removal...\n"); 
        init_struct1(dsp, 1, 1000, 3, 1, 1);
        fprintf(stderr, "remove_narrowband_rfi(): Initialized structure\n");
	remove_rfi(dsp);
	dsp->stats_done = true;
}


void init_struct1(DataStruct *dsp, int channel_width, int window_size, 
                 double thresh_limit, int replace_data, int record_stats){
   dsp->channel_width = channel_width;
   dsp->window_size = window_size;
   dsp->thresh_limit = thresh_limit;
   dsp->counter = 0; 
   dsp->replace_data = replace_data;
   dsp->record_stats = record_stats;
   if (dsp->window) free(dsp->window);
   dsp->window = (double*)malloc(sizeof(double)*dsp->window_size);
   // fprintf(stderr, "Ptr to window %p\n",dsp->window);
} 

void remove_rfi(DataStruct *dsp){
    int start_channel, end_channel;
    int counter;
    double percent;
    int dchan = (STPCHAN-STRTCHAN)/(dsp->channel_width);

    for(int i=0; i<dchan; i++){
    	start_channel = STRTCHAN + i*(dsp->channel_width);
    	end_channel = STRTCHAN + (i+1)*(dsp->channel_width);
    	compute_timesum(dsp, start_channel, end_channel);
    	process_first_window(dsp,   start_channel, end_channel);
        process_remainder(dsp,   start_channel, end_channel);
    }
    percent = (double)dsp->counter/(double)((dsp->num_samples)*
              ((STPCHAN-STRTCHAN)/(dsp->channel_width)))*100;
    printf("Percentage of data flagged as RFI: %lf %% \n\n", percent); 
}

void compute_timesum(DataStruct *dsp, int start_channel, int end_channel){
    double time_sum; 
    for(int i=0; i < dsp->num_samples; i++){
        time_sum = 0;
        for(int j = start_channel; j < end_channel; j++){
            time_sum += dsp->data[NCHAN*i + j];
        }
        dsp->input[i] = time_sum; 
    }
}

void process_first_window(DataStruct *dsp, int start_channel, int end_channel){

    calc_first_window_stats(dsp);

    if (dsp->record_stats) update_avgsd(dsp, start_channel, end_channel, 0);

    for(int k = 1; k < dsp->window_size; k++){
        if (dsp->record_stats) update_avgsd(dsp, start_channel, end_channel, k);
        adjust_data(dsp, start_channel, end_channel, k);
        }
    
    /*Recalculate stats using cleaned array*/
    calc_first_window_stats(dsp);
    
    if (dsp->record_stats) update_avgsd(dsp, start_channel, end_channel, 0);
    for(int k = 0; k < dsp->window_size; k++){
        if (dsp->record_stats) update_avgsd(dsp, start_channel, end_channel, k);
    }

}

void calc_first_window_stats(DataStruct *dsp){
    int current_index;
    double current_sample, oldM, M, S, V;
    M = 0; S = 0;

    for(int i = 0; i < dsp->window_size; i++)
        dsp->window[i] = dsp->input[i];
    
    for(int k = 0; k < dsp->window_size; k++){
        current_sample = (double)dsp->window[k];
        oldM = M;
        M = M + (current_sample- M)/(k+1);
        S = S + (current_sample- M)*(current_sample- oldM);
    }
    V = S/(dsp->window_size-1);
    dsp->curravg = M;
    dsp->currsd = sqrt(V);
}

void process_remainder(DataStruct *dsp, int start_channel, int end_channel){
    double old_sample, current_sample, next_sample, oldM, S, V, newM;

    V = pow(dsp->currsd,2);

    for (long j=dsp->window_size; j < dsp->num_samples; j++){
        /*Clean the current sample based on stats from previous cleaned sample */
        old_sample = dsp->window[0];
        adjust_data(dsp, start_channel, end_channel, j);
        
        /*Adjust window for calculation of stats*/
        memmove(&(dsp->window[0]), &(dsp->window[1]), 
                  (dsp->window_size)*sizeof(double));
        current_sample = dsp->input[j];
        dsp->window[dsp->window_size-1] = dsp->input[j];
        
        if (dsp->record_stats) update_avgsd(dsp, start_channel, end_channel, j);
        /*Update standard deviation and mean*/
        oldM = dsp->curravg;
        newM = oldM + (current_sample-old_sample)/(dsp->window_size);
        V = V + (current_sample-old_sample)*(current_sample- newM +
                        old_sample- oldM)/(dsp->window_size-1);

        dsp->currsd = sqrt(V); dsp->curravg = newM;
    }
}

void update_avgsd(DataStruct *dsp, int start_channel, int end_channel, 
                  long long current_sample){
    long long cs;
    for(int i= start_channel; i < end_channel; i++){
        cs = NCHAN*current_sample + i;
	dsp->avg[cs]= dsp->curravg;
	dsp->sd[cs] = dsp->currsd;
    }
}

void adjust_data(DataStruct *dsp, int start_channel, int end_channel, 
                 long long current_sample){

    double random, thresh_top, thresh_bottom;
    long long currsam, prevsam;

    thresh_top = dsp->curravg + 0.1*dsp->currsd* dsp->thresh_limit;
    thresh_bottom = dsp->curravg - 0.1*dsp->currsd* dsp->thresh_limit;

    if(dsp->input[current_sample] > thresh_top || 
       dsp->input[current_sample] < thresh_bottom){
        random = gasdev(&seed);
	dsp->counter++;
	dsp->window[dsp->window_size] = dsp->curravg + random*dsp->currsd;
	for(int i= start_channel; i < end_channel; i++){
	    currsam = NCHAN*current_sample+i;
	    prevsam = NCHAN*(current_sample-1)+i;
	    if (dsp->replace_data){
               //fprintf(stderr,"Replacing data\n");
               dsp->data[currsam] = 0;} //dsp->avg[prevsam]+random*dsp->sd[prevsam];
 	    random = gasdev(&seed);
	}
    }
}

DataStruct *create_DataStruct(long num_samples){
   printf("Creating DataStruct\n");
   DataStruct *dsp = (DataStruct *)malloc(sizeof(DataStruct));
   dsp->num_samples = num_samples;
   dsp->data = (short int*)malloc(sizeof(short int)*
                           num_samples*NCHAN);
   // fprintf(stderr,"Ptr to data %p\n",dsp->data);
   dsp->input = (double*)malloc(sizeof(double)*num_samples);
   dsp->avg = (double*)malloc(sizeof(double)*num_samples*NCHAN);
   dsp->sd = (double*)malloc(sizeof(double)*num_samples*NCHAN);
   dsp->window = NULL;
   if (!(dsp->data) || !(dsp->input) //|| !(dsp->output) 
       || !(dsp->avg) || !(dsp->sd))
   	printf("Malloc Failed, input data size possibly too large");
   return dsp;
}

	
int main(int argc, char *argv[]){    
    int pol = 0;  // 0 = both pols, 1 = xx, 2 = yy
    int fil_flag = 0; // 0 = raw, 1 = filterbank
    int bad_channel_flag = 0;
    int narrowband_rfi_flag = 0;
    int broadband_rfi_flag = 0;
    int zero_nonbandpass_flag = 0;
    int arg;
    //char *infile = NULL;
    char infile[2048];

    while((arg=getopt(argc,argv,"hd:xyf:bnzr")) !=-1){
      switch(arg){
      case 'h':
        fprintf(stderr,"\nUsage: %s [OPTIONS]\n\n",argv[0]);
        fprintf(stderr,"-h\tPrint this help message\n"
                "-d [dir]  Directory containing raw data\n"
                "-f [file] Filterbank file name\n\n"
                "-x\tProcess only xx pol (default=both)\n"
                "-y\tProcess only yy pol (default=both)\n\n"
                "-b\tRemove broadband RFI\n"
                "-r\tIdentify and remove bad channels\n"
                "-n\tRemove narrowband RFI\n"
                "-z\tZero the non-bandpass channels\n\n");
        return 0;
      case 'x':
        pol = 1; break;
      case 'y':
        pol = 2; break;
      case 'f':
        fil_flag = 1;
        if (!optarg) fprintf(stderr,"No file given." 
                     "Please provide a file to process\n");
        fprintf(stderr,"Input file: %s\n",optarg);
        strcpy(infile,optarg);
        break;
      case 'd':
        if (!optarg) fprintf(stderr,"No directory given."
                     "Please provide directory to process\n");
        strcpy(infile,optarg);
        break;
      case 'b':
        broadband_rfi_flag = 1; break;
      case 'n':
        narrowband_rfi_flag = 1; break;
      case 'z':
        zero_nonbandpass_flag = 1; break;
      case 'r':
        bad_channel_flag = 1; break;
      default:
        fprintf(stderr,"Usage: %s [OPTIONS]\n",argv[0]);
        fprintf(stderr,"-h\tPrint this help message\n"
                "-d\tDirectory containing raw data\n"
                "-f\tFilterbank file name\n\n"
                "-x\tProcess only xx pol (default=both)\n"
                "-y\tProcess only yy pol (default=both)\n\n"
                "-b\tRemove broadband RFI\n"
                "-r\tIdentify and remove bad channels\n"
                "-n\tRemove narrowband RFI\n"
                "-z\tZero the non-bandpass channels\n");
      }
    }
        if (!infile) perror("main(): No files to process!");

        char outfile[2048];
        output_filename(outfile, 0, pol, bad_channel_flag, 
                        narrowband_rfi_flag, broadband_rfi_flag, 
                        zero_nonbandpass_flag);

	if (fil_flag) process_filterbank_file(infile, outfile, pol, bad_channel_flag, 
                                              narrowband_rfi_flag, broadband_rfi_flag,                                              zero_nonbandpass_flag);
	if (!fil_flag) process_raw_data_files(infile, outfile, pol, bad_channel_flag, 
                                              narrowband_rfi_flag, broadband_rfi_flag,
                                              zero_nonbandpass_flag);
	return 0;
}

#define IA 16807
#define IM 2147483647 
#define AM (1.0/IM)
#define IQ 127773
#define IR 2836
#define NTAB 32
#define NDIV (1+(IM-1)/NTAB) 
#define EPS 1.2e-7
#define RNMX (1.0-EPS)

//float gasdev(long *idum);
//float ran1(long *idum);

float ran1(long *idum){
/*“Minimal” random number generator of Park and Miller with Bays-Durham shuffle and added safeguards. Returns a uniform random deviate between 0.0 and 1.0 (exclusive of the endpoint values). Call with idum a negative integer to initialize; thereafter, do not alter idum between successive deviates in a sequence. RNMX should approximate the largest floating value that is less than 1.*/
	int j;
	long k;
	static long iy=0; 
	static long iv[NTAB]; 
	float temp;
	if (*idum <= 0 || !iy) {
		if (-(*idum) < 1) *idum=1; 
		else *idum = -(*idum);
		for (j=NTAB+7;j>=0;j--) {
		//Be sure to prevent idum = 0.
			k=(*idum)/IQ; 
			*idum=IA*(*idum-k*IQ)-IR*k; 
			if (*idum < 0) *idum += IM; 
			if (j < NTAB) iv[j] = *idum;
		}
		iy=iv[0]; 
	}
	k=(*idum)/IQ;
	 *idum=IA*(*idum-k*IQ)-IR*k;
	if (*idum < 0) *idum += IM; 
	j=iy/NDIV;
	iy=iv[j];
	iv[j] = *idum;
	if ((temp=AM*iy) > RNMX) return RNMX; 
	else return temp;
}

float gasdev(long *idum){
/*Returns a normally distributed deviate with zero mean and unit variance, using ran1(idum) as the source of uniform deviates.*/
	static int iset=0; 
	static float gset; 
	float fac,rsq,v1,v2;
	if (*idum < 0) iset=0; 
	if (iset == 0) {
		do { 
			v1=2.0*ran1(idum)-1.0; 
			v2=2.0*ran1(idum)-1.0; 
			rsq=v1*v1+v2*v2;
		} while (rsq >= 1.0 || rsq == 0.0); 
		fac=sqrt(-2.0*log(rsq)/rsq);
		gset=v1*fac;
		iset=1;
		return v2*fac;
	} else {
    	iset=0;
	return gset;
	}
}


/*
void init_narrow_struct(DataStruct *dsp){
	fprintf(stderr, "Initializing Narrow Struct\n");
	dsp->output = dsp->data;
	dsp->channel_width = 1;
	dsp->window_size = 1000;
	dsp->thresh_limit = 3;
	dsp->counter = 0;
	dsp->replace_data = true;
	dsp->record_stats = true;
	dsp->window = (double*)malloc(sizeof(double)*dsp->window_size);
}

void create_stat_struct(DataStruct *dsp){
	dsp->channel_width = 1;
	dsp->window_size = 1000;
	dsp->thresh_limit = 3;
	dsp->counter = 0;
	dsp->replace_data = false;
	dsp->record_stats = true;
	dsp->window = (double*)malloc(sizeof(double)*dsp->window_size);
}*/

