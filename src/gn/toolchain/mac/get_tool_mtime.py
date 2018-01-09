# Copyright 2016 The Chromium Authors. All rights reserved.
# Copyright (c) 2017 the Volcano Authors. All rights reserved.
# Licensed under the GPLv3.

import os
import sys

# Usage: python get_tool_mtime.py path/to/file1.py path/to/file2.py
#
# Prints a GN scope with the variable name being the basename sans-extension
# and the value being the file modification time. A variable is emitted for
# each file argument on the command line.

if __name__ == '__main__':
  for f in sys.argv[1:]:
    variable = os.path.splitext(os.path.basename(f))[0]
    print('%s = %d' % (variable, os.path.getmtime(f)))
