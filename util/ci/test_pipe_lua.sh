#!/bin/bash -e
# Test the Client Lua Pipe (named pipe IPC for client-side Lua execution)
# Requires xvfb-run or Xvfb for headless display.
#
# Usage:
#   ./util/ci/test_pipe_lua.sh

PIPE_PATH="/tmp/antilua_lua_test"
RESP_FILE="/tmp/antilua_lua_test_resp"
CONFIG_FILE=$(mktemp)
WORLD_DIR=$(mktemp -d)
VICTIM_FILE=$(mktemp)
RUNTIME_DIR=$(mktemp -d)
chmod 700 "$RUNTIME_DIR"
export XDG_RUNTIME_DIR="$RUNTIME_DIR"

cleanup() {
	kill $GAME_PID 2>/dev/null || true
	wait $GAME_PID 2>/dev/null || true
	rm -f "$PIPE_PATH" "$RESP_FILE" "$CONFIG_FILE" "$VICTIM_FILE"
	rm -rf "$WORLD_DIR" "$RUNTIME_DIR"
}
trap cleanup EXIT

# Create config with pipe enabled
cat > "$CONFIG_FILE" << 'ENDCONF'
pipe_lua_enable = true
pipe_lua_path = /tmp/antilua_lua_test
ENDCONF

cat > "$WORLD_DIR/world.mt" << 'ENDWORLD'
gameid = devtest
backend = dummy
player_backend = dummy
auth_backend = dummy
ENDWORLD

echo "=== Client Lua Pipe Test ==="

# Find virtual display tool
if command -v xvfb-run &>/dev/null; then
	VIRT_DISPLAY="xvfb-run --auto-servernum"
elif command -v Xvfb &>/dev/null; then
	Xvfb :99 -screen 0 1024x768x24 &
	export DISPLAY=:99
else
	echo "SKIP: Need xvfb-run or Xvfb for headless testing"
	exit 0
fi

# Start game in background with 30s timeout
$VIRT_DISPLAY timeout 30 ./bin/antilua --info --world "$WORLD_DIR" --go \
	--config "$CONFIG_FILE" 2>/dev/null &
GAME_PID=$!

# Wait for the pipe to appear (up to 20 seconds)
for i in $(seq 1 40); do
	if [ -p "$PIPE_PATH" ]; then
		break
	fi
	sleep 0.5
done

if [ ! -p "$PIPE_PATH" ]; then
	echo "FAIL: Pipe was not created within 20 seconds"
	exit 1
fi

PIPE_MODE=$(stat -c '%a' "$PIPE_PATH")
if [ "$PIPE_MODE" != "600" ]; then
	echo "FAIL: Pipe mode is $PIPE_MODE, expected 600"
	exit 1
fi

PASS_COUNT=0
FAIL_COUNT=0

pipe_request() {
	rm -f "$RESP_FILE"
	printf '%s\n' "$1" > "$PIPE_PATH"
	for _ in $(seq 1 50); do
		[ -f "$RESP_FILE" ] && return 0
		sleep 0.1
	done
	return 1
}

check() {
	local name="$1"
	local expected="$2"
	local actual="$3"
	if [ "$actual" = "$expected" ]; then
		echo "  PASS: $name"
		PASS_COUNT=$((PASS_COUNT + 1))
	else
		echo "  FAIL: $name"
		echo "    expected: $expected"
		echo "    got:      $actual"
		FAIL_COUNT=$((FAIL_COUNT + 1))
	fi
}

# Test 1: simple arithmetic expression
pipe_request '{"code":"return 1+1","file":"'$RESP_FILE'"}' || true
RESULT=$(cat "$RESP_FILE" 2>/dev/null || echo "timeout")
check "simple expression" "$(printf "ok\n2")" "$RESULT"

# Test 2: error handling
pipe_request '{"code":"error(\"test error\")","file":"'$RESP_FILE'"}' || true
RESULT=$(cat "$RESP_FILE" 2>/dev/null || echo "timeout")
if echo "$RESULT" | head -1 | grep -q '^error$'; then
	echo "  PASS: error handling"
	PASS_COUNT=$((PASS_COUNT + 1))
else
	echo "  FAIL: error handling"
	echo "    expected: error/..."
	echo "    got:      $RESULT"
	FAIL_COUNT=$((FAIL_COUNT + 1))
fi

