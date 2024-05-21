#!/bin/sh

set -ex

gcc -Wall -fPIC -shared -O3 -o pam_math.so pam_math.c

tmpdir=$(mktemp -d pam_math_test.XXXXXX)
trap 'rm -vrf "$tmpdir"' EXIT

cat >"$tmpdir/math" <<EOF
auth required $PWD/pam_math.so \
  .attempts=3 .amin=-10 .amax=10 .mmin=-10 .mmax=10 \
  .questions=3 \
  .ops=+-*/dqrm
EOF

export LD_PRELOAD=libpam_wrapper.so
export PAM_WRAPPER=1
export PAM_WRAPPER_SERVICE_DIR=$tmpdir
pamtester math u authenticate
