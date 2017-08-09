#!/usr/bin/env python3

import sys
import math

POINTS = 4096
BITS = 16
LIMIT = 2 ** (BITS - 1)

for i in range(POINTS + 1):
    value = int(round(math.cos(i * (math.pi / 2) / POINTS) * LIMIT))
    if value == LIMIT:
        value -= 1

    sys.stdout.write("{:5d}".format(value))

    if i != POINTS:
        sys.stdout.write(",")

    if (i % 8 == 7) or (i == POINTS):
        sys.stdout.write("\n")
    else:
        sys.stdout.write(" ")
