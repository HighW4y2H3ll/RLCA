import os
import sys
import matplotlib.pyplot as plt


tgt = [i for i in range(5)]
name = ['python', 'ffmpeg 3.1.3', 'tar', 'gzip', 'gcc']

mnobrk = [ 13 , 16 , 4, 3 , 23]
mbrk = [15, 18, 5, 4, 27]

tnul = [0.26, 0.129, 0.11, 0.184, 0.753]
tnobrk = [ 0.29 , 0.175 , 0.129 , 0.192 , 0.985]
t256k = [0.82 , 0.43 , 0.129 , 1.341 , 2.37]
t64m = [1.041 , 0.81 , 0.129 , 1.339 , 2.605]
t2g = [1.028 , 3.41 , 0.154 , 1.343 , 36.958]
t4g = [16.739 , 3.421 , 0.158 , 1.331 , 107.25]
tunlimit = [16.61 , 3.999 , 0.669 , 6.662 , 124.75]

ntnobrk = [tnobrk[i]/tnul[i] for i in range(5)]
nt256k = [t256k[i]/tnul[i] for i in range(5)]
nt64m = [t64m[i]/tnul[i] for i in range(5)]
nt2g = [t2g[i]/tnul[i] for i in range(5)]
nt4g = [t4g[i]/tnul[i] for i in range(5)]
ntunlimit = [tunlimit[i]/tnul[i] for i in range(5)]

barwidth = 0.15
opacity = 0.4

# memory
fig, ax = plt.subplots()
lb = ['no brk', '256KB', '64MB', '2GB', '4GB']
rt1 = plt.bar(tgt, ntnobrk, barwidth, fill=False, hatch='+', label=lb[0])
rt2 = plt.bar([i+barwidth for i in tgt], nt256k, barwidth, fill=False, hatch='\\', label=lb[1])
rt3 = plt.bar([i+barwidth*2 for i in tgt], nt64m, barwidth, fill=False, hatch='/', label=lb[2])
rt4 = plt.bar([i+barwidth*3 for i in tgt], nt2g, barwidth, fill=False, hatch='x', label=lb[3])
rt5 = plt.bar([i+barwidth*4 for i in tgt], nt4g, barwidth, fill=False, hatch='-', label=lb[4])


plt.xticks([i+barwidth*2.5 for i in tgt], name)
plt.ylabel("Normalized Performance", fontsize=20)
#plt.ylim(0, 100)
plt.yscale('log')
plt.legend()
fig.savefig('/home/hu/Desktop/brktime.png')
plt.close(fig)



