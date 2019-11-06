#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Copyright (c) 2017-2018 the Volcano Authors. All rights reserved.
# Licensed under the GPLv3.

"""Runs 'ld -shared' and generates a .TOC file that's untouched when unchanged.

This script exists to avoid using complex shell commands in
gcc_toolchain.gni's tool("solink"), in case the host running the compiler
does not have a POSIX-like shell (e.g. Windows).
"""

import argparse
import os
import subprocess
import sys

import wrapper_utils


def CollectSONAME(args):
  """Replaces: readelf -d $sofile | grep SONAME"""
  toc = ''
  readelf = subprocess.Popen(wrapper_utils.CommandToRun(
      [args.readelf, '-d', args.sofile]), stdout=subprocess.PIPE,
      bufsize=-1, universal_newlines=True)
  for line in readelf.stdout:
    if 'SONAME' in line:
      toc += line
  return readelf.wait(), toc


def CollectDynSym(args):
  """Replaces: nm --format=posix -g -D $sofile | cut -f1-2 -d' '"""
  toc = ''
  nm = subprocess.Popen(wrapper_utils.CommandToRun([
      args.nm, '--format=posix', '-g', '-D', args.sofile]),
      stdout=subprocess.PIPE, bufsize=-1, universal_newlines=True)
  for line in nm.stdout:
    toc += ' '.join(line.split(' ', 2)[:2]) + '\n'
  return nm.wait(), toc


def CollectTOC(args):
  result, toc = CollectSONAME(args)
  if result == 0:
    result, dynsym = CollectDynSym(args)
    toc += dynsym
  return result, toc


def UpdateTOC(tocfile, toc):
  if os.path.exists(tocfile):
    old_toc = open(tocfile, 'r').read()
  else:
    old_toc = None
  if toc != old_toc:
    open(tocfile, 'w').write(toc)


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--readelf',
                      required=True,
                      help='The readelf binary to run',
                      metavar='PATH')
  parser.add_argument('--nm',
                      required=True,
                      help='The nm binary to run',
                      metavar='PATH')
  parser.add_argument('--strip',
                      help='The strip binary to run',
                      metavar='PATH')
  parser.add_argument('--sofile',
                      required=True,
                      help='Shared object file produced by linking command',
                      metavar='FILE')
  parser.add_argument('--tocfile',
                      required=True,
                      help='Output table-of-contents file',
                      metavar='FILE')
  parser.add_argument('--output',
                      required=True,
                      help='Final output shared object file',
                      metavar='FILE')
  parser.add_argument('--resource-whitelist',
                      help='Merge all resource whitelists into a single file.',
                      metavar='PATH')
  parser.add_argument('command', nargs='+',
                      help='Linking command')
  args = parser.parse_args()

  # Work-around for gold being slow-by-default. http://crbug.com/632230
  fast_env = dict(os.environ)
  fast_env['LC_ALL'] = 'C'

  if args.resource_whitelist:
    whitelist_candidates = wrapper_utils.ResolveRspLinks(args.command)
    wrapper_utils.CombineResourceWhitelists(
        whitelist_candidates, args.resource_whitelist)

  resdef = '-DVOLCANO_ADD_TO_APP="'
  # Filter out all resdef from args.command
  command = [arg for arg in args.command if not arg.startswith(resdef)]
  resdir = [arg[len(resdef):-1]
            for arg in args.command if arg.startswith(resdef)]
  if resdir: # Do not create .add.txt unless list is nonempty
    resdict = {}
    for d in resdir:
      resdict[d] = 1
    with open('%s.add.txt' % args.output, 'w') as addfile:
      for d in resdict:
        addfile.write(d + '\n')

  # run the actual link.
  result = subprocess.call(
      wrapper_utils.CommandToRun(command), env=fast_env)
  if result != 0:
    print('dumping RSP file:')
    with open(args.output + '.rsp') as fp:
      for line in fp:
        for w in line.split(' '):
          print(w)
    return result

  # Next, generate the contents of the TOC file.
  result, toc = CollectTOC(args)
  if result != 0:
    return result

  # If there is an existing TOC file with identical contents, leave it alone.
  # Otherwise, write out the TOC file.
  UpdateTOC(args.tocfile, toc)

  # Finally, strip the linked shared object file (if desired).
  if args.strip:
    result = subprocess.call(wrapper_utils.CommandToRun(
        [args.strip, '--strip-unneeded', '-o', args.output, args.sofile]))

  return result


if __name__ == '__main__':
  sys.exit(main())
