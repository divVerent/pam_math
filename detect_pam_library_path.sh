#!/bin/sh

find /lib* /usr/lib* /usr/local/lib* -name pam_unix.so |\
head -n 1 |\
rev |\
cut -d / -f 2- |\
rev |\
grep .
