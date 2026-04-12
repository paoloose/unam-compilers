#!/bin/bash

# This file invokes the syntax analyzer agains a set of input files in a directory

# Usage
# run_tests.sh <directory>

INPUT_DIR=$1
GRAMMAR_FILE="part_1/grammar.txt"
EXECUTABLE="./build/first_and_follow"

if [ -z "$INPUT_DIR" ]; then
  echo "Usage: $0 <directory>"
  exit 1
fi

if [ ! -f "$EXECUTABLE" ]; then
  echo "Error: Executable not found at $EXECUTABLE"
  echo "Please build the project first."
  exit 1
fi

if [ ! -f "$GRAMMAR_FILE" ]; then
  echo "Error: Grammar file not found at $GRAMMAR_FILE"
  exit 1
fi

for test_file in "$INPUT_DIR"/*; do
  $EXECUTABLE $GRAMMAR_FILE $test_file > /dev/null 2>&1
  status=$?
  echo --------------------------------
  echo $test_file

  if [[ "$test_file" == *"accept.txt" ]]; then
    if [ $status -eq 0 ]; then
      echo "Result: OK (Expected accept, got accept)"
    else
      echo "Result: FAILED (Expected accept, got reject)"
    fi
  elif [[ "$test_file" == *"reject.txt" ]]; then
    if [ $status -ne 0 ]; then
      echo "Result: OK (Expected reject, got reject)"
    else
      echo "Result: FAILED (Expected reject, got accept)"
    fi
  else
    if [ $status -eq 0 ]; then
      echo "Result: ACCEPTED"
    else
      echo "Result: REJECTED"
    fi
  fi
done

echo
echo "-> All tests finished."
