# Copyright 2016 The Chromium Authors. All rights reserved.
# Copyright (c) 2017-2018 the Volcano Authors. All rights reserved.
# Licensed under the GPLv3.

import os
import re
import subprocess
import sys

# This script executes libool and filters out logspam lines like:
#    '/path/to/libtool: file: foo.o has no symbols'

def Main(cmd_list):
  libtool_re = re.compile(b'^.*libtool: (?:for architecture: \S* )?'
                          b'file: .* has no symbols$')
  libtool_re5 = re.compile(
      b'^.*libtool: warning for library: ' +
      b'.* the table of contents is empty ' +
      b'\(no object file members in the library define global symbols\)$')
  env = os.environ.copy()
  # Ref:
  # http://www.opensource.apple.com/source/cctools/cctools-809/misc/libtool.c
  # The problem with this flag is that it resets the file mtime on the file to
  # epoch=0, e.g. 1970-1-1 or 1969-12-31 depending on timezone.
  env['ZERO_AR_DATE'] = '1'
  libtoolout = subprocess.Popen(cmd_list, stderr=subprocess.PIPE, env=env)
  _, err = libtoolout.communicate()
  for line in err.splitlines():
    if not libtool_re.match(line) and not libtool_re5.match(line):
      print(line, file=sys.stderr)
  # Unconditionally touch the output .a file on the command line if present
  # and the command succeeded. A bit hacky.
  if not libtoolout.returncode:
    for i in range(len(cmd_list) - 1):
      if cmd_list[i] == '-o' and cmd_list[i+1].endswith('.a'):
        os.utime(cmd_list[i+1], None)
        break
  return libtoolout.returncode


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
