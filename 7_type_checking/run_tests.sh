#!/bin/bash

# Ennuyeux Test Runner
# Re-builds the parser and executes all examples in the examples/ directory.

PARSER="./ennuyeux_parser"
TESTS_DIR="tests"
SUCCESS_COUNT=0
TOTAL_COUNT=0

# Ensure we are in the right directory
if [ ! -f "Makefile" ]; then
    echo "Error: Makefile not found. Please run this script from the project root."
    exit 1
fi

# Build first
make all
if [ $? -ne 0 ]; then
    echo "❌ Build failed!"
    exit 1
fi
echo

echo "🪰 Running Test Suite"
echo

# Loop through all .ennuyeux files in examples/ and tests/
for test_file in $(find $TESTS_DIR -type f -name "*.ennuyeux" | sort); do
    TOTAL_COUNT=$((TOTAL_COUNT + 1))
    filename=$(basename "$test_file")
    # For reporting, use the relative path so we can see which dir it's in
    display_name="${test_file#*/}"

    # Run the parser and capture output
    $PARSER "$test_file" 1>/dev/null 2>/dev/null
    exit_code=$?

    # Check if this is an intentional error file
    if [[ "$test_file" == *"error"* ]] || [[ "$test_file" == *"mismatch"* ]] || [[ "$test_file" == *"invalid_syntax"* ]]; then
        if [ $exit_code -ne 0 ]; then
            printf "✅ [%-33s] PASS (Expected Failure)\n" "$display_name"
            SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
        else
            printf "❌ [%-33s] FAIL (Expected failure but parsed successfully)\n" "$display_name"
        fi
    else
        if [ $exit_code -eq 0 ]; then
            printf "✅ [%-33s] PASS\n" "$display_name"
            SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
        else
            printf "❌ [%-33s] FAIL\n" "$display_name"
        fi
    fi
done

echo
echo "🪰 Got $SUCCESS_COUNT / $TOTAL_COUNT passed"

if [ $SUCCESS_COUNT -eq $TOTAL_COUNT ]; then
    echo "All tests passed successfully 🪰🪰"
    exit 0
else
    echo "Some tests failed"
    exit 1
fi
