#! /usr/bin/env python3

import argparse
from functools import reduce
import os
import re
import string
import sys

from six.moves import range


def printusage():
    print("""[-n <n>] filename ...""")


if os.getenv('USAGE'):
    printusage()
    sys.exit(0)

N = 10

parser = argparse.ArgumentParser()
parser.add_argument('-n', help='n')
parser.add_argument('files', nargs='*')
args = parser.parse_args()

if args.n is not None:
    N = int(args.n)

if len(args.files) < 1:
    printusage()
    sys.exit(1)

parse = re.compile(r"running (.*) fitness = ([-+0-9.e]+)")


def sgn(x):
    if x < 0:
        return -1
    return 1


fit = {}

for filename in args.files:
    with open(filename, 'r') as f:
        for line in f.xreadlines():
            p = parse.match(line)
            if p:
                s = string.split(p.group(1))
                s[11] = str(sgn(int(s[11])))
                s = string.join(s)
                if s in fit:
                    fit[s].append(float(p.group(2)))
                else:
                    fit[s] = [float(p.group(2))]


def add(a, b):
    return a + b


m = []

for id in fit.keys():
    lst = fit[id]
    avg = reduce(add, lst, 0) / float(len(lst))
    m.append((avg, id, len(lst)))

m.sort(lambda x, y: sgn(x[0] - y[0]))
# m.sort(lambda x, y: sgn(y[2] - x[2]))
for i in range(N):
    print(m[i][1], 'fitness =', m[i][0], '(%d)' % m[i][2])
