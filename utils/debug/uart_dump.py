#!/usr/bin/env python
import datetime
import getopt
import re
import sys

from serial import Serial


class colors:
    END = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4'
    SLOW = '\033[5'
    FAST = '\033[6'
    BLACK = '\033[30m'
    RED = '\033[31m'
    GREEN = '\033[32m'
    YELLOW = '\033[33m'
    BLUE = '\033[34m'
    MAGENTA = '\033[35m'
    CYAN = '\033[36m'
    WHITE = '\033[37m'
    BLINK = '\033[5m'
    VERBOSE = GREEN
    INFO = WHITE
    ERROR = RED
    FATAL = RED
    WARNING = YELLOW



def main(argv):
    baud_rate = 115200
    port = ""
    help = "Usage: {} -p <port> [-b <baud_rate>] \nDefault baud rate: 115200".format(
        sys.argv[0])

    if len(sys.argv) < 2:
        print(help)
        quit()

    try:
        opts, args = getopt.getopt(argv[1:], "p:b:h", ["port="])

        if opts != None:
            for opt, arg in opts:
                if opt == '-h':
                    print(help)
                    sys.exit()
                elif opt in ("-p", "--port"):
                    port = arg
                elif opt in "-b":
                    baud_rate = int(arg)
    except getopt.GetoptError:
        print(help)

    # find "[00000000]" or [00:00:00.000,000]
    timestamp_re = re.compile(r'(\[[0-9]{8}\])')
    formatted_timestamp_re = re.compile(r'(\[([0-9]{2}.){3}[0-9]{3}\,[0-9]{3}\])')

    ser = Serial(port, 115200)
    print("🎧 Listening UART (8N1 {}) on {}".format(baud_rate, port))

    while 1:
        line = ser.readline()
        if not line:
            break

        to_print = ""
        try:
            to_print = line.decode('utf-8').rstrip()
        except Exception as err:
            print("ERROR: {}".format(err))

        text_color = colors.END
        if to_print.__contains__("<err>"):
            text_color = colors.RED
        elif to_print.__contains__("<wrn>"):
            text_color = colors.YELLOW
        elif to_print.__contains__("<inf>"):
            text_color = colors.GREEN
        elif to_print.__contains__("<dbg>"):
            text_color = colors.END

        now = datetime.datetime.now()

        with_colored_remote_time = to_print
        if timestamp_re.search(to_print):
            with_colored_remote_time = timestamp_re.sub(colors.BLUE + '\\1' + text_color, to_print)
        if formatted_timestamp_re.search(to_print):
            with_colored_remote_time = formatted_timestamp_re.sub(colors.BLUE + '\\1' + text_color, to_print)
        local_time = f"{now.isoformat(' ')[:23]}" + colors.END
        to_print = local_time + " " + with_colored_remote_time + colors.END

        print(to_print)


if __name__ == '__main__':
    main(sys.argv)
