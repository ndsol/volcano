#!/usr/bin/env python
# Copyright (c) 2017-2018 the Volcano Authors. All rights reserved.
# Licensed under the GPLv3.

import os
import sys

if __name__ == '__main__':
  if len(sys.argv) < 2:
    sys.stderr.write('Usage: %s $ANDROID_NDK_HOME' % sys.argv[0])
    sys.exit(1)
  with open(os.path.join(sys.argv[1], "source.properties")) as pfile:
    for line in pfile:
      k, v = line.split("=", 1)
      if k.strip() == "Pkg.Revision":
        # v.strip() will look something like:
        # "15.2.4203891"
        ver = v.strip().split(".")
        print(ver[0])
        break
