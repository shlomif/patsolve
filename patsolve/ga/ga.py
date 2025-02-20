#! /usr/bin/env python3

import os
import re
import string
import sys
import time

import rng
from rng import torat

from six.moves import range


def printusage():
    print("[-c crossprob] [-m mutateprob] [-n Npop] [-l #games] [pop]")


crossprob = .1
mutateprob = .1
Npop = 100
Start = 0
Len = 20
Gen = 0
Opts = '-skS -M20'
Mem = 10 * 1000 * 1000


def parseargs():
    # TODO(parseargs): implement
    return ([], [])


optlist, args = parseargs("c:m:n:l:")

for opt, arg in optlist:
    if opt == '-c':
        crossprob = float(arg)
    elif opt == '-m':
        mutateprob = float(arg)
    elif opt == '-n':
        Npop = int(arg)
    elif opt == '-l':
        Len = int(arg)

if len(args) > 1:
    printusage()
    sys.exit(1)

seed = int((time.time() * 10000.) % (1 << 30))
rng.seed(seed)


def mutate(lst, p):
    """mutate(list, probability) -> list

    Given a list of integers and a mutation probability, mutate the list
    by either adding or subtracting from some of the elements.  The
    mutated list is returned.
    """

    (p, q) = torat(p)
    m = [1] * 24 + [2] * 12 + [3] * 6 + [4] * 3 + [5] * 2 + [6] * 1

    def flip(x, p=p, q=q, m=m):
        if rng.flip(p, q):
            y = m[rng.random() % len(m)]
            if rng.flip(1, 2):
                return x + y
            return x - y
        return x

    return map(flip, lst)


def cross(l1, l2, p):
    """cross(list, list, probability) -> list

    Take elements from one list until a crossover is made, then take
    elements from the other list, and so on, with the given probability
    of a crossover at each position.  The initial list is chosen at
    random from one of the two lists with equal probability.  The lists
    must be the same length.
    """

    lst = [0] * len(l1)
    x = map(None, l1, l2)
    j = rng.random() % 2
    (p, q) = torat(p)
    for i in range(len(l1)):
        if rng.flip(p, q):
            j = 1 - j
        lst[i] = x[i][j]

    return lst


def breed(l1, l2):
    """breed(list, list) -> list

    Given two individuals, cross them and mutate the result.
    """

    return mutate(cross(l1, l2, crossprob), mutateprob)


def initpop(nl=None):
    """initpop

    The initial population is a list of length Npop of lists of
    length Nparam.
    """

    global Npop, Nparam

    if nl:
        lst = []
        with open(nl[0], 'r') as f:
            for s in f.readlines():
                if s[0] != '#':
                    m = map(int, string.split(s))
                    lst.append(m)
        Npop = len(lst)
        Nparam = len(lst[0])
    else:
        # oldcanon = [5, 4, 6, 0, 0, 0, 0, 0, 0, 0, 1, 3, 0, 1]
        # canon0 = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
        canon1 = [2, 6, 2, 0, -5, -9, -5, -11, 3, 1, -5, 2, 2, 0]
        canon2 = [1, 1, 6, -2, -1, -2, -2, -3, 0, -1, 2, 4, 6, 1]
        Nparam = len(canon1)
        lst = [breed(canon1, canon2) for x in range(Npop)]
#                lst = [mutate(canon0, .9) for x in range(Npop)]

    return lst


def newpop(result):
    """newpop

    The result of a generation is a list of pairs, where each
    pair consists of the fitness value and the individual that
    generated it.  Construct a new population (formatted as a
    list of lists) using the following steps:
        1. Sort by the fitness value (smallest first).
        2. Replace the last half of the sorted population with
           new individuals bred from pairs chosen at random
           from the first half.
    The new population is returned.
    """

    result.sort()
    print('%d: %g, %s' % (Gen, result[0][0], result[0][1]))
    pop = [0] * Npop
    cut = int(Npop * .5)
    for i in range(cut):
        pop[i] = result[i][1]
    for i in range(cut, Npop):
        a = rng.random() % cut
        b = rng.random() % cut
        pop[i] = breed(result[a][1], result[b][1])

    return pop


def sgn(x):
    if x < 0:
        return -1
    return 1


def refill(pop):
    """Get rid of duplicates."""

    for lst in pop:
        lst[9] = sgn(lst[9])
        lst[-1] = abs(lst[-1])
    pop.sort()

    newpop = [0] * Npop
    i = 0
    last = None
    for lst in pop:
        if lst != last:
            newpop[i] = lst
            i += 1
        last = lst
    for j in range(i, Npop):
        a = rng.random() % i
        b = rng.random() % i
        newpop[j] = breed(newpop[a], newpop[b])

    return newpop


def run(pop):
    """run

    Test each member of the population and save it with its
    fitness value.
    """

    result = [0] * Npop
    i = 0
    for lst in pop:
        result[i] = (fitness(lst), lst)
        i += 1

    return result


def get_ycoeff(lst):
    """get_ycoeff

    Find the quadratic through (0, lst[0]), (25, lst[1]), and (50, lst[2]).
    Return the coefficients as a string, e.g., '.5 1.2 2.'
    """

    with open('/tmp/x', 'w') as f:
        f.write('0 %d\n' % lst[0])
        f.write('25 %d\n' % lst[1])
        f.write('50 %d\n' % lst[2])
    os.system('fit/dofit /tmp/x > /tmp/coeff')
    with open('/tmp/coeff', 'r') as infh:
        lst = infh.readlines()
    return string.join(map(str, map(float, lst)))


move = re.compile(r"([0-9]+) moves.")
pos = re.compile(r"([0-9]+) positions generated.")
upos = re.compile(r"([0-9]+) unique positions.")
memrem = re.compile(r"Mem_remain = ([0-9]+)")
malloc = re.compile(r".*memory.*")


def fitness(lst):
    """Run the individual and return the fitness value."""

    nx = Nparam - 4
    param = '-c%d ' % (abs(lst[-1]) + 1)
    param += '-X%d %s ' % (nx, string.join(map(str, lst[:nx])))
    s = get_ycoeff(lst[-4:-1])
    param += '-Y %s' % s
    resname = 'res'

    cmd = '/home/tomh/src/patsolve/xpatsolve %s -N%d %d %s > %s'
    cmd %= (Opts, Start, Start + Len, param, resname)

    print(('running %s ') % param),
    sys.stdout.flush()
    t0 = os.times()[2]
    os.system(cmd)
    t1 = os.times()[2]

    with open(resname, 'r') as f:
        mysum = 0
        n = 0
        mem = 0
        for s in f.xreadlines():
            x = move.findall(s)
            # x = pos.findall(s)
            # x = memrem.findall(s)
            if x:
                m = int(x[-1])
                mysum += m
                n += 1
            elif malloc.match(s):
                mysum += 1000
                mem += 1
                n += 1

    if n == 0:
        print('no result')
        return 1000000
#        fit = Mem - float(mysum) / n
#        fit = float(mysum) / n
    fit = (t1 - t0) / Len
    if mem > 0:
        print('fitness = %g, oom = %d' % (fit, mem))
    else:
        print('fitness = %g' % fit)
    return fit


def ga():
    """Run the GA over many generations."""

    global Start, Gen

    pop = initpop(args)
    while True:
        with open('curpop', 'w') as f:
            f.write("# %s\n" % Opts)
            for lst in pop:
                f.write("%s\n" % string.join(map(str, lst)))
        pop = refill(pop)
        result = run(pop)
        pop = newpop(result)
        # Start += Len
        Start = rng.random() % 1000000000
        Gen += 1


ga()
