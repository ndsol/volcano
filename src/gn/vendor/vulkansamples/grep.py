#!/usr/bin/env python
# Copyright (c) 2017 the Volcano Authors. Licensed under GPLv3.
# grep.py: filter text files

import errno
import os
import re
import sys

if len(sys.argv) != 3:
  print("Usage: %s input.txt output.txt" % sys.argv[0])
  sys.exit(1)

# It might be nice to provide the pattern on the command line.
pattern = re.compile("include.*vulkan/vulkan\.h")
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

if count == 0:
  print("error: input was empty: %s" % sys.argv[1])
  sys.exit(1)
