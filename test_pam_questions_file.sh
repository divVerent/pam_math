#!/bin/sh

set -ex

config=${1:-examples/questions_file}
user=${2:-user}

tmpdir=$(mktemp -d -t pam_math_test.XXXXXX)
trap 'rm -vrf "$tmpdir"' EXIT

name=${config##*/}

sed -e "s, pam_questions_file\.so , $PWD/pam_questions_file.so ,g" "$config" > "$tmpdir/$name"

export LD_PRELOAD=libpam_wrapper.so
export PAM_WRAPPER=1
export PAM_WRAPPER_SERVICE_DIR=$tmpdir
pamtester "$name" "$user" authenticate
