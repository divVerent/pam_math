#!/bin/sh

find /lib* /usr/lib* /usr/local/lib* -name pam_deny.so -o -name pam_deny.so.\* |\
head -n 1 |\
rev |\
cut -d / -f 2- |\
rev |\
grep .
