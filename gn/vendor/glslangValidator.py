#!/usr/bin/env python
# Copyright (c) 2017 the Volcano Authors. Licensed under GPLv3.
# glslangValidator compiles GLSL shader source into SPIR-V binary
import os
import os.path
import re
import subprocess
import sys

def runCoproc(cmd, args):
	args = [ os.path.join(os.getcwd(), cmd) ] + args
	coproc = subprocess.Popen(args,
		stdout=subprocess.PIPE,
		stderr=subprocess.STDOUT)

	while True:
		line = coproc.stdout.readline()
		line = line.splitlines()
		if len(line) < 1 or line[0] == '':
			return
		line = line[0]
		# filter out annoying warning
		if b"version 450 is not yet complete" in line:
			continue
		# do not print filename again (ninja already does it)
		if line == args[-1]:
			continue
		print(line)

def copyHeader(args, struct_file):
	target = re.sub("\.spv$", ".h", struct_file.replace("struct_", ""))
	for i in range(1, len(args)):
		if args[-i] == target:
			args[-i] = struct_file
			runCoproc("copyHeader", args)
			return
	print("unable to generate struct_file, \"%s\" not found:" % target)
	print(args)
	sys.exit(1)

args = []
for a in sys.argv[1:]:
	if a[0:4] == "spv_":
		# sanitize identifier so it is valid in C++
		# TODO: unicode
		if len(a) > 4:
			a = a[0:4] + re.sub(r"[^A-Za-z_]", "_", a[4]) + a[5:]
		if len(a) > 5:
			a = a[0:5] + re.sub("[^0-9A-Za-z_]", "_", a[5:])
	args.append(a)

struct_file = args.pop()
runCoproc("glslangValidator", args)
copyHeader(args, struct_file)
