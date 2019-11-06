# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Copyright (c) 2017-2018 the Volcano Authors. All rights reserved.
# Licensed under the GPLv3.

"""Helper functions for gcc_toolchain.gni wrappers."""

import os
import re
import subprocess
from select import select
import shlex
import sys

_BAT_PREFIX = 'cmd /c call '
_WHITELIST_RE = re.compile('whitelisted_resource_(?P<resource_id>[0-9]+)')


def CommandToRun(command):
  """Generates commands compatible with Windows.

  When running on a Windows host and using a toolchain whose tools are
  actually wrapper scripts (i.e. .bat files on Windows) rather than binary
  executables, the |command| to run has to be prefixed with this magic.
  The GN toolchain definitions take care of that for when GN/Ninja is
  running the tool directly.  When that command is passed in to this
  script, it appears as a unitary string but needs to be split up so that
  just 'cmd' is the actual command given to Python's subprocess module.

  Args:
    command: List containing the UNIX style |command|.

  Returns:
    A list containing the Windows version of the |command|.
  """
  if command[0].startswith(_BAT_PREFIX):
    command = command[0].split(None, 3) + command[1:]
  return command


def ResolveRspLinks(inputs):
  """Return a list of files contained in a response file.

  Args:
    inputs: A command containing rsp files.

  Returns:
    A set containing the rsp file content."""
  rspfiles = [a[1:] for a in inputs if a.startswith('@')]
  resolved = set()
  for rspfile in rspfiles:
    with open(rspfile, 'r') as f:
      resolved.update(shlex.split(f.read()))

  return resolved


def CombineResourceWhitelists(whitelist_candidates, outfile):
  """Combines all whitelists for a resource file into a single whitelist.

  Args:
    whitelist_candidates: List of paths to rsp files containing all targets.
    outfile: Path to save the combined whitelist.
  """
  whitelists = ('%s.whitelist' % candidate for candidate in whitelist_candidates
                if os.path.exists('%s.whitelist' % candidate))

  resources = set()
  for whitelist in whitelists:
    with open(whitelist, 'r') as f:
      resources.update(f.readlines())

  with open(outfile, 'w') as f:
    f.writelines(resources)


def ExtractResourceIdsFromPragmaWarnings(text):
  """Returns set of resource IDs that are inside unknown pragma warnings.

  Args:
    text: The text that will be scanned for unknown pragma warnings.

  Returns:
    A set containing integers representing resource IDs.
  """
  used_resources = set()
  lines = text.splitlines()
  for ln in lines:
    match = _WHITELIST_RE.search(ln)
    if match:
      resource_id = int(match.group('resource_id'))
      used_resources.add(resource_id)

  return used_resources



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


class runcmd_return_stderr(runcmd):
  def __init__(self, env, max_out_len):
    if sys.platform == "darwin":
      # see https://stackoverflow.com/questions/46402772
      env["JAVA_OPTS"] = "-XX:+IgnoreUnrecognizedVMOptions --add-modules java.se.ee"
    super(runcmd_return_stdout, self).__init__(env)
    self.max_out_len = max_out_len
    self.result = b""
    self.also_stderr = False

  def handle_output(self, is_stdout, out, data):
    if is_stdout:
      return
    self.result += data
    if len(self.result) > self.max_out_len:
      self.p.kill()

  def run(self, args):
    super(runcmd_return_stdout, self).run(args)
    if len(self.result) > self.max_out_len:
      print("run(%s) got more than %d bytes" % (args, self.max_out_len))
      sys.exit(1)
    return (self.result
                .decode('utf8', 'ignore')
                .encode(sys.stdout.encoding, 'replace')
                .decode(sys.stdout.encoding))

def CaptureCommandStderr(command):
  """Returns the stderr of a command.
  """
  r = runcmd_return_stderr(os.environ.copy(), 4096)
  stderr = r.run(command)
  return r.p.returncode, stderr
