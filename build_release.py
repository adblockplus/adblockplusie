#!/usr/bin/env python

import sys, os, re, subprocess

if len(sys.argv) < 2:
  print >>sys.stderr, "Please add a command line parameter with the path of the signing key file"
  sys.exit(1)

basedir = os.path.dirname(os.path.abspath(sys.argv[0]))
key = sys.argv[1]

def sign(*argv):
  subprocess.call([
    "signtool",
    "sign", "/v",
    "/d", "Adblock Plus",
    "/du", "http://adblockplus.org/",
    "/f", key,
    "/tr", "http://www.startssl.com/timestamp"
  ] + list(argv))

def read_macro_value(file, macro):
  handle = open(file, 'rb')
  for line in handle:
    match = re.search(r"^\s*#define\s+%s\s+\w?\"(.*?)\"" % macro, line)
    if match:
      return match.group(1)
  raise Exception("Macro %s not found in file %s" % (macro, file))

version = read_macro_value(os.path.join(basedir, "src", "shared", "Version.h"), "IEPLUGIN_VERSION");
buildnum, dummy = subprocess.Popen(['hg', 'id', '-R', basedir, '-n'], stdout=subprocess.PIPE).communicate()
buildnum = re.sub(r'\D', '', buildnum)
while version.count(".") < 1:
  version += ".0"
version += ".%s" % buildnum

subprocess.call([os.path.join(basedir, "libadblockplus", "createsolution.bat")])

for arch in ("ia32", "x64"):
  platform = "/p:Platform=%s" % {"ia32": "Win32", "x64": "x64"}[arch]
  subprocess.call([
    "msbuild",
    os.path.join(basedir, "libadblockplus", "build", arch, "libadblockplus.sln"),
    "/p:Configuration=Release",
    platform
  ])

  subprocess.call([
    "msbuild",
    os.path.join(basedir, "AdblockPlus.sln"),
    "/p:Configuration=Release Test",
    platform])

  plugin = {"ia32": "AdblockPlus.dll", "x64": "AdblockPlusx64.dll"}[arch]
  sign(os.path.join(basedir, "build", arch, "Release Test", plugin),
      os.path.join(basedir, "build", arch, "Release Test", "AdblockPlusEngine.exe"))

installerParams = os.environ.copy()
installerParams["VERSION"] = version
subprocess.call(["nmake"], env=installerParams, cwd=os.path.join(basedir, "WixInstaller"))
sign(os.path.join(basedir, "build", "ia32", "adblockplusie-%s-en-us-ia32.msi" % version),
    os.path.join(basedir, "build", "x64", "adblockplusie-%s-en-us-x64.msi" % version))
