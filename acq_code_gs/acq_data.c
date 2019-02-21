#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <omp.h>
#include "acq_udp.h"

#define GET_MOD_NUM(){				\
    memcpy(&mod,sockpar->ubuf+6,UDP_MOD);	\
  }/* Get packet header*/
#define GET_QUAD_NUM(){				\
    memcpy(&quad,sockpar->ubuf+7,UDP_QUAD);	\
  }/* Get packet header*/
#define GET_SEQ_NUM(){				\
    memcpy(&seq,sockpar->ubuf,UDP_SEQ);		\
  }/* Get packet header*/

#define GET_NEXT_PKT(){							\
    while(recvfrom(sockpar->fd,sockpar->ubuf,UDP_PAYLOAD,0,NULL,NULL)!=UDP_PAYLOAD){ \
      if(errno != EAGAIN){						\
	perror("acquire_socket_data");					\
	return 1;							\
      }									\
    }									\
    GET_SEQ_NUM();							\
    GET_MOD_NUM();							\
    GET_QUAD_NUM();							\
  }/*Get UDP_PAYLOAD size data packet from the socket*/

static int DoFinish=0;
void init_sock_stat(SockStat *stat);
void report_socket_stat(SockStat *stat);
int init_socket(SockPar *sockpar);
int acquire_socket_data(SockPar *sockpar, int debug, unsigned char req_mod, unsigned char req_quad, Module *module[]);
int transfer_socket_data(SockPar *sockpar, int debug);

void init_sock_stat(SockStat *stat){ 
  stat->total         = 0;
  stat->got           = 0;
  stat->lost          = 0;
  stat->bad           = 0;
  stat->rate          = 0;
  stat->dtotal        = 0;
  stat->dgot          = 0;
  stat->dlost         = 0;
  stat->dbad          = 0;
  stat->drate         = 0;
  stat->start.tv_sec  = 0;
  stat->start.tv_nsec = 0;
  stat->dstart.tv_sec = 0;
  stat->dstart.tv_nsec= 0;
  stat->log_rate      = 1*NACC; // should be a multiple of NACC

  return;
}

void report_socket_stat(SockStat *stat){
  struct timespec curr_time;
  double          dt,ddt;
  time_t          now = time(0);
  char            time[100];
  
  clock_gettime(CLOCK_MONOTONIC,&curr_time);
  dt=(curr_time.tv_sec + curr_time.tv_nsec/1.0e9)-(stat->start.tv_sec 
				   + stat->start.tv_nsec/1.0e9);
  ddt=(curr_time.tv_sec + curr_time.tv_nsec/1.0e9)-(stat->dstart.tv_sec 
				   + stat->dstart.tv_nsec/1.0e9);
  strftime(time,100,"%Y-%m-%d_%T", localtime(&now));
  
  /* report the statistics */
  fprintf(stderr,"Time: %s\n",time);
  fprintf(stderr,"Cumulative  Stats: Total/Got/Lost/Bad  %ld/%ld/%ld/%ld\n",
	  stat->total+stat->lost+stat->bad,stat->got,stat->lost,stat->bad);
  fprintf(stderr,"Differential Stats: Total/Got/Lost/Bad  %ld/%ld/%ld/%ld\n",
	  stat->dtotal+stat->dlost+stat->dbad,stat->dgot,stat->dlost,
	  stat->dbad);
  fprintf(stderr,"Cummlative/Differental Rate:  %10.2f/%-10.2f (Mbytes/sec)\n",
	  stat->got*UDP_PAYLOAD/(1024.0*1024.0*dt),
	  stat->dgot*UDP_PAYLOAD/(1024.0*1024.0*ddt));
  fprintf(stderr,"Cummlative/Differental Loss:     %10.2f/%-10.2f\n",
	  stat->lost/(1.0*stat->total),stat->dlost/(1.0*stat->dtotal));
  
  /* reset the differential statistics counters */
  stat->dstart = curr_time;
  stat->dtotal = 0;
  stat->dgot   = 0;
  stat->dlost  = 0;
  stat->dbad   = 0;

  return;
}

