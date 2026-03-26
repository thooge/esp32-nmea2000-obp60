#!/bin/bash

dir="$1"

total=0

while IFS= read -r -d '' file; do
    lines=$(wc -l < "$file")
    echo "$file : $lines"
    total=$((total + lines))
done < <(find "$dir" \( -name "*.c" -o -name "*.cpp" -o -name "*.h" \) -type f -print0)

echo "-----------------------"
echo "Over all files: $total"