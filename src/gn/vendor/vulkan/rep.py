#!/usr/bin/env python
# Copyright (c) 2017-2018 the Volcano Authors. Licensed under GPLv3.
# rep.py: a regexp search-and-replace tool

import errno
import os
import re
import sys

if len(sys.argv) != 5:
  print("Usage: %s input.txt output.txt pattern replacement" % sys.argv[0])
  sys.exit(1)

pattern = re.compile(sys.argv[3])
replacement = sys.argv[4]
out = os.path.normpath(sys.argv[2])
try:
  os.makedirs(os.path.dirname(out))
except OSError as e:
  if e.errno != errno.EEXIST:
    raise

count = 0
with open(sys.argv[1], "r") as fin, open(out, "w") as fout:
  for line in fin:
    count = 1
    if pattern.search(line) is None:
      fout.write(line)
    else:
      fout.write(replacement + "\n")

if count == 0:
  print("error: input was empty: %s" % sys.argv[1])
  sys.exit(1)
