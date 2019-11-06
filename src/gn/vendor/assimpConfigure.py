#!/usr/bin/env python
# Copyright (c) 2017-2018 the Volcano Authors. Licensed under GPLv3.
# Process input files that need text replacement to build assimp.

import argparse
import errno
import os
import sys

if __name__ == "__main__":
  def file_exists_validator(filename):
    if not os.path.exists(filename):
      raise argparse.ArgumentTypeError("%s not found" % filename)
    return filename

  def mkdir_validator(filename):
    try: os.makedirs(os.path.dirname(filename))
    except OSError as e:
      if e.errno != errno.EEXIST: raise
    return filename

  def assignment_validator(s):
    if len(s.split("=")) != 2:
      raise argparse.ArgumentTypeError("invalid assignment: %s" % s)
    return s

  parser = argparse.ArgumentParser(description="Translate .in files for assimp")
  parser.add_argument("-in", action="append", type=file_exists_validator)
  parser.add_argument("-out", action="append", type=mkdir_validator)
  parser.add_argument("-d", action="append", type=assignment_validator)
  args = vars(parser.parse_args())

  if args["in"] is None:
    print("Missing required arg: -in")
    sys.exit(1)
  if args["out"] is None:
    print("Missing required arg: -out")
    sys.exit(1)
  if len(args["in"]) != len(args["out"]):
    print("Every -in must have a matching -out.")
    print("Found %d -in and %d -out." % (
      len(args["in"]), len(args["out"])))
    sys.exit(1)
  if args["d"] is None:
    args["d"] = []

  for fin, fout in zip(args["in"], args["out"]):
    with open(fin, "r") as infile, open(fout, "w") as outfile:
      for line in infile:
        w = line.split(" ")
        if len(w) > 0 and w[0] == "#cmakedefine":
          if w[1] in args["d"]:
            line = "#define %s" % " ".join(w[1:])
          else:
            line = "#undef %s" % w[1]
        else:
          for kv in args["d"]:
            k = kv.split("=", 1)
            line = line.replace("@%s@" % k[0], k[1])
        outfile.write(line)