# Test 3: string result
pipe_request '{"code":"return \"hello\"","file":"'$RESP_FILE'"}' || true
RESULT=$(cat "$RESP_FILE" 2>/dev/null || echo "timeout")
check "string result" "$(printf "ok\nhello")" "$RESULT"

# Test 4: boolean results
pipe_request '{"code":"return true, false","file":"'$RESP_FILE'"}' || true
RESULT=$(cat "$RESP_FILE" 2>/dev/null || echo "timeout")
check "boolean results" "$(printf "ok\ntrue\nfalse")" "$RESULT"

# Test 5: nil result
pipe_request '{"code":"return nil","file":"'$RESP_FILE'"}' || true
RESULT=$(cat "$RESP_FILE" 2>/dev/null || echo "timeout")
check "nil result" "$(printf "ok\nnil")" "$RESULT"

# Test 6: no return value
pipe_request '{"code":"local x = 1","file":"'$RESP_FILE'"}' || true
RESULT=$(cat "$RESP_FILE" 2>/dev/null || echo "timeout")
check "no return value" "$(printf "ok")" "$RESULT"

# Test 7: table serialization via tostring
pipe_request '{"code":"return {1,2,3}","file":"'$RESP_FILE'"}' || true
RESULT=$(cat "$RESP_FILE" 2>/dev/null || echo "timeout")
FIRST_LINE=$(echo "$RESULT" | head -1)
if [ "$FIRST_LINE" = "ok" ]; then
	echo "  PASS: table result"
	PASS_COUNT=$((PASS_COUNT + 1))
else
	echo "  FAIL: table result"
	echo "    got: $RESULT"
	FAIL_COUNT=$((FAIL_COUNT + 1))
fi

# Test 8: discard an oversized request without poisoning the next request
rm -f "$RESP_FILE"
{
	printf '{"code":"'
	head -c 70000 /dev/zero | tr '\000' x
	printf '","file":"%s"}\n' "$RESP_FILE"
} > "$PIPE_PATH"
pipe_request '{"code":"return \"after oversized\"","file":"'$RESP_FILE'"}' || true
RESULT=$(cat "$RESP_FILE" 2>/dev/null || echo "timeout")
check "oversized request recovery" "$(printf "ok\nafter oversized")" "$RESULT"

# Test 9: refuse a symlink response without modifying its target
printf 'preserved\n' > "$VICTIM_FILE"
rm -f "$RESP_FILE"
ln -s "$VICTIM_FILE" "$RESP_FILE"
printf '%s\n' '{"code":"return \"unsafe\"","file":"'$RESP_FILE'"}' > "$PIPE_PATH"
sleep 0.5
RESULT=$(cat "$VICTIM_FILE")
check "symlink response refusal" "preserved" "$RESULT"
rm -f "$RESP_FILE"

# Test 10: reject invalid field types and continue processing
printf '%s\n' \
	'{"code":{"not":"a string"}}' \
	'{"code":"return 1","file":["not","a","string"]}' > "$PIPE_PATH"
pipe_request '{"code":"return \"after invalid fields\"","file":"'$RESP_FILE'"}' || true
RESULT=$(cat "$RESP_FILE" 2>/dev/null || echo "timeout")
check "invalid field recovery" "$(printf "ok\nafter invalid fields")" "$RESULT"

# Test 11: detach metadata is stored in an owner-only directory and file
pipe_request '{"code":"core.detach()","file":"'$RESP_FILE'"}' || true
SESSION_DIR="$RUNTIME_DIR/antilua"
SESSION_FILE="$SESSION_DIR/session"
check "session directory mode" "700" "$(stat -c '%a' "$SESSION_DIR")"
check "session file mode" "600" "$(stat -c '%a' "$SESSION_FILE")"

# Test 12: opt-in JSON preserves structured Lua return values
pipe_request '{"code":"return {answer=42,items={\"stone\",\"dirt\"}}","file":"'$RESP_FILE'","result_format":"json"}' || true
RESULT=$(cat "$RESP_FILE" 2>/dev/null || echo "timeout")
check "structured JSON result" \
	"$(printf 'ok\n[{"answer":42,"items":["stone","dirt"]}]')" "$RESULT"

echo ""
echo "=== Results: $PASS_COUNT passed, $FAIL_COUNT failed ==="
[ "$FAIL_COUNT" -eq 0 ]
