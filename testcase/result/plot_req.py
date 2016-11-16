import os
import sys

rawtag = "_raw_html.txt"
riotag = "_rio_warning_html.txt"

client = [ i*10 for i in range(101) ]
rawreqavg = []
rioreqavg = []


for e in os.listdir("./"):
    if e == sys.argv[1]+rawtag:
        with open(e, 'r') as fd:
            while fd.readline()[:7] != "Clients":
                continue
            fd.readline()
            for i in range(101):
                rawreqavg.append(int(fd.readline().split(',')[2].strip()))
    if e == sys.argv[1]+riotag:
        with open(e, 'r') as fd:
            while fd.readline()[:7] != "Clients":
                continue
            fd.readline()
            for i in range(101):
                rioreqavg.append(int(fd.readline().split(',')[2].strip()))

import matplotlib.pyplot as plt
fig, ax = plt.subplots()

plt.plot(client, rawreqavg, 'b*')
plt.plot(client, rawreqavg, 'r')
plt.plot(client, rioreqavg, 'g+')
plt.plot(client, rioreqavg, 'b')

plt.xlabel("Concurrent Clients", fontsize=20)
plt.ylabel("Requests per second", fontsize=18)

plt.ylim(6000, 200000)

plt.legend()
fig.savefig('/home/hu/Desktop/figure_' + sys.argv[1] + '.png')
plt.close(fig)
#plt.show()

