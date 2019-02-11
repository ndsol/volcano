#!/usr/bin/env python
# Copyright (c) 2017-2018 the Volcano Authors. Licensed under GPLv3.
# glslangValidator compiles GLSL shader source into SPIR-V binary
import errno
import os
import os.path
import re
from select import select
import subprocess
import sys
from subninja import runcmd_ignore_output, runcmd_no_output

class runcmd_filter_glslang_validator(runcmd_ignore_output):
  def filter_out_filename(self, the_string):
    self.filter_out_string = the_string
    return self

  def handle_output(self, is_stdout, out, data):
    lines = data.splitlines()
    for line in lines:
      if line == b"":
        # suppress blank lines
        continue
      # filter out annoying warning
      if b"version 450 is not yet complete" in line:
        continue
      # do not print filename (ninja already does it)
      if line.decode(sys.stdout.encoding) == self.filter_out_string:
        continue
      out.write(line.decode(sys.stdout.encoding) + "\n")
    out.flush()

def copyHeader(cenv, struct_file, args):
  target = re.sub("\.spv$", ".h", struct_file.replace("struct_", ""))
  for i in range(1, len(args)):
    if args[-i] == target:
      args[-i] = struct_file
      copy_header = os.path.join(os.getcwd(), sys.argv[1], "copyHeader")
      runcmd_no_output(cenv).run([ copy_header ] + args)
      return
  print("unable to generate struct_file, \"%s\" not found:" % target)
  print(args)
  sys.exit(1)

if __name__ == "__main__":
  cenv = os.environ.copy()
  args = []
  args_extra = []
  found_extra = False
  for a in sys.argv[2:]:
    if a[0:4] == "spv_":
      # sanitize identifier so it is valid in C++
      # TODO: sanitize unicode
      if len(a) > 4:
        a = a[0:4] + re.sub("[^0-9A-Za-z_]", "_", a[4:])
    if a == "--extra_flags":
      found_extra = True
      continue
    if found_extra:
      args_extra.append(a)
    else:
      args.append(a)

  struct_file = args.pop()
  glslang_validator = os.path.join(os.getcwd(), sys.argv[1], "glslangValidator")
  g = runcmd_filter_glslang_validator(cenv)
  g.filter_out_filename(args[-1])
  g.run([ glslang_validator ] + args_extra + args)
  copyHeader(cenv, struct_file, args)
