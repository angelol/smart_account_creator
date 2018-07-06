#!/usr/bin/env python

import random

alphabet = "12345abcdefghijklmnoprstuvwxyz"

print(''.join(random.Random().choices(alphabet, weights=None, k=12)))
