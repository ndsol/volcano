#!/usr/bin/env python
# Copyright (c) 2017-2018 the Volcano Authors. Licensed under GPLv3.
# volcano-header.py: parse the vulkan spec and generate headers.

import argparse
import os
import re
import sys
import xml.etree.ElementTree

class HeaderGen:
  def __init__(self, reg, outputfilename):
    self.reg = reg
    self.outputfilename = outputfilename
    self.err = None
    self.gen = None

  def __enter__(self):
    return self # __enter__ is called by 'with' statement.

  def __exit__(self, exc_type, exc_value, traceback):
    # __exit__ is called when 'with' statement ends
    if self.err:
      raise self.err

  def generate(self, gen, genOpts):
    if False:
      self.reg.dumpReg()
      return 0
    self.reg.setGenerator(gen)

    maxApiK = None
    for k in self.reg.apidict:
      feat = self.reg.apidict[k]
      if feat.elem.get('api') != 'vulkan':
        next
      if maxApiK and feat.name <= self.reg.apidict[maxApiK].name:
        next
      maxApiK = k

    genOpts.apiname = 'vulkan'
    genOpts.versions = '.'
    genOpts.emitversions = '.'
    genOpts.addExtensions = '.'
    genOpts.emitExtensions = '.'
    s = os.path.split(self.outputfilename)
    genOpts.directory = s[0]
    genOpts.filename = s[1]
    self.reg.apiGen(genOpts)
    self.reg.apiReset()
    return 0

def writeAutostype(h):
  class OneType:
    def __init__(self, name, stype):
      self.name = name
      self.stype = stype
      self.pre = ''
      self.post = ''

    def tostring(self):
      s = self.pre
      s += 'inline VkStructureType autoSType(const %s&) {\n' % self.name
      s += '  return %s;\n' % self.stype
      s += '}\n'
      s += self.post
      return s

  from generator import GeneratorOptions, OutputGenerator
  class AutostypeOpts(GeneratorOptions):
    def __init__(self):
      super().__init__()

  class Autostype(OutputGenerator):
    def __init__(self, reg):
      #super().__init__(sys.stdout, sys.stdout, sys.stdout)
      super().__init__(None, None, None)
      self.typemap = {}
      self.reg = reg
      self.apiname = None
      self.findSType = re.compile(r'VK_STRUCTURE_TYPE_\w+')

    def beginFile(self, genOpts):
      super().beginFile(genOpts)
      self.apiname = genOpts.apiname
      self.outFile.write('''/* Copyright (c) 2017-2018 the Volcano Authors.
 * Licensed under the GPLv3.
 * THIS FILE IS AUTO-GENERATED. DO NOT EDIT.
 * The volcano-header.py script will overwrite any changes.
 */
#pragma once
#ifdef __cplusplus
''')

    def genType(self, typeinfo, name, alias):
      super().genType(typeinfo, name, alias)
      if alias:
        next # Skip 'typedef $name $alias' because $alias has no stype.
      cat = typeinfo.elem.get('category')
      if not cat or cat not in ('struct'):
        next
      memberTypes = typeinfo.elem.findall('.//member//type')
      for mType in memberTypes:
        if mType.text == 'VkStructureType':
          res = self.findSType.search(
              xml.etree.ElementTree.tostring(typeinfo.elem).decode('ascii'))
          if res:
            self.typemap[name] = OneType(name, res.group(0))
          break

    def endFile(self):
      # Guard types defined by an extension
      for ename in self.reg.extdict:
        ext = self.reg.extdict[ename]
        extdef = None
        if ext.elem.get('supported') != self.apiname:
          next
        for e in ext.elem.findall('.//require//enum'):
          if e.get('value') == '"%s"' % ename:
            extdef = e.get('name')
            break
        if not extdef:
          next
        for req in ext.elem.findall('.//require'):
          second = req.get('extension')
          for typeinfo in req.findall('.//type'):
            name = typeinfo.get("name")
            if name in self.typemap: # non-struct types have no stype
              t = self.typemap[name]
              # Only apply a guard to a second ext if none there yet
              # Most second types just repeat the types already in 'second'.
              # A few actually require both exts and apppear only once.
              if not second or not t.pre:
                t.pre += '#ifdef %s\n' % extdef
                t.post += '#endif /*%s*/\n' % extdef
      for name in self.typemap:
        self.outFile.write(self.typemap[name].tostring())
      self.outFile.write('#endif /*__cplusplus*/\n')
      super().endFile()

  return h.generate(Autostype(h.reg), AutostypeOpts())

supported = {
  'autostype.h': writeAutostype,
}

def main(parser):
  args = parser.parse_args()
  outputs = vars(args)['output-filename']
  for outputfilename in outputs:
    base = os.path.basename(outputfilename)
    if base not in supported:
      parser.print_help()
      print('')
      print('"%s" is not a known filename' % base)
      return 1

  vk_path = os.path.split(args.registry)[0]
  try:
    sys.path.insert(0, vk_path)
    from reg import Registry
  except Exception as e:
    print('Unable to import reg from %s' % vk_path)
    print('* taken from -registry command-line value %s' % args.registry)
    print(e)
    return 1
  reg = Registry()
  try:
    tree = xml.etree.ElementTree.parse(args.registry)
  except Exception as e:
    print(e)
    return 1
  reg.loadElementTree(tree)
  tree = None
  for outputfilename in outputs:
    with HeaderGen(reg, outputfilename) as h:
      # Check if open() succeeded
      if h.err:
        print(h.err)
        h.err = None
        return 1
      # Call the function mapped to by the 'supported' dict.
      if supported[base](h):
        return 1
  return 0

if __name__ == '__main__':
  vkname = 'vk.xml'
  parser = argparse.ArgumentParser()
  hint = 'Full path to generate. Filename must be a known filename: '
  hintPrefix = '[ '
  for n in supported:
    hint = hint + ('%s%s' % (hintPrefix, n))
    hintPrefix = '| '
  parser.add_argument('output-filename', nargs='+', help=hint + ' ]')
  parser.add_argument('-registry', action='store', default=vkname,
                      help='Use a different registry instead of vk.xml.')
  sys.exit(main(parser))
