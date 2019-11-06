#!/usr/bin/env python
# Copyright (c) 2017-2018 the Volcano Authors. All rights reserved.
# Licensed under the GPLv3.
import os
import shutil
import sys

def main(source, dest):
  """Emulation of rm -rf out && cp -af in out."""
  if os.path.exists(dest):
    if os.path.isdir(dest):
      def _on_error(fn, path, excinfo):
        # The operation failed, possibly because the file is set to
        # read-only. If that's why, make it writable and try the op again.
        if not os.access(path, os.W_OK):
          os.chmod(path, stat.S_IWRITE)
        fn(path)
      shutil.rmtree(dest, onerror=_on_error)
    else:
      if not os.access(dest, os.W_OK):
        # Attempt to make the file writable before deleting it.
        os.chmod(dest, stat.S_IWRITE)
      os.unlink(dest)

  if os.path.isdir(source):
    shutil.copytree(source, dest)
  else:
    shutil.copy2(source, dest)
    # "touch" the file (windows mtime bug in shutil.copy2).
    os.utime(dest, None)

if __name__ == '__main__':
  sys.exit(main(*sys.argv[1:]))
