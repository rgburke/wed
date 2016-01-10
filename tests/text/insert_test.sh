#!/bin/bash

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

# Utility script for adding tests that were forgotten
# or should have been added earlier ;)

shopt -s nullglob

print_usage() {
    echo \
"Usage:

insert_test.sh TESTNAME

TESTNAME:
The new test name in the format \d{3}-[-\w]+
i.e. [number]-[name]
e.g. 001-insert

Insert a new test at an existing test number.
All tests with a test number greater than or equal
to the supplied test number are incremented.
"
}

fatal() {
    echo "ERROR: $@" 1>&2
    exit 1
}

fatal_usage() {
    echo "ERROR: $@" 1>&2
    print_usage 1>&2
    exit 1 
}

TEST_NAME="$1"

if [ -z "$TEST_NAME" ]; then
    fatal_usage 'Must provide TESTNAME'
fi

if [[ "$TEST_NAME" =~ ^([0-9]{3})-[-_[:alnum:]]+$ ]]; then
    TEST_NUM=$((10#${BASH_REMATCH[1]}))
else
    fatal_usage "Invalid TESTNAME: $TEST_NAME"
fi

cd "$(dirname "$(realpath "$0")")"

TESTS=([0-9][0-9][0-9]-*/)
TOTAL_TEST_NUM=${#TESTS[@]}

if [ $TOTAL_TEST_NUM -eq 0 ]; then
    fatal 'No tests found'
elif [ $TEST_NUM -gt $TOTAL_TEST_NUM ]; then
    fatal "$TEST_NAME is greater than largest existing test:" \
          "${TESTS[$TOTAL_TEST_NUM - 1]}"
fi

for ((k = $TOTAL_TEST_NUM; k >= $TEST_NUM; k--)); do
    TEST_NAME_PART=$(echo "${TESTS[$k - 1]}" | tail -c +5)
    NEW_TEST_NAME=$(printf "%03d-%s" "$((k + 1))" "$TEST_NAME_PART")
    
    git mv "${TESTS[$k - 1]}" "$NEW_TEST_NAME" || \
        fatal "Unable to rename directory ${TESTS[$k - 1]} to $NEW_TEST_NAME"
done

mkdir "$TEST_NAME" || fatal "Unable to create directory $TEST_NAME"

echo "Test directory $TEST_NAME successfully created"

