#!/usr/bin/env python
# Copyright (c) 2017-2018 the Volcano Authors. Licensed under GPLv3.
# pkgconfig.py: calls pkg-config

import errno
import os
from select import select
import subprocess
import sys

if len(sys.argv) < 2:
  print("Usage: %s package" % sys.argv[0])
  sys.exit(1)

if sys.platform[:5] != "linux":
  print("Usage: %s package. Other platforms should use a different tool." %
        sys.argv[0])
  sys.exit(1)

class runcmd(object):
  def __init__(self, env):
    self.have_out = False
    self.have_err = False
    self.env = env
    self.p = None

  def handle_output(self, is_stdout, out, data):
    out.write(data.decode('utf8', 'ignore')
        .encode(sys.stdout.encoding, 'replace').decode(sys.stdout.encoding))
    out.flush()

  def run(self, args):
    path_saved = os.environ["PATH"]
    if "PATH" in self.env:
      os.environ["PATH"] = self.env["PATH"]
    try:
      import pty
      masters, slaves = zip(pty.openpty(), pty.openpty())
      self.p = subprocess.Popen(args, stdout=slaves[0], stderr=slaves[1],
                                env = self.env)
      for fd in slaves: os.close(fd)
      readable = { masters[0]: sys.stdout, masters[1]: sys.stderr }
      while readable:
        for fd in select(readable, [], [])[0]:
          try: data = os.read(fd, 1024)
          except OSError as e:
            if e.errno != errno.EIO: raise
            del readable[fd]
            continue
          if not data:
            del readable[fd]
            continue
          self.handle_output(fd == masters[0], readable[fd], data)
          if fd != masters[0]:
            self.have_err = True
          else:
            self.have_out = True
      self.p.wait()
    except ImportError:
      if sys.platform != "win32":
        raise
      self.p = subprocess.Popen(args, shell=True)
      (o, e) = self.p.communicate()
      self.have_out = False
      self.have_err = False
      if o:
        self.have_out = True
        self.handle_output(True, sys.stdout, o)
      if e:
        self.have_err = True
        self.handle_output(False, sys.stdout, e)
    os.environ["PATH"] = path_saved


class runcmd_no_stderr(runcmd):
  def run(self, args):
    super(runcmd_no_stderr, self).run(args)
    if self.have_err:
      print("run(%s) spit something out on stderr" % args)
      sys.exit(1)
    if self.p.returncode != 0:
      print("run(%s) returned %d" % (args, self.p.returncode))
      sys.exit(1)

if __name__ == "__main__":
  exe = "pkg-config"
  exe_args = [ exe ] + sys.argv[1:]
  runcmd_no_stderr(os.environ.copy()).run([ exe ] + exe_args)
