#ifndef _ACQ_UDP_INCLUDED_
#define _ACQ_UDP_INCLUDED_

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

#define NUM_MOD		 1			  // number of modules 
#define UDP_SEQ		 4			  // UDP pkt seq length (bytes)
#define UDP_MOD		 1			  // UDP pkt module (bytes)
#define UDP_QUAD	 1			  // UDP pkt quadrant (bytes)
#define UDP_HDR         16                        // UDP pkt HDRsize (bytes)
#define UDP_DATA       512                        // UDP pkt DATAsize (bytes)
#define UDP_PAYLOAD   (UDP_HDR+UDP_DATA)          // UDP pkt total size (bytes) 
#define NSOCKBUF         3                        // num of accumulate buffers
#define NACC          8192                        // number of packets to 
                                                  // accumulate per buffer
#define ACC_BUFSIZE   (NACC*UDP_PAYLOAD)          // Data accumulate buffer size
#define ACC_FLGSIZE    NACC                       // flg accumulate buffer size
#define FILESIZE        102                       // File size for data storage in MB

//#define UDP_IP        "10.0.10.10"                // default ip addr
#define UDP_IP        "127.0.0.1"                // default ip addr
#define UDP_PORT       10000                      // default port number

typedef struct {
  /*
    keep track of cumulative numbers, as well as differential numbers 
     (i.e. the updates since the last log report). A log report is issued
     after every log_rate packets.
  */
  struct timespec start,dstart;  // start time of the data acquisition
  unsigned long   total,dtotal;  // estimated total packets sent
  unsigned long   got,dgot;      // packets got
  unsigned long   lost,dlost;    // packets lost (i.e. never got)
  unsigned long   bad,dbad;      // packets completely out of sequence
  double          rate,drate;    // estimated data rate (M bytes/s)
  long            log_rate;      // Number of packets after which to report 
                                // stats (< 0 ==> don't generate report)
}SockStat;

typedef struct{
  unsigned char   *data;        // data buf (size ACC_BUFSIZE) 
  unsigned char   *flag;        // flag buf (size ACC_FLGSIZE) 
  unsigned long    start;       // seq no of first packet in buf
  unsigned long    stop;        // seq no of first packet in next buf
  unsigned int     idx;         // index of this buf in the circular buffer
  unsigned int     count;       // no of pkts recieved 
} SockBuf;

typedef struct {
  int                fd;                // socket file descriptor
  struct sockaddr_in addr;              // socket address
  unsigned char      ubuf[UDP_PAYLOAD]; // buffer into which to copy udp pkt
  SockBuf            sbuf[NSOCKBUF];    // circular buffer into which to 
                                        // accumulate udp data
  SockBuf            *curr;             // pointer to current accum buf
  SockBuf            *next;             // pointer to next accum buf
  SockBuf            *copy;             // pointer to filled (read to copy)
                                        // accum buf
  double             switch_thresh;     // threshold fill fraction of "next" 
                                        // buffer to stop  waiting for old pkts
  SockStat           stat;              // stats of packets recieved, lost etc.
}SockPar;

#endif 

