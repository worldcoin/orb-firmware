#!/bin/bash

set -eu
set -o pipefail

CLANG_FORMAT=clang-format-12

c_and_h_files=$(find . -path ./build -prune -o -name '*.[ch]' -print)
yaml_files=$(find . -path ./build -prune -o -name '*.yaml' -print)

show_help() {
    echo "$0 [format|check|help]"
}

do_check() {
    if ! output=$("$CLANG_FORMAT" --dry-run $c_and_h_files 2>&1); then
        echo "Error running $CLANG_FORMAT!"
        echo "$output"
        exit 1
    fi

    if [ -n "$output" ]; then
        echo "Formatting required!"
        echo "$output"
        exit 1
    fi

    [ -t 1 ] && yamllint_flags='-f colored'

    if ! output=$(yamllint $yamllint_flags $yaml_files 2>&1) || [ -n "$output" ]; then
        echo "Error running yamllint or formatting needed!"
        echo "$output"
        exit 1
    fi
}

do_format() {
    "$CLANG_FORMAT" -i $c_and_h_files
}

if [ $# -gt 0 ]; then
    case "$1" in
        check)
            do_check
            ;;
        format)
            do_format
            ;;
        help)
            show_help
            ;;
    esac
else
    do_format
fi
