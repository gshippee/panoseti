import socket
import numpy as np
import time
import sys
from struct import *
import datetime
import pickle
from multiprocessing import Pool

def UtcNow():
    now = datetime.datetime.utcnow()
    return (now - datetime.datetime(1970, 1, 1)).total_seconds(), now.microsecond


def MakePacket(module, quad, packet_no):
    header = pack('2H4s2l', 1, packet_no%65536, str(module+.1*quad).zfill(4), UtcNow()[0], UtcNow()[1])
    data = pack('256h', *np.random.poisson(5, 256))
    packet = header+data
    return packet
    #print(unpack('2h4s2l256h',packet))
    #print('')
    #print('')
    #print('')



def MakePacket_helper(args):
    return MakePacket(*args)

def Quabo(module):
    global sockets
    quads = [1,2,3,4]
    i = 0
    socket = sockets[module]
    while(True):
        job_args = [(module,quad, i) for j,quad in enumerate(quads)] 
        socket[0].sendto(MakePacket(*job_args[0]), (localIP, localPort))
        socket[1].sendto(MakePacket(*job_args[1]), (localIP, localPort))
        socket[2].sendto(MakePacket(*job_args[2]), (localIP, localPort))
        socket[3].sendto(MakePacket(*job_args[3]), (localIP, localPort))
        #time.sleep(.001)
        i+=1

no_modules = 1
sockets = []
quabo_sockets = []
for i in range(no_modules):
    for j in range(4):
        vars()['sock'+str(j)] = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        quabo_sockets.append(vars()['sock'+str(j)])
    #vars()['sock'+str(i)] = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    #sockets.append(vars()['sock'+str(i)])
    sockets.append(quabo_sockets)
modules = np.arange(0,no_modules,1)

def main():

    q = Pool(no_modules)
    start_time = datetime.datetime.utcnow()
    q.map(Quabo,modules)
    end_time = datetime.datetime.utcnow()
    print(end_time-start_time)



sock1 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
#localIP = "127.0.0.1"
localIP = '100.64.4.188'
localPort = 6789
bufferSize = 1024
Message = "Hello"
Message= 'f1a525da11f6'.decode('hex')
Message1 = "Goodbye"

MakePacket(1,1,0)
if __name__ == '__main__':
    main()

