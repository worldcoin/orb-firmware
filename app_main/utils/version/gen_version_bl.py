#!/usr/bin/env python
import getopt
import os
import sys
import re
from optparse import OptionParser
from subprocess import Popen, PIPE
import subprocess

def main(argv):
    opts = None

    help = "\033[33m-v <bl_version>\033[0m"
    bl_version = ''
    path_version_h = ''
    path_version_ini = ''

    try:
        opts, args = getopt.getopt(argv, "v:i:hf:", ["version="])

    except getopt.GetoptError:
        print(help)

    if opts != None:
        for opt, arg in opts:
            if opt == '-h':
                print(help)
                sys.exit()
            elif opt in ("-v", "--version"):
                bl_version = arg
                # Parse BL version to check pattern
                a = re.compile("[0-9]")
                if not a.match(bl_version):
                    print("\033[31mERROR: Version must match the pattern]\033[0m")
                    sys.exit(0)
            elif opt in ("-i"):
                path_version_h = arg
            elif opt in ("-f"):
                path_version_ini = arg

    path, fl = os.path.split(os.path.realpath(__file__))
    script_path = os.path.join(path, '')

    if path_version_ini == '':
        print("ERROR: version.ini path is missing")
        sys.exit(0)
    else:
        ini_file = open(path_version_ini, 'r')

    # Read major, minor, patch and bl versions from version.ini file
    ini_file.readline()
    firmware_major = str(int(ini_file.readline()[6:]))
    firmware_minor = str(int(ini_file.readline()[6:]))
    firmware_patch = str(int(ini_file.readline()[6:]))
    bl_version_ini = '' # Read BL version from ini if we don't want to change version
    if bl_version != '':
        bl_version_ini = str(int(ini_file.readline()[3:]))
    else:
        bl_version = str(int(ini_file.readline()[3:]))
    hw_rev = str(int(ini_file.readline()[3:]))
    ini_file.close()

    # If there is a change on version.ini to be made:
    if bl_version_ini == '':
        #write major, minor, patch and bl version to version.ini
        ini_file = open(path_version_ini, 'w')
        ini_file.writelines("[version]" + "\nmajor=" + firmware_major + "\nminor=" + firmware_minor + "\npatch=" + firmware_patch + "\nbl=" + bl_version + "\nhw=" + hw_rev)
        ini_file.close()

    # Choose the version.h path if passed in argument
    if path_version_h == '':
        print("ERROR: version.h path is missing")
    else:
        header_file = open(path_version_h, 'w')

    # Update version.h
    header_file.seek(0)

    header_file.write( "#ifndef __VERSION_H__\n")
    header_file.write( "#define __VERSION_H__\n")
    header_file.write( "\n")
    header_file.write( "/* clang-format off */")
    header_file.write( "\n")
    header_file.write( "#define BL_REV " + bl_version + "\n")
    header_file.write( "\n")
    header_file.write( "/* clang-format on */")
    header_file.write( "\n")
    header_file.write( "#endif // __VERSION_H\n")

    print("\033[32mVersion set to {}.\033[0m".format(bl_version))
    print("\033[33mWARN: Please compile the sources from scratch.\033[0m")

if __name__ == '__main__':
    main(sys.argv[1:])
