#!/bin/sh

set -ex

gcc -Wall -fPIC -shared -O3 -o pam_math.so pam_math.c

podman run -i --pull=newer --rm --mount=type=bind,source=$PWD,target=/pam_math \
	docker.io/library/debian:stable /bin/sh -exc '
		apt update
		apt install pamtester
		install -m755 /pam_math/pam_math.so /lib/x86_64-linux-gnu/security/
		echo "auth requisite pam_math.so .questions=3 .amax=100 .mmax=10 .ops=+-*/" > /etc/pam.d/math
		pamtester math u authenticate
	'