int init_socket(SockPar *sockpar){
  int i,j;

  /* set the default socket parameters */
  sockpar->fd                   = -1;
  sockpar->addr.sin_family      = PF_INET;  
  sockpar->addr.sin_addr.s_addr = inet_addr(UDP_IP);
  sockpar->addr.sin_port        = htons(UDP_PORT); 

  /* set the accum buffer parameters */
  for(i=0;i<NSOCKBUF;i++){
    SockBuf *sbuf=sockpar->sbuf+i;
    if((sbuf->data=malloc(ACC_BUFSIZE))==NULL){
      fprintf(stderr,"init_socket() Malloc Failure\n");
      return 1;
    }
    if((sbuf->flag=malloc(ACC_FLGSIZE))==NULL){
      fprintf(stderr,"init_socket() Malloc Failure\n");
      return 1;
    }
    for(j=0;j<NACC;j++)sbuf->flag[j]=1; // all data pre-flagged
    sbuf->start =  0;
    sbuf->stop  =  0;
    sbuf->count =  0;
    sbuf->idx   =  i;
  }   
  
  /* initialise pointers to curr, next and copy buffers */
  sockpar->curr     = &sockpar->sbuf[0];
  sockpar->next     = &sockpar->sbuf[1];
  sockpar->copy     = NULL;
  
  /* threshold filling fraction of the "next" buffer. If more than this
     fraction of the "next" buffer is filled, stop waiting for packets from 
     the "current" buffer */
  sockpar->switch_thresh  = 0.7;

  init_sock_stat(&sockpar->stat);
  
  return 1;
}

