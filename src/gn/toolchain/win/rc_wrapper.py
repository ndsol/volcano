#!/usr/bin/env python
# Copyright (c) 2017 the Volcano Authors. All rights reserved.
# Licensed under the GPLv3.
import subprocess
import sys
import os

msvc_deps_prefix = 'Note: including file: '

def get_env(arch):
  """Gets the saved environment from a file for a given architecture."""
  # The environment is saved as an "environment block" (see CreateProcess
  # and msvs_emulation for details). We convert to a dict here.
  # Drop last 2 NULs, one for list terminator, one for trailing vs. separator.
  pairs = open(arch).read()[:-2].split('\0')
  kvs = [item.split('=', 1) for item in pairs]
  return dict(kvs)

def call(*popenargs, **kwargs):
  process = subprocess.Popen(stdout=subprocess.PIPE, universal_newlines=True,
                             *popenargs, **kwargs)
  output, unused_err = process.communicate()
  return process.poll(), output

def main(arch, source, output, rc_name, *args):
  """Output header dependencies and filter logo banner from invocations
  of rc.exe. Older versions of RC don't support the /nologo flag."""
  env = get_env(arch)
  args = list(args)

  output_dir = os.path.split(output)[0]
  source_name = os.path.split(source)[1]

  # Try fixing all those relative include directories.
  def fix_dirs(arg):
    if not arg.startswith('-I'):
      return arg

    return '/I{}'.format(os.path.relpath(arg[2:], output_dir))

  cl_args = list(map(fix_dirs, args))

  # This needs shell=True to search the path in env for the cl executable.
  retcode, out = call(["cl.exe", "/nologo", "/showIncludes", "/wd4005"] +
                       cl_args + ["/P", os.path.relpath(source, output_dir)],
                      env=env,
                      stderr=subprocess.STDOUT,
                      shell=True,
                      # This is necessary so our .i file (generated by /P)
                      # doesn't get written to the root build directory.
                      cwd=output_dir)
  if retcode:
    print(out)
    return retcode

  # Now we need to fix the relative paths of our included files
  for line in out.splitlines():
    if not line or line.strip() == source_name:
      continue

    if not line.startswith(msvc_deps_prefix):
      print(line)
      continue

    filename = line[len(msvc_deps_prefix):].strip()
    filename = os.path.normpath(os.path.join(output_dir, filename))

    print('{}{}'.format(msvc_deps_prefix, filename))

  retcode, out = call([rc_name] + args + ["/fo" + output, source],
                      shell=True, env=env, stderr=subprocess.STDOUT)
  for line in out.splitlines():
    if (not line.startswith('Microsoft (R) Windows (R) Resource Compiler') and
        not line.startswith('Copyright (C) Microsoft Corporation') and
        line):
      print(line)
  return retcode

if __name__ == '__main__':
  sys.exit(main(*sys.argv[1:]))
