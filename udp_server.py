import socket
import pickle
from struct import *
import time
from astropy.io import fits
import sys
'''
hdu = fits.open('p_d0w0m0q0_20181213_164730_23988.fits')
print(len(hdu))
hdr = hdu[0].header
calcsize(hdu[0].header)
print(hdr)
keys = hdr.keys()
print(keys)
print(hdu[0].header[keys[0]])
for i in range(len(keys)):
    print(keys[i]+': '+str(hdu[0].header[keys[i]]))
for i in range(len(hdu)-1):
    data = hdu[i].data
'''

UDP_IP = "127.0.0.1"
UDP_PORT = 6789

sock = socket.socket(socket.AF_INET, # Internet
                     socket.SOCK_DGRAM) # UDP
sock.bind((UDP_IP, UDP_PORT))
packet_no = []
while True:
    data, addr = sock.recvfrom(1024) # buffer size is 1024 bytes
    #print((unpack('2H4s2l256h',(data)))[1])
    packet_no.append((unpack('2H4s2l256h',(data)))[1])
    #if len(packet_no)%10000==0:
    print('No packets: ' + str(len(packet_no)))
    time.sleep(.0001)
