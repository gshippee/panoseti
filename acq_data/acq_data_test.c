/*Sends MAX_PKTS number of packets to each port*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define UDP_HDR        16                        //UDP pkt HDRsize (bytes)
#define UDP_DATA       512                     //UDP pkt DATAsize (8192 bytes)
#define UDP_PAYLOAD    (UDP_HDR+UDP_DATA)       //UDP pkt total size (bytes) 
#define SRV_IP         "127.0.0.1"
#define PORTS          1                        //Number of ports to send the data to
#define MAX_PKTS       819200                   //Number of packets to be sent to each port of the receiver
#define SLEEPTIME      60000                    //Parameter that controls the datarate
int port_num[] = {10000, 9930, 9931, 9932, 9933, 9934, 9935, 9936, 9937, 9938, 9939, 9940};

int main(void){ 
  struct sockaddr_in s_serv;
  int soc,i,slen=sizeof(s_serv);
  unsigned long *pkt_no;
  unsigned char *mod_no;
  unsigned char *quad_no;
  unsigned char buf[UDP_PAYLOAD];
  struct timeval curr_time, start_time;
  double dt;
  
  if ((soc = socket(AF_INET, SOCK_DGRAM, 0))<0) 
    perror("Cannot create socket");
  else{
    //Setting the server address
    memset((char *)&s_serv, 0, slen);
    s_serv.sin_family = AF_INET;
    s_serv.sin_port = htons(port_num[0]);
    if(inet_aton(SRV_IP, &s_serv.sin_addr)==0) 
      fprintf(stderr, "inet_aton() failed\n");
    
    memset(buf, 0, UDP_PAYLOAD);
    pkt_no = (unsigned long *)(buf);
    mod_no = (unsigned char *)(buf+8);
    quad_no = (unsigned char *)(buf+10);
    (*mod_no) = 0;
    (*quad_no) = 1;
    (*pkt_no) = 1;
    
    gettimeofday(&start_time,NULL);
   
    char module_no = '0';
    char quad_no = '1';
    while ((*pkt_no)!=MAX_PKTS){
      int sending = sendto(soc, buf, UDP_PAYLOAD, 0, (struct sockaddr *)&s_serv, slen);
      (*pkt_no)++;
      for(i=0;i<SLEEPTIME;i++){} 
    }
    
    gettimeofday(&curr_time,NULL);
    dt=(curr_time.tv_sec + curr_time.tv_usec/1.0e6)-(start_time.tv_sec + start_time.tv_usec/1.0e6);
    
    /* memset(buf, 0, UDP_PAYLOAD); */
    /* int sending = sendto(soc, buf, UDP_PAYLOAD, 0, (struct sockaddr *)&s_serv, slen); */    
    fprintf(stderr, "Data rate: %10.2f (Mbytes/sec)\n", (double)MAX_PKTS*(double)UDP_PAYLOAD/(1024.0*1024.0*dt));
    
    close(soc);
  }
}

