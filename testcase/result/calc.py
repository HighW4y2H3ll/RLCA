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

rawavg = 0
rioavg = 0
for i in rawreqavg:
    rawavg += i

for i in rioreqavg:
    rioavg += i

print rawavg, rioavg

print (rawavg - rioavg)*1.0/rawavg

