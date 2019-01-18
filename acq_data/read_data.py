from struct import *
import sys

with open('data_1547763551.bin', mode = 'rb') as file:
    while True:
        header = file.read(16)
        data = file.read(512)
        packet = unpack("LHBB2l", header)
        print(packet)
