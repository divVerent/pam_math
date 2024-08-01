#!/bin/sh

set -ex

config=${1:-examples/all_basic}
user=${2:-user}

tmpdir=$(mktemp -d -t pam_math_test.XXXXXX)
trap 'rm -vrf "$tmpdir"' EXIT

name=${config##*/}

sed -e "s, pam_math\.so , $PWD/pam_math.so ,g" "$config" > "$tmpdir/$name"

export LD_PRELOAD=libpam_wrapper.so
export PAM_WRAPPER=1
export PAM_WRAPPER_SERVICE_DIR=$tmpdir
pamtester "$name" "$user" authenticate
