import os
import sys

rawtag = "_raw_html.txt"
riotag = "_rio_warning_html.txt"

client = [ i*10 for i in range(101) ]
rawmemavg = []
riomemavg = []


for e in os.listdir("./"):
    if e == sys.argv[1]+rawtag:
        with open(e, 'r') as fd:
            while fd.readline()[:7] != "Clients":
                continue
            fd.readline()
            for i in range(101):
                rawmemavg.append(float(fd.readline().split(',')[6].strip()))
    if e == sys.argv[1]+riotag:
        with open(e, 'r') as fd:
            while fd.readline()[:7] != "Clients":
                continue
            fd.readline()
            for i in range(101):
                riomemavg.append(float(fd.readline().split(',')[6].strip()))

import matplotlib.pyplot as plt

fig, ax = plt.subplots()

plt.plot(client, rawmemavg, 'b*')
plt.plot(client, rawmemavg, 'r')
plt.plot(client, riomemavg, 'g+')
plt.plot(client, riomemavg, 'b')

plt.xlabel("Concurrent Clients", fontsize=20)
plt.ylabel("Memory Usage (MB)", fontsize=20)

plt.ylim(0, 100)

plt.legend()
fig.savefig('/home/hu/Desktop/mem_' + sys.argv[1] + '.png')
plt.close(fig)
#plt.show()

