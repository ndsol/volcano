#!/usr/bin/env python
# Copyright (c) 2017-2018 the Volcano Authors. Licensed under GPLv3.
# rep.py: a regexp search-and-replace tool

import errno
import os
import re
import sys

if len(sys.argv) != 5:
  if len(sys.argv) != 6 or sys.argv[1] != "-":
    print("Usage: %s input.txt output.txt pattern replacement" % sys.argv[0])
    sys.exit(1)

pattern = re.compile(sys.argv[3])
replacement = sys.argv[4]

def do_rep(line):
  m = pattern.search(line)
  if m:
    return line[:m.start()] + replacement + line[m.end():]
  return line

if len(sys.argv) == 6 and sys.argv[1] == "-":
  # Read text from sys.argv[5] instead of from a file
  print(do_rep(sys.argv[5]))
else:
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
      line = do_rep(line)
      fout.write(line)

  if count == 0:
    print("error: input was empty: %s" % sys.argv[1])
    sys.exit(1)
