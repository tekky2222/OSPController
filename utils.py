#! /usr/bin/env python
import os, sys, subprocess

SKETCH_VERSION_CPP = os.path.join("OSPController", "version.cpp")

def shellCmd(cmd):
  return subprocess.check_output(cmd.split(' ')).strip().decode("utf-8")

def getDescribe():
  return shellCmd("git describe --long --tags --always --dirty")
def getGitDate():
  return shellCmd("git log -1 --date=format:%Y%m%d --format=%ad")
def getVersion():
  try:
    return getDescribe().replace("-dirty", ".d") + "-" + str(getGitDate())
  except Exception as e:
    return os.path.basename(os.getcwd())

def writeVersionCpp(version):
  content = (
    '#include "version.h"\n\n'
    'const char* GIT_VERSION = "' + version.replace('\\', '\\\\').replace('"', '\\"') + '";\n'
  )
  os.makedirs(os.path.dirname(SKETCH_VERSION_CPP), exist_ok=True)
  with open(SKETCH_VERSION_CPP, 'w', encoding='utf-8') as ofile:
    ofile.write(content)
  return SKETCH_VERSION_CPP

def prettyPrint():
  try: #optional colorful output
    from colorama import Fore, Back, Style
    print(Back.YELLOW + Fore.BLACK + " git version " + Back.BLACK + Fore.YELLOW + " " + getVersion() + " " + Style.RESET_ALL)
  except Exception:
    print("git version " + getVersion())


arg = sys.argv[1] if len(sys.argv) > 1 else ""

if arg == "version":
  prettyPrint()
elif arg == "simple":
  print(getVersion())

else:
  prettyPrint()
  version = getVersion()
  path = writeVersionCpp(version)
  print(" - version written to " + path)

  try:  # if running inside platformio (sketch already compiles version.cpp)
    Import("env")
  except NameError:
    pass
