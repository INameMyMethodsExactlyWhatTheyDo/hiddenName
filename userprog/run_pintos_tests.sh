#!/bin/bash

TEST_FILES='
wait-twice
exec-missing
'

for file in $TEST_FILES; do
    echo "" # add line spacing between tests
    rm build/tests/userprog/$file.output 2> /dev/null
    make build/tests/userprog/$file.result > /dev/null
	cat build/tests/userprog/$file.output
done
