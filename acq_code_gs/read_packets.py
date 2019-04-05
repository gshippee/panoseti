from struct import *
import numpy as np
import matplotlib.pyplot as plt
from astropy.io import fits

def rotated(array_2d):
    list_of_tuples = zip(*array_2d[::-1])
    return [list(elem) for elem in list_of_tuples]

data,header = fits.getdata('p_d0w0m0q0_20181213_164730_23988.fits', header = True)
hdu_number = 0
fits.getheader('p_d0w0m0q0_20181213_164730_23988.fits', hdu_number)
print(header)

with open("data_1554492570_3.bin", "rb") as f:
    while True:
        try:
            bytesback = f.read(16)
            data = unpack('256H',f.read(512))
            data = np.array(data).reshape(16,16)
        except: 
            print("End of packet")
        acq_mode = bytesback[0]
        pktno = bytesback[3]*256 +bytesback[2]
        UTC = bytesback[9]*256*256*256 + bytesback[8]*256*256 + bytesback[7]*256 +bytesback[6]
        NANO = bytesback[13]*256*256*256 + bytesback[12]*256*256 + bytesback[11]*256 +bytesback[10]
        header['ACQMODE'] = acq_mode
        header['PACKETNO'] = pktno
        header['UTC'] = UTC
        header['NANOSEC'] = NANO
        file_name = "data_"+str(pktno)+".fits"
        fits.writeto(file_name, data, header, clobber = True)
        print(pktno)
        #print(len(bytesback))
        #print("acqmode= " + str(bytesback[0])+ ", pktno = " + str(pktno) + ", bdloc= " + hex(bytesback[5]*256 +bytesback[4]),end='')
        #print ("elapsed time= " + str(ET) + str(" pktno = ") + str(pktno) + "\n")


        

plt.imshow(data)
plt.show()
