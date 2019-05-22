#!/usr/bin/env python
# Copyright (c) 2017-2018 the Volcano Authors. All rights reserved.
# Licensed under the GPLv3.
import sys
import os

def main(manifest_path, resource_path, resource_name):
  """Creates a resource file pointing a SxS assembly manifest.
  |args| is tuple containing path to resource file, path to manifest file
  and resource name which can be "1" (for executables) or "2" (for DLLs)."""
  with open(resource_path, 'wb') as output:
    output.write('#include <windows.h>\n%s RT_MANIFEST "%s"' % (
      resource_name,
      os.path.abspath(manifest_path).replace('\\', '/')))

  return 0

if __name__ == '__main__':
  sys.exit(main(*sys.argv[1:]))
