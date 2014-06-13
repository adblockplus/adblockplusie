#!/usr/bin/env python

import sys, os, re, subprocess

def print_usage():
  print >>sys.stderr, "Usage: %s release|devbuild SIGNING_KEY_FILE" % (os.path.basename(sys.argv[0]))

if len(sys.argv) < 3:
  print_usage()
  sys.exit(1)

basedir = os.path.dirname(os.path.abspath(sys.argv[0]))
build_type = sys.argv[1]

if not build_type in ["release", "devbuild"]:
  print_usage()
  sys.exit(2)

key = sys.argv[2]

def sign(*argv):
  subprocess.check_call([
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

if build_type == "devbuild":
  while version.count(".") < 1:
    version += ".0"
  buildnum = subprocess.check_output(['hg', 'id', '-R', basedir, '-n'])
  buildnum = re.sub(r'\D', '', buildnum)
  version += ".%s" % buildnum

subprocess.check_call([os.path.join(basedir, "createsolution.bat"), version, build_type])

for arch in ("ia32", "x64"):
  subprocess.check_call([
    "msbuild",
    os.path.join(basedir, "build", arch, "adblockplus.sln"),
    "/p:Configuration=Release", "/target:AdblockPlus;AdblockPlusEngine",
  ])

  sign(os.path.join(basedir, "build", arch, "Release", "AdblockPlus.dll"),
      os.path.join(basedir, "build", arch, "Release", "AdblockPlusEngine.exe"))

installerParams = os.environ.copy()
installerParams["VERSION"] = version
subprocess.check_call(["nmake", "/A", "ia32", "x64"], env=installerParams, cwd=os.path.join(basedir, "installer"))
sign(os.path.join(basedir, "build", "ia32", "adblockplusie-%s-FINAL-ia32.msi" % version),
    os.path.join(basedir, "build", "x64", "adblockplusie-%s-FINAL-x64.msi" % version))

# If this fails, please check if InnoSetup is installed and added to you PATH 
subprocess.check_call(["iscc", "/A", os.path.join(basedir, "installer", "src", "innosetup-exe", "64BitTwoArch.iss"), "/Dversion=%s" % version], env=installerParams, cwd=os.path.join(basedir, "installer"))

# Do the signing dance described on http://wix.sourceforge.net/manual-wix3/insignia.htm
bundle = os.path.join(basedir, "build", "adblockplusie-%s.exe" % version)
engine = os.path.join(basedir, "build", "engine-%s.exe" % version)
subprocess.check_call(["insignia", "-ib", bundle, "-o", engine])
sign(engine)
subprocess.check_call(["insignia", "-ab", engine, bundle, "-o", bundle])
sign(bundle)
