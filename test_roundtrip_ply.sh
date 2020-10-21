#!/usr/bin/env bash

input=`readlink -f $2`

# ./roundtrip_ply input.ply output.ply
$1 $input $TEST_TMPDIR/output.ply

# show sizes for debugging
du -hs $input
du -hs $TEST_TMPDIR/output.ply

# diff input/output
diff -q $input $TEST_TMPDIR/output.ply
