#!/usr/bin/env bash

#
# Copyright (C) 2015 Richard Burke
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#

# Return error code if any command in a pipeline fails
set -o pipefail
# expands non-matching globs to zero arguments 
shopt -s nullglob

warn() {
    echo "$@" >&2
}

fatal() {
    warn "$@"
    exit 1
}

# Set working directory to script directory
cd "$(dirname "$(realpath "$0")")"

WED_BIN=../../wed

if [ ! -f "$WED_BIN" ]; then
    fatal "$WED_BIN binary doesn't exist"
fi

# Generate array of test directories
TESTS=([0-9][0-9][0-9]-*/)
TEST_NUM=${#TESTS[@]}

if [ $TEST_NUM -eq 0 ]; then
    fatal 'No tests found'    
fi

TEST_OUTPUT_FILE='test.output'
TEST_SUCCESS_NUM=0

for t in "${TESTS[@]}"; do
    # Remove trailing /
    t="${t%/}"

    if ! ls "$t"/{input,output,cmd} >/dev/null 2>&1; then
        warn "Invalid test found: $t"      
        continue
    fi 

    /bin/rm -f "$t/$TEST_OUTPUT_FILE"

    TEST_CMD="$(sed '/^[[:space:]]*#/d' "$t/cmd" | tr -d '\n')"
    TEST_CMD+="<M-C-s>$t/$TEST_OUTPUT_FILE<Enter>"

    TEST_CFG_OPT=''

    if [ -f "$t/config" ]; then
        TEST_CFG_OPT="--config-file $t/config"
    fi

    "$WED_BIN" --test-mode --key-string "$TEST_CMD" $TEST_CFG_OPT "$t/input"
    EXIT_CODE=$?

    if [ $EXIT_CODE -ne 0 ]; then
        warn "Test $t FAILED: exit code $EXIT_CODE"
    elif [ ! -f "$t/$TEST_OUTPUT_FILE" ]; then
        warn "Test $t FAILED: output file $t/$TEST_OUTPUT_FILE doesn't exist"
    elif ! diff "$t/output" "$t/$TEST_OUTPUT_FILE" >/dev/null; then
        warn "Test $t FAILED: output differs from expected output"
    else
        ((TEST_SUCCESS_NUM++))
    fi
done

if [ $TEST_SUCCESS_NUM -lt $TEST_NUM ]; then
    fatal "$(
        awk -v tests=$TEST_NUM -v passed=$TEST_SUCCESS_NUM 'BEGIN {
            failed = tests - passed;
            pct_passed = int((passed / tests) * 100);
            pct_failed = 100 - pct_passed;

            printf "Test summary: %d (%d%%) failed " \
                   "and %d (%d%%) passed out of %d tests",
                   failed, pct_failed, passed, pct_passed, tests;
        }'
    )"
fi

echo "All $TEST_NUM text tests passed"

