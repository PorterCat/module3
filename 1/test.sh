#!/bin/sh
set -e

cd "$(dirname "$0")/bin"

dd if=/dev/zero of=test-file bs=4096 count=1024 2>/dev/null

echo "Anonymous pipes:"
./my_cp test-file >/dev/null 2>&1 &
pid=$!
attempt=0
seen_pipe=0
while [ "$attempt" -lt 100000 ]; do
    if ls -l "/proc/$pid/fd" 2>/dev/null | grep -q 'pipe:\['; then
        ls -l "/proc/$pid/fd" | grep 'pipe:\['
        seen_pipe=1
        break
    fi
    attempt=$((attempt + 1))
done
wait "$pid"
[ "$seen_pipe" -eq 1 ]
cmp test-file test-file.copy

echo "FIFO files:"
./my_cp -p test-channel test-file >/dev/null 2>&1 &
pid=$!
attempt=0
seen_fifo=0
while [ "$attempt" -lt 100000 ]; do
    if [ -p test-channel.data ] && [ -p test-channel.control ]; then
        ls -l test-channel.data test-channel.control
        seen_fifo=1
        break
    fi
    attempt=$((attempt + 1))
done
wait "$pid"
[ "$seen_fifo" -eq 1 ]
cmp test-file test-file.copy

if ./my_cp missing-file >/dev/null 2>&1; then
    echo "Missing file test failed"
    exit 1
fi

if ./my_cp >/dev/null 2>&1; then
    echo "No arguments test failed"
    exit 1
fi

if ./my_cp -z test-file >/dev/null 2>&1; then
    echo "Invalid option test failed"
    exit 1
fi

rm -f test-file test-file.copy
echo "Tests passed"