int acquire_socket_data(SockPar *sockpar, int debug, unsigned char req_mod, unsigned char req_quad, Module *module[] ){
  SockStat      *stat=&sockpar->stat;
  unsigned      i,j;
  unsigned int seq,seq_tmp,last_seq, start_seq;
  unsigned char 		mod, quad;
  int           ngood,ntry,idx;
  int 		first_time=0;
  unsigned long offset=0;

  while(!DoFinish){
    GET_NEXT_PKT();
    if (mod == req_mod && quad == req_quad){
    //fprintf(stderr,"curr: %ld, module: %x, quadrant %x, %x, %x\n",seq, mod, quad, req_mod, req_quad);
	    if(sockpar->curr->start==0 && first_time==0){
	      // need to find a place to  start. Wait till we have 3 successive 
	      // packets with consecutive packet numbers
	      for(i=0;i<3;i++){
		SockBuf *sbuf=sockpar->sbuf+i;
		for(j=0;j<NACC;j++)sbuf->flag[j]=1; // all data pre-flagged
	      }
	      ngood    = 0;
	      last_seq = seq;
	      ntry     = 0;
	      while(ngood<3){
		GET_NEXT_PKT();
    		if (mod == req_mod && quad == req_quad){
			//fprintf(stderr,"curr: %ld, module: %x, quadrant %x\n",seq, mod, quad);
			if(seq-last_seq == 1) ngood++;
			else ngood=0;
			last_seq=seq; ntry++;
		}
		if(ntry>1024){
		  fprintf(stderr,"acq: Can't find a good place to start!\n");
		  return -1;
		}
	      }
	      fprintf(stderr,"acq: Starting with PktNo %ld\n",seq);
	      first_time = 1;
	      start_seq = seq;
	      sockpar->curr->start = NACC/NUM_MOD*(unsigned int)req_mod;
	      sockpar->curr->stop  = NACC/NUM_MOD*((unsigned int)req_mod+1); // first packet of next buffer
	      sockpar->next->start = sockpar->curr->stop; 
	      sockpar->next->stop  = sockpar->next->start+NACC/NUM_MOD*(unsigned int)req_mod;
	      //fprintf(stderr,"Start %ld\n", sockpar->curr->start);
	      //fprintf(stderr,"Stop %ld\n", sockpar->curr->stop);
	      //fprintf(stderr,"Start %ld\n", NACC/NUM_MOD*(unsigned int)req_mod);
	      //fprintf(stderr,"Stop %ld\n", NACC/NUM_MOD*((unsigned int)req_mod +1));
	      clock_gettime(CLOCK_MONOTONIC,&stat->start);
	      stat->dstart=stat->start;
	    }
	    seq = (seq-start_seq+NACC/NUM_MOD*(unsigned int)req_mod)%NACC;
	    /* copy packet data to the correct location, if possible */
	    if(seq >= NACC/NUM_MOD*(unsigned int)req_mod && seq < NACC/NUM_MOD*((unsigned int)req_mod+1)){
	    	fprintf(stderr,"curr: %ld, module: %x, quadrant %x, count %ld\n",seq, mod, quad, sockpar->curr->count);
		/* packet belongs to current buffer */
		if (debug) fprintf(stderr,"curr: %ld\n",seq);
		offset=seq;
		if(!sockpar->curr->flag[offset]){
			/* duplicate packet! */
			stat->bad++;stat->dbad++;
		}else{
		    sockpar->curr->flag[offset]=0;
		    offset=offset*UDP_PAYLOAD;
		    memcpy(sockpar->curr->data+offset,sockpar->ubuf,UDP_PAYLOAD);
		    sockpar->curr->count++;
		    stat->got++; stat->dgot++;
		}
	    }else if(seq >= NACC/NUM_MOD*((unsigned int)req_mod+1) && seq < NACC/NUM_MOD*((unsigned int)req_mod+1)+NACC){
	    	//fprintf(stderr,"curr: %ld, module: %x, quadrant %x\n",seq, mod, quad);
		/* packet belongs to next buffer */
		if (debug) fprintf(stderr,"next: %ld\n",seq);
		offset=seq;
		if(!sockpar->next->flag[offset]){
		    /* duplicate packet */
		    stat->bad++;stat->dbad++;
		}else{
		    sockpar->next->flag[offset]=0;
		    offset=offset*UDP_PAYLOAD;
		    memcpy(sockpar->next->data+offset,sockpar->ubuf,UDP_PAYLOAD);
		    sockpar->next->count++;
		    stat->got++;stat->dgot++;
		}
	    }else{
		    /* packet is badly out of time, drop */
		if (debug) fprintf(stderr,"pkt drop\n");
		    stat->bad++;stat->dbad++;
	    }

	    if((sockpar->curr->count == NACC) || 
	       (sockpar->next->count > sockpar->switch_thresh*NACC)){
	      /* update number of lost packets */
	      stat->total         += NACC;
	      stat->dtotal        += NACC;
	      stat->lost          += (NACC-sockpar->curr->count);
	      stat->dlost         += (NACC-sockpar->curr->count);
	      sockpar->copy        = sockpar->curr;
	      sockpar->curr        = sockpar->next;
	      /* update the "next" buffer pointer and preflag all data */
	      idx                  = (sockpar->next->idx+1)%NSOCKBUF;
	      sockpar->next        = sockpar->sbuf+idx;
	      sockpar->next->count = 0;
	      sockpar->next->start = sockpar->curr->stop;
	      sockpar->next->stop  = sockpar->next->start+NACC;
	      for(i=0;i<NACC;i++) sockpar->next->flag[i]=1;
	    }

	    /*Restart if you get a bunch of bad packets*/
	    if(stat->bad > 0.75*NACC/NUM_MOD){
	      fprintf(stderr,"\n\nGot a lot of bad packets. Resetting.\n\n");
	      //TODO: Transfer useful data before this
	      GET_NEXT_PKT()
	      sockpar->curr->start = seq;
	      sockpar->curr->stop  = seq+NACC;
	      sockpar->next->start = sockpar->curr->stop;
	      sockpar->next->stop  = sockpar->next->start+NACC;
	      //sockpar->curr->start = NACC/NUM_MOD*(unsigned int)req_mod;
	      //sockpar->curr->stop  = NACC/NUM_MOD*((unsigned int)req_mod+1); // first packet of next buffer
	      //sockpar->next->start = sockpar->curr->stop; 
	      //sockpar->next->stop  = sockpar->next->start+NACC/NUM_MOD*(unsigned int)req_mod;
	      for(i=0;i<3;i++){
		SockBuf *sbuf=sockpar->sbuf+i;
		for(j=0;j<NACC;j++)sbuf->flag[j]=1; // pre-flag data
	      }
	      clock_gettime(CLOCK_MONOTONIC,&stat->start);
	      stat->dstart=stat->start;
	      stat->bad=0;
	      stat->got=0; //to reset statistics info
	    }

	    /* report statististics periodically */
	    if((stat->dgot + stat->dbad) == stat->log_rate)
	      report_socket_stat(stat);

	  }//DoFinish
	    }
	    
	    /* free buffers, allocation was done in init_socket() */
	    for(i=0;i<NSOCKBUF;i++){
	      free(sockpar->sbuf[i].data);
	      free(sockpar->sbuf[i].flag);
	    }

    return 1;
}

