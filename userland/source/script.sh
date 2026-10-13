#!/bin/dash
x=5
y=10
echo "sum: $((x + y))"

if [ "$x" -lt "$y" ]; then
    echo "$x is less than $y"
else
    echo "$x is not less than $y"
fi

for i in 1 2 3; do
    echo "loop: $i"
done

count=0
while [ "$count" -lt 3 ]; do
    echo "while: $count"
    count=$((count + 1))
done
