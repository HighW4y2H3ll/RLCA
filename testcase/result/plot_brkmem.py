import os
import sys
import matplotlib.pyplot as plt


tgt = [i for i in range(5)]
name = ['python', 'ffmpeg 3.1.3', 'tar', 'gzip', 'gcc']

mnobrk = [ 13 , 16 , 4, 3 , 23]
mbrk = [15, 18, 5, 4, 27]

tnobrk = [ 0.29 , 0.175 , 0.129 , 0.192 , 0.985]
t256k = [0.82 , 0.43 , 0.129 , 1.341 , 2.37]
t64m = [1.041 , 0.81 , 0.129 , 1.339 , 2.605]
t2g = [1.028 , 3.41 , 0.154 , 1.343 , 36.958]
t4g = [16.739 , 3.421 , 0.158 , 1.331 , 107.25]
tunlimit = [16.61 , 3.999 , 0.669 , 6.662 , 124.75]

barwidth = 0.35
opacity = 0.4

# memory
fig, ax = plt.subplots()
lb = ['RLCA with brk tracing', 'RLCA without brk tracing']
rt1 = plt.bar(tgt, mbrk, barwidth, fill=False, hatch='/', label=lb[0])
rt2 = plt.bar([i+barwidth for i in tgt], mnobrk, barwidth, fill=False, hatch='\\', label=lb[1])


plt.xticks([i+barwidth for i in tgt], name)
plt.ylabel("Memory Usage (MB)", fontsize=20)
plt.ylim(0, 40)
plt.legend()
fig.savefig('/home/hu/Desktop/brkmem.png')
plt.close(fig)



