#!/usr/bin/env python
# Copyright (c) 2017-2018 the Volcano Authors. All rights reserved.
# Licensed under the GPLv3.
import sys
import os

def main(path):
  """Simple stamp command."""
  if os.path.exists(path):
    os.utime(path, None)
  else:
    open(path, 'w').close()

if __name__ == '__main__':
  sys.exit(main(*sys.argv[1:]))
