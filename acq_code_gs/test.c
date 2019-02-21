/*Sends MAX_PKTS number of packets to each port*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <omp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define NUM_CLIENTS	8
#define UDP_HDR        16                        //UDP pkt HDRsize (bytes)

#define UDP_DATA       512                     //UDP pkt DATAsize (8192 bytes)
#define UDP_PAYLOAD    (UDP_HDR+UDP_DATA)       //UDP pkt total size (bytes) 
#define SRV_IP         "127.0.0.1"
#define PORTS          1                        //Number of ports to send the data to
#define MAX_PKTS       819200                   //Number of packets to be sent to each port of the receiver
#define SLEEPTIME      60000                    //Parameter that controls the datarate
int port_num[] = {10000, 9930, 9931, 9932, 9933, 9934, 9935, 9936, 9937, 9938, 9939, 9940};

typedef struct {
	unsigned int  *pkt_no;
	unsigned char *mod_no;
	unsigned char *quad_no;
	unsigned char buf[UDP_PAYLOAD];
	struct sockaddr_in s_serv;
	int soc,i,slen;//slen=sizeof(s_serv);
} Client;

void send_packets(Client client){
      int sending = sendto(client.soc, client.buf, UDP_PAYLOAD, 0, (struct sockaddr *)&client.s_serv, client.slen);
      (*client.pkt_no)++;
      for(int i=0;i<SLEEPTIME;i++){} 
      //fprintf(stderr,"Module no: %x\n", *client.mod_no);
     // fprintf(stderr,"Packet no: %ld, Module no: %x\n", *client.pkt_no, *client.mod_no);
}


int main(void){ 
  struct timeval curr_time, start_time;
  double dt;
  int i,tid;
  int first_loop = 0;
  Client client[NUM_CLIENTS];
  for(int j = 0; j <= NUM_CLIENTS;j++){
	  client[j].slen=sizeof(client[j].s_serv);
	  if ((client[j].soc = socket(AF_INET, SOCK_DGRAM, 0))<0) 
	    perror("Cannot create socket");
	  else{
	    //Setting the server address
	    memset((char *)&client[j].s_serv, 0, client[j].slen);
	    client[j].s_serv.sin_family = AF_INET;
	    client[j].s_serv.sin_port = htons(10000+j);
	    if(inet_aton(SRV_IP, &client[j].s_serv.sin_addr)==0) 
	      fprintf(stderr, "inet_aton() failed\n");
	  }
    	  memset(client[j].buf, 0, UDP_PAYLOAD);
	  client[j].pkt_no = (unsigned int *)(client[j].buf);
	  client[j].mod_no = (unsigned char *)(client[j].buf+6);
	  client[j].quad_no = (unsigned char *)(client[j].buf+7);
	  *client[j].mod_no=j;
	  *client[j].quad_no=0;
	  *client[j].pkt_no=1;
	  client[j].slen=sizeof(client[j].s_serv);
  }

  Client start;
  start.slen=sizeof(start.s_serv);
  if ((start.soc = socket(AF_INET, SOCK_DGRAM, 0))<0) 
    perror("Cannot create socket");
  else{
    //Setting the server address
    memset((char *)&start.s_serv, 0, start.slen);
    start.s_serv.sin_family = AF_INET;
    start.s_serv.sin_port = htons(port_num[0]);
    if(inet_aton(SRV_IP, &start.s_serv.sin_addr)==0) 
      fprintf(stderr, "inet_aton() failed\n");
  }
  memset(start.buf, 0, UDP_PAYLOAD);
  start.pkt_no = (unsigned int *)(start.buf);
  start.mod_no = (unsigned char *)(start.buf+6);
  start.quad_no = (unsigned char *)(start.buf+7);
  *start.mod_no=250;
  *start.quad_no=0;
  *start.pkt_no=1;
  start.slen=sizeof(start.s_serv);
  gettimeofday(&start_time,NULL);
  int sending = sendto(start.soc, start.buf, UDP_PAYLOAD, 0, (struct sockaddr *)&start.s_serv, start.slen);
#pragma omp parallel num_threads(NUM_CLIENTS) private (tid) 
  { 
    while ((*client[0].pkt_no)!=MAX_PKTS){
    tid = omp_get_thread_num();
    send_packets(client[tid]);
    }
  }    
    gettimeofday(&curr_time,NULL);
    dt=(curr_time.tv_sec + curr_time.tv_usec/1.0e6)-(start_time.tv_sec + start_time.tv_usec/1.0e6);
    
    fprintf(stderr, "Data rate: %10.2f (Mbytes/sec)\n", (double)NUM_CLIENTS*(double)MAX_PKTS*(double)UDP_PAYLOAD/(1024.0*1024.0*dt));
  for(int k =0;k<=NUM_CLIENTS;k++){ 
    close(start.soc);
  }
  return 0;
}

