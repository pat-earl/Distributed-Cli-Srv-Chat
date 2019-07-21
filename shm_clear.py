#!/usr/bin/python3

# Credit to Vinnie
import os
import getpass

user = getpass.getuser()

# rm shms
l = os.popen('ipcs -m').readlines()

for line in range(3, len(l)):
    if l[line] != '0' and l[line] != '\n':
        l[line] = l[line].split()
        if l[line][2] == user:
            os.popen('ipcrm -m ' + l[line][1])

# rm sems
l = os.popen('ipcs -s').readlines()

for line in range(3, len(l)):
    if l[line] != '0' and l[line] != '\n':
        l[line] = l[line].split()
        if l[line][2] == user:
            os.popen('ipcrm -s ' +  l[line][1])
