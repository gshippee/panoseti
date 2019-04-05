from struct import *

with open('data_1551133371_3.bin', 'r') as fp:
    header = np.fromfile(fp, dtype='<H', count = n_header)
    data = np.fromfile(fp, dtype='<H', count = n_data)
    flag = np.fromfile(fp, dtype='<B', count = n_flag)
