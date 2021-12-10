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
    help = "\033[33mHelp:\npython gen_version.py -i path/to/version.h [-f path/to/version.ini | -v <major.minor.patch>] [-m] [-p]\n-f -v\teither provide ini [f]ile or manually enter the [v]ersion\n-m\tincrement minor\n-p\tincrement patch\033[0m"
    app_version = ''
    path_version_h = ''
    path_version_ini = ''
    increment_patch = False
    increment_minor = False
    firmware_major = ''
    firmware_minor = ''
    firmware_patch = ''

    try:
        opts, args = getopt.getopt(argv, "v:i:hmpf:", ["version="])

    except getopt.GetoptError:
        print(help)

    if opts != None:
        for opt, arg in opts:
            if opt == '-h':
                print(help)
                sys.exit()
            elif opt in ("-v", "--version"):
                app_version = arg
            elif opt in ("-i"):
                path_version_h = arg
            elif opt in ("-f"):
                path_version_ini = arg
            elif opt == '-m':
                increment_minor = True
            elif opt == '-p':
                increment_patch = True

    path, fl = os.path.split(os.path.realpath(__file__))
    script_path = os.path.join(path, '')

    if path_version_ini == '':
        print("ERROR: version.ini path is missing")
        sys.exit(0)
    else:
        ini_file = open(path_version_ini, 'r')

    #get major, minor, patch and bl versions from version.ini
    ini_file.readline()
    firmware_major = str(int(ini_file.readline()[6:]))
    firmware_minor = str(int(ini_file.readline()[6:]))
    firmware_patch = str(int(ini_file.readline()[6:]))
    bl_version = str(int(ini_file.readline()[3:]))
    hw_rev = str(int(ini_file.readline()[3:]))
    ini_file.close()

    if app_version == '':
        if increment_minor:
            firmware_minor = str(int(firmware_minor) + 1 )
            firmware_patch = "0"

        if increment_patch:
            firmware_patch = str(int(firmware_patch) + 1 )

        if not increment_minor and not increment_patch:
            print("Provided .ini file has been left untouched")

    else:
        # Parse app version
        a = re.compile("[0-9].[0-9].[0-9]")
        if a.match(app_version):
            app_version_list = app_version.split('.')
            firmware_patch = app_version_list.pop()
            firmware_minor = app_version_list.pop()
            firmware_major = app_version_list.pop()
        else:
            print("ERROR: Version must match the pattern major.minor.patch")
            sys.exit(0)

    if increment_minor or increment_patch:
        ini_file = open(path_version_ini, 'w')
        print(
            "Writing version into {}: {}.{}.{} / bl:{} / hw:{}".format(path_version_ini, firmware_major, firmware_minor,
                                                                       firmware_patch, bl_version, hw_rev))
        # write major, minor, patch and bl version to version.ini
        ini_file.writelines(
            "[version]" + "\nmajor=" + firmware_major + "\nminor=" + firmware_minor + "\npatch=" + firmware_patch + "\nbl=" + bl_version + "\nhw=" + hw_rev+ "\n")
        ini_file.close()

    if path_version_h == '' and (increment_patch or increment_minor):
        print("Job done without generating a new version.h file (path missing)")
        sys.exit(0)

    if path_version_h == '':
        print("ERROR: version.h path is missing")
        print(help)
        sys.exit()
    else:
        header_file = open(path_version_h, 'w')

    print("Writing version into {}: {}.{}.{} / bl:{} / hw:{}".format(path_version_h, firmware_major, firmware_minor,
                                                                     firmware_patch, bl_version, hw_rev))

    # update version.h
    header_file.seek(0)

    header_file.write("#ifndef VERSION_H__\n")
    header_file.write("#define VERSION_H__\n")
    header_file.write("\n")
    header_file.write("/* clang-format off */")
    header_file.write("\n")
    proc = subprocess.Popen('git rev-parse --abbrev-ref HEAD', shell=True, stdout=subprocess.PIPE, )
    build=proc.communicate()[0]
    branch_len = len(build.rstrip())
    header_file.write( "#define     FIRMWARE_BRANCH_NAME_SIZE   {}\n".format(str(branch_len)))
    header_file.write( "#define     FIRMWARE_BRANCH_NAME        \"{}\"\n".format(build.rstrip()[:branch_len]))
    proc=subprocess.Popen('git rev-parse HEAD', shell=True, stdout=subprocess.PIPE, )
    build=proc.communicate()[0]
    header_file.write( "#define     FIRMWARE_BRANCH_SHA         0x{}\n".format(build.rstrip()[:8]))
    header_file.write( "\n")
    header_file.write( "#define     FIRMWARE_TYPE_DEV           0xFF\n")
    header_file.write( "#define     FIRMWARE_TYPE_PROD          0x00\n")
    header_file.write( "\n")
    header_file.write( "#define     FIRMWARE_VERSION_MAJOR      {}\n".format(firmware_major))
    header_file.write( "#define     FIRMWARE_VERSION_MINOR      {}\n".format(firmware_minor))
    header_file.write( "#define     FIRMWARE_VERSION_PATCH      {}\n".format(firmware_patch))
    header_file.write( "\n")
    header_file.write( "#ifdef DEBUG\n")
    header_file.write( "\t#define     FIRMWARE_TYPE       FIRMWARE_TYPE_DEV\n")
    header_file.write( "#else\n")
    header_file.write( "\t#define     FIRMWARE_TYPE       FIRMWARE_TYPE_PROD\n")
    header_file.write( "#endif\n")
    header_file.write( "\n")
    header_file.write( "#define     HARDWARE_REV                {}\n".format(hw_rev))
    header_file.write( "/* clang-format on */")
    header_file.write( "\n")
    header_file.write( "#endif\n")


if __name__ == '__main__':
    main(sys.argv[1:])
