# acq_data
C code to bind to a socket and record received data to disk. A gcc
complier enabled with OpenMP functionality is required to run the
code.  

acq_data.c  
The data acquisition script.  
Compile using: gcc -fopenmp acq_data.c -o acq  
Run with: ./acq -i <IP address> -p <PORT No>  

acq_udp.h  
Header information for receiving packets. Edit this to suit your specifications and recompile the acquisition script for the changes to take effect.  

acq_data_test.c  
Test script that sends packets to loop back address.  
Compile using: gcc acq_data_test.c -o test  
Run using: ./test  
