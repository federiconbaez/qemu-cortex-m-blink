#!/bin/sh
# Runs the firmware images in QEMU and checks exit codes (semihosting) and
# the UART transcript. Exit 0 only if every check passes.
set -u
QEMU=${QEMU:-qemu-system-arm}
run() {
  timeout 20 "$QEMU" -M lm3s6965evb -display none -monitor none -serial stdio \
    -semihosting-config enable=on,target=native -kernel "$1"
}
fail=0

out=$(run build/blink.elf); rc=$?
printf '%s\n' "$out"
[ "$rc" -eq 0 ] || { echo "blink.elf: exit code $rc, want 0"; fail=1; }
printf '%s\n' "$out" | grep -qx PASS || { echo "blink.elf: no PASS line"; fail=1; }
printf '%s\n' "$out" | grep '^edge ' > build/edges.txt
if ! diff -u tests/golden_edges.txt build/edges.txt; then
  echo "blink.elf: edge timeline differs from tests/golden_edges.txt"; fail=1
fi

out=$(run build/blink-fault.elf); rc=$?
printf '%s\n' "$out"
[ "$rc" -eq 3 ] || { echo "blink-fault.elf: exit code $rc, want 3"; fail=1; }
# UNDEFINSTR (CFSR bit 16) escalated to HardFault (HFSR FORCED, bit 30).
printf '%s\n' "$out" | grep -q 'cfsr=0x00010000 hfsr=0x40000000' || { echo "blink-fault.elf: fault status not reported"; fail=1; }

# The reported PC must be the address of the udf instruction in the image.
OBJDUMP=${OBJDUMP:-arm-none-eabi-objdump}
udf=$("$OBJDUMP" -d build/blink-fault.elf | awk '/\tudf/ { sub(":", "", $1); print $1; exit }')
pc=$(printf '%s\n' "$out" | sed -n 's/.*hardfault pc=0x0*\([0-9a-f]*\).*/\1/p')
[ -n "$udf" ] && [ "$pc" = "$udf" ] || { echo "blink-fault.elf: pc '$pc' is not the udf at '$udf'"; fail=1; }

[ "$fail" -eq 0 ] && echo "qemu-test: all checks passed"
exit "$fail"
