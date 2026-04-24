#!/bin/bash

# Ennuyeux Test Runner
# Re-builds the parser and executes all examples in the examples/ directory.

PARSER="./ennuyeux_parser"
EXAMPLES_DIR="examples"
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

# Loop through all .ennuyeux files in examples/
for test_file in $(ls $EXAMPLES_DIR/*.ennuyeux | sort); do
    TOTAL_COUNT=$((TOTAL_COUNT + 1))
    filename=$(basename "$test_file")
    
    # Run the parser and capture output
    output=$($PARSER "$test_file" 2>&1)
    exit_code=$?
    
    # Check if this is an intentional error file (contains 'error')
    if [[ "$filename" == *"error"* ]]; then
        if [ $exit_code -ne 0 ]; then
            printf "✅ [%-33s] PASS (Expected Failure)\n" "$filename"
            SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
        else
            printf "❌ [%-33s] FAIL (Expected failure but parsed successfully)\n" "$filename"
        fi
    else
        if [ $exit_code -eq 0 ]; then
            printf "✅ [%-33s] PASS\n" "$filename"
            SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
        else
            printf "❌ [%-33s] FAIL\n" "$filename"
            echo "   Message: $(echo "$output" | grep -i "error" | head -n 1)"
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
