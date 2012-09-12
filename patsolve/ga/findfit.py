#! /usr/bin/env python

import sys, re, string
from util import *

usage("""[-n <n>] filename ...""")

N = 10

optlist, args = parseargs("n:")

for opt, arg in optlist:
    if opt == '-n':
        N = int(arg)

if len(args) < 1:
    printusage()
    sys.exit(1)

parse = re.compile(r"running (.*) fitness = ([-+0-9.e]+)")

def sgn(x):
    if x < 0: return -1
    return 1

fit = {}

for filename in args:
    f = open(filename, 'r')
    for line in f.xreadlines():
        p = parse.match(line)
        if p:
            s = string.split(p.group(1))
            s[11] = str(sgn(int(s[11])))
            s = string.join(s)
            if fit.has_key(s):
                fit[s].append(float(p.group(2)))
            else:
                fit[s] = [float(p.group(2))]
    f.close()

def add(a, b):
    return a + b

m = []

for id in fit.keys():
    l = fit[id]
    avg = reduce(add, l, 0) / float(len(l))
    m.append((avg, id, len(l)))

m.sort(lambda x, y: sgn(x[0] - y[0]))
#m.sort(lambda x, y: sgn(y[2] - x[2]))
for i in xrange(N):
    print m[i][1], 'fitness =', m[i][0], '(%d)' % m[i][2]