int transfer_socket_data(SockPar *sockpar, int debug){

  unsigned int     max_pkt = (FILESIZE*1024*1024)/ACC_BUFSIZE;
  unsigned int     npkt=0,i,idx0,idx,sleep_time,cnt=0;
  char             filename[50];
  SockBuf          *idxc;
  FILE             *fp;
  struct tm        *now;
  time_t           rawtime;
  
  idx0              = NSOCKBUF-1;
  sleep_time        = 0.1; // micro seconds

  fprintf(stderr,"Listening on %s:%d \n",
	  inet_ntoa(sockpar->addr.sin_addr),ntohs(sockpar->addr.sin_port));

  /* Include time and date info in the data file*/
  time(&rawtime);
  now = localtime(&rawtime);
  strftime(filename,sizeof(filename),"data_%s.bin",now);	
  fp=fopen(filename,"a+");
  
  while(!DoFinish){
    if(sockpar->copy != NULL && (idx=sockpar->copy->idx) != idx0){
      /* got fresh data*/
      if (debug) fprintf(stderr,"idx: %d\tidx0: %d\n",idx,idx0);
      if(idx != (idx0+1)%NSOCKBUF) // missed at least one buf!
	     fprintf(stderr,"transfer_socket_data(): missed copying a buffer!\n");

      idx0 = idx;
      idxc = sockpar->copy;
      
      if (npkt==max_pkt){
	fclose(fp);
	npkt=0;
	time(&rawtime);
	now = localtime(&rawtime);
	strftime(filename,sizeof(filename),"data_%s.bin",now);	
	fp=fopen(filename,"a+");
      }
      fwrite(idxc->data, UDP_DATA, NACC, fp);
      fwrite(idxc->flag, sizeof(unsigned char), NACC, fp);
      npkt++;
    }else{ /* data not available yet, check again*/
      usleep(sleep_time);
      //for(i=0;i<1000;i++);
    }
  }
  return 0;
}

int main(int argc, char **argv){
  SockPar          sockpar;
  SockStat        *stat=&sockpar.stat;
  int              tid,arg;
  unsigned short   port;
  char 		   req_mod;
  char		   req_quad;
  int              sockbufsize=1024*1024*1024; // 512 MB socket buffer
  int              debug = 0;
  Module module[NUM_MODULES];


  /* check if the user want's to override the defaults */
  while((arg=getopt(argc,argv,"hvi:p:")) !=-1){
    switch(arg){
    case 'h':
      fprintf(stderr,"\nUsage: %s [OPTIONS]\n\n",argv[0]);
      fprintf(stderr,"-h\tPrint this help essage\n"
                     "-i\tIP address of host\n"
                     "-p\tPort of the host\n"
                     "-v\tPrint helpful debug messages\n");
      return 0;
    case 'i':
      fprintf(stderr,"inet_addr= %s\n",optarg);
      if(inet_aton(optarg,&sockpar.addr.sin_addr)==0){
	     fprintf(stderr,"Illegal IP address %s\n",optarg);
	     return 1;
      }
      break;
    case 'p':
      port=(unsigned short)atoi(optarg);
      sockpar.addr.sin_port = htons(port); 
      break;
    case 'v':
      debug = 1; break;
    default:
      fprintf(stderr,"\nUsage: %s [OPTIONS]\n\n",argv[0]);
      fprintf(stderr,"-h\tPrint this help essage\n"
                     "-i\tIP address of host\n"
                     "-p\tPort of the host\n"
                     "-v\tPrint helpful debug messages\n");
      return 1;
    }
  }

  init_socket(&sockpar);

  /* open the socket and bind to the specified address */
  if((sockpar.fd=socket(AF_INET,SOCK_DGRAM|SOCK_NONBLOCK,IPPROTO_UDP))<0){
    perror("acq: Can't open socket");
    return 1;
  }
  if(bind(sockpar.fd,(struct sockaddr*)&sockpar.addr,
	  (socklen_t)sizeof(struct sockaddr))){
    perror("acq: Can't bind socket");
    return 1;
  }


  /* set the socket buffer size to some large value */
  if(setsockopt(sockpar.fd, SOL_SOCKET, SO_RCVBUF, &sockbufsize, sizeof(int))){
    perror("acq: Can't set SockBufSize");
    return 1;
  }

#pragma omp parallel num_threads(2) private (tid) shared(sockpar)
  { tid  = omp_get_thread_num(); 
    /*if(tid==0){
      acquire_socket_data(&sockpar,debug, tid, 1);  // copy data from socket 
    }
    if(tid==1){
      acquire_socket_data(&sockpar,debug, tid, 1);  // copy data from socket 
    }*/
	  fprintf(stderr, "TID: %ud\n", tid);
    if(tid==0){
      acquire_socket_data(&sockpar,debug, tid, 0, &module);  // copy data from socket 
    }
    else{
      transfer_socket_data(&sockpar,debug); //transfer socket data
    }
  }

  return 0;
}
