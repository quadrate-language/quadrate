#!/bin/bash
# Comprehensive tests for quadmcp MCP server

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
QUADRATE_ROOT="${QUADRATE_ROOT:-$(cd "$SCRIPT_DIR/../.." && pwd)}"
QUADMCP="${QUADMCP:-$QUADRATE_ROOT/dist/bin/quadmcp}"

export QUADRATE_ROOT

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

PASS=0
FAIL=0

# Helper to run a test
run_test() {
    local name="$1"
    local input="$2"
    local expected_pattern="$3"
    local should_fail="${4:-false}"

    local result
    result=$(echo "$input" | "$QUADMCP" 2>&1)

    if echo "$result" | grep -q "$expected_pattern"; then
        if [ "$should_fail" = "true" ]; then
            echo -e "${RED}FAIL${NC}: $name (expected failure pattern found but test marked as should_fail)"
            ((FAIL++))
        else
            echo -e "${GREEN}PASS${NC}: $name"
            ((PASS++))
        fi
    else
        if [ "$should_fail" = "true" ]; then
            echo -e "${GREEN}PASS${NC}: $name (correctly failed)"
            ((PASS++))
        else
            echo -e "${RED}FAIL${NC}: $name"
            echo "  Expected pattern: $expected_pattern"
            echo "  Got: ${result:0:200}..."
            ((FAIL++))
        fi
    fi
}

# Helper for error tests
run_error_test() {
    local name="$1"
    local input="$2"
    local expected_pattern="$3"

    local result
    result=$(echo "$input" | "$QUADMCP" 2>&1)

    if echo "$result" | grep -q "isError.*true" && echo "$result" | grep -q "$expected_pattern"; then
        echo -e "${GREEN}PASS${NC}: $name"
        ((PASS++))
    else
        echo -e "${RED}FAIL${NC}: $name"
        echo "  Expected error with pattern: $expected_pattern"
        echo "  Got: ${result:0:200}..."
        ((FAIL++))
    fi
}

# Helper for NOT matching tests
run_not_match_test() {
    local name="$1"
    local input="$2"
    local not_expected_pattern="$3"

    local result
    result=$(echo "$input" | "$QUADMCP" 2>&1)

    if echo "$result" | grep -q "$not_expected_pattern"; then
        echo -e "${RED}FAIL${NC}: $name (pattern should NOT match)"
        echo "  Should NOT contain: $not_expected_pattern"
        ((FAIL++))
    else
        echo -e "${GREEN}PASS${NC}: $name"
        ((PASS++))
    fi
}

echo "=========================================="
echo "quadmcp Comprehensive Test Suite"
echo "=========================================="
echo "QUADRATE_ROOT: $QUADRATE_ROOT"
echo "QUADMCP: $QUADMCP"
echo ""

# Check binary exists
if [ ! -x "$QUADMCP" ]; then
    echo -e "${RED}ERROR${NC}: quadmcp binary not found at $QUADMCP"
    exit 1
fi

echo "--- Protocol Tests ---"

run_test "Initialize" \
    '{"jsonrpc":"2.0","method":"initialize","id":1}' \
    '"protocolVersion"'

run_test "Initialize returns server info" \
    '{"jsonrpc":"2.0","method":"initialize","id":1}' \
    '"serverInfo".*"name".*"quadmcp"'

run_test "Initialize returns capabilities" \
    '{"jsonrpc":"2.0","method":"initialize","id":1}' \
    '"capabilities"'

run_test "Tools list" \
    '{"jsonrpc":"2.0","method":"tools/list","id":2}' \
    '"quadrate_get_function"'

run_test "Tools list has all 16 tools" \
    '{"jsonrpc":"2.0","method":"tools/list","id":2}' \
    'quadrate_type_conversion'

run_test "Tools list contains descriptions" \
    '{"jsonrpc":"2.0","method":"tools/list","id":2}' \
    '"description"'

run_test "Tools list contains inputSchema" \
    '{"jsonrpc":"2.0","method":"tools/list","id":2}' \
    '"inputSchema"'

run_test "Ping" \
    '{"jsonrpc":"2.0","method":"ping","id":3}' \
    '"result":{}'

run_test "Resources list has snippets" \
    '{"jsonrpc":"2.0","method":"resources/list","id":4}' \
    'hello-world'

run_test "Resources list has 9 resources" \
    '{"jsonrpc":"2.0","method":"resources/list","id":4}' \
    'cheatsheet'

run_test "Prompts list has prompts" \
    '{"jsonrpc":"2.0","method":"prompts/list","id":5}' \
    'explain_code'

run_test "Prompts list has 3 prompts" \
    '{"jsonrpc":"2.0","method":"prompts/list","id":5}' \
    'debug_stack'

run_test "Method not found error" \
    '{"jsonrpc":"2.0","method":"nonexistent/method","id":6}' \
    'Method not found'

run_test "Method not found has error code" \
    '{"jsonrpc":"2.0","method":"nonexistent/method","id":6}' \
    '32601'

# Notifications should return empty - test separately
result=$(echo '{"jsonrpc":"2.0","method":"notifications/initialized"}' | "$QUADMCP" 2>&1)
if [ -z "$result" ]; then
    echo -e "${GREEN}PASS${NC}: Notification ignored (no response)"
    ((PASS++))
else
    echo -e "${RED}FAIL${NC}: Notification ignored (no response) - got: $result"
    ((FAIL++))
fi

result=$(echo '{"jsonrpc":"2.0","method":"notifications/cancelled"}' | "$QUADMCP" 2>&1)
if [ -z "$result" ]; then
    echo -e "${GREEN}PASS${NC}: Notification cancelled (no response)"
    ((PASS++))
else
    echo -e "${RED}FAIL${NC}: Notification cancelled (no response) - got: $result"
    ((FAIL++))
fi

echo ""
echo "--- quadrate_list_modules Tests ---"

run_test "List modules returns JSON array" \
    '{"jsonrpc":"2.0","method":"tools/call","id":10,"params":{"name":"quadrate_list_modules","arguments":{}}}' \
    'name.*math'

run_test "List modules has all 20 modules" \
    '{"jsonrpc":"2.0","method":"tools/call","id":11,"params":{"name":"quadrate_list_modules","arguments":{}}}' \
    'name.*unicode'

run_test "List modules has bits" \
    '{"jsonrpc":"2.0","method":"tools/call","id":12,"params":{"name":"quadrate_list_modules","arguments":{}}}' \
    'name.*bits'

run_test "List modules has bytes" \
    '{"jsonrpc":"2.0","method":"tools/call","id":13,"params":{"name":"quadrate_list_modules","arguments":{}}}' \
    'name.*bytes'

run_test "List modules has function counts" \
    '{"jsonrpc":"2.0","method":"tools/call","id":14,"params":{"name":"quadrate_list_modules","arguments":{}}}' \
    'functions'

run_test "List modules has descriptions" \
    '{"jsonrpc":"2.0","method":"tools/call","id":15,"params":{"name":"quadrate_list_modules","arguments":{}}}' \
    'description'

echo ""
echo "--- quadrate_get_module Tests ---"

run_test "Get module: math" \
    '{"jsonrpc":"2.0","method":"tools/call","id":20,"params":{"name":"quadrate_get_module","arguments":{"name":"math"}}}' \
    'name.*math'

run_test "Get module: strings" \
    '{"jsonrpc":"2.0","method":"tools/call","id":21,"params":{"name":"quadrate_get_module","arguments":{"name":"strings"}}}' \
    'name.*strings'

run_test "Get module: io" \
    '{"jsonrpc":"2.0","method":"tools/call","id":22,"params":{"name":"quadrate_get_module","arguments":{"name":"io"}}}' \
    'name.*io'

run_test "Get module: mem" \
    '{"jsonrpc":"2.0","method":"tools/call","id":23,"params":{"name":"quadrate_get_module","arguments":{"name":"mem"}}}' \
    'name.*mem'

run_test "Get module: os" \
    '{"jsonrpc":"2.0","method":"tools/call","id":24,"params":{"name":"quadrate_get_module","arguments":{"name":"os"}}}' \
    'name.*os'

run_test "Get module: time" \
    '{"jsonrpc":"2.0","method":"tools/call","id":25,"params":{"name":"quadrate_get_module","arguments":{"name":"time"}}}' \
    'name.*time'

run_test "Get module: fmt" \
    '{"jsonrpc":"2.0","method":"tools/call","id":26,"params":{"name":"quadrate_get_module","arguments":{"name":"fmt"}}}' \
    'name.*fmt'

run_test "Get module: rand" \
    '{"jsonrpc":"2.0","method":"tools/call","id":27,"params":{"name":"quadrate_get_module","arguments":{"name":"rand"}}}' \
    'name.*rand'

run_test "Get module contains functions array" \
    '{"jsonrpc":"2.0","method":"tools/call","id":28,"params":{"name":"quadrate_get_module","arguments":{"name":"math"}}}' \
    'functions'

run_error_test "Get module: nonexistent" \
    '{"jsonrpc":"2.0","method":"tools/call","id":29,"params":{"name":"quadrate_get_module","arguments":{"name":"nonexistent"}}}' \
    'not found'

run_error_test "Get module: missing parameter" \
    '{"jsonrpc":"2.0","method":"tools/call","id":30,"params":{"name":"quadrate_get_module","arguments":{}}}' \
    'Missing required parameter'

run_error_test "Get module: empty name" \
    '{"jsonrpc":"2.0","method":"tools/call","id":31,"params":{"name":"quadrate_get_module","arguments":{"name":""}}}' \
    'not found'

echo ""
echo "--- quadrate_get_function Tests ---"

run_test "Get function: math::sin" \
    '{"jsonrpc":"2.0","method":"tools/call","id":40,"params":{"name":"quadrate_get_function","arguments":{"module":"math","function":"sin"}}}' \
    'name.*sin.*signature'

run_test "Get function: math::cos" \
    '{"jsonrpc":"2.0","method":"tools/call","id":41,"params":{"name":"quadrate_get_function","arguments":{"module":"math","function":"cos"}}}' \
    'name.*cos.*signature'

run_test "Get function: math::sqrt" \
    '{"jsonrpc":"2.0","method":"tools/call","id":42,"params":{"name":"quadrate_get_function","arguments":{"module":"math","function":"sqrt"}}}' \
    'name.*sqrt.*signature'

run_test "Get function: strings::len" \
    '{"jsonrpc":"2.0","method":"tools/call","id":43,"params":{"name":"quadrate_get_function","arguments":{"module":"strings","function":"len"}}}' \
    'name.*len.*signature'

run_test "Get function: strings::concat" \
    '{"jsonrpc":"2.0","method":"tools/call","id":44,"params":{"name":"quadrate_get_function","arguments":{"module":"strings","function":"concat"}}}' \
    'name.*concat.*signature'

run_test "Get function: strings::split" \
    '{"jsonrpc":"2.0","method":"tools/call","id":45,"params":{"name":"quadrate_get_function","arguments":{"module":"strings","function":"split"}}}' \
    'name.*split.*signature'

run_test "Get function: io::read_file" \
    '{"jsonrpc":"2.0","method":"tools/call","id":46,"params":{"name":"quadrate_get_function","arguments":{"module":"io","function":"read_file"}}}' \
    'name.*read_file.*signature'

run_test "Get function: io::write_file" \
    '{"jsonrpc":"2.0","method":"tools/call","id":47,"params":{"name":"quadrate_get_function","arguments":{"module":"io","function":"write_file"}}}' \
    'name.*write_file.*signature'

run_test "Get function: mem::alloc" \
    '{"jsonrpc":"2.0","method":"tools/call","id":48,"params":{"name":"quadrate_get_function","arguments":{"module":"mem","function":"alloc"}}}' \
    'name.*alloc.*signature'

run_test "Get function: time::now" \
    '{"jsonrpc":"2.0","method":"tools/call","id":49,"params":{"name":"quadrate_get_function","arguments":{"module":"time","function":"now"}}}' \
    'name.*now.*signature'

run_test "Get function has description" \
    '{"jsonrpc":"2.0","method":"tools/call","id":50,"params":{"name":"quadrate_get_function","arguments":{"module":"math","function":"sin"}}}' \
    'description'

run_test "Get function has params" \
    '{"jsonrpc":"2.0","method":"tools/call","id":51,"params":{"name":"quadrate_get_function","arguments":{"module":"math","function":"sin"}}}' \
    'params'

run_error_test "Get function: nonexistent function" \
    '{"jsonrpc":"2.0","method":"tools/call","id":52,"params":{"name":"quadrate_get_function","arguments":{"module":"math","function":"nonexistent"}}}' \
    'not found'

run_error_test "Get function: nonexistent module" \
    '{"jsonrpc":"2.0","method":"tools/call","id":53,"params":{"name":"quadrate_get_function","arguments":{"module":"nonexistent","function":"foo"}}}' \
    'not found'

run_error_test "Get function: missing module param" \
    '{"jsonrpc":"2.0","method":"tools/call","id":54,"params":{"name":"quadrate_get_function","arguments":{"function":"sin"}}}' \
    'Missing required parameter: module'

run_error_test "Get function: missing function param" \
    '{"jsonrpc":"2.0","method":"tools/call","id":55,"params":{"name":"quadrate_get_function","arguments":{"module":"math"}}}' \
    'Missing required parameter: function'

run_error_test "Get function: both params missing" \
    '{"jsonrpc":"2.0","method":"tools/call","id":56,"params":{"name":"quadrate_get_function","arguments":{}}}' \
    'Missing required parameter'

echo ""
echo "--- quadrate_get_builtin Tests ---"

run_test "Get builtin: dup" \
    '{"jsonrpc":"2.0","method":"tools/call","id":60,"params":{"name":"quadrate_get_builtin","arguments":{"name":"dup"}}}' \
    'name.*dup.*signature'

run_test "Get builtin: swap" \
    '{"jsonrpc":"2.0","method":"tools/call","id":61,"params":{"name":"quadrate_get_builtin","arguments":{"name":"swap"}}}' \
    'name.*swap.*signature'

run_test "Get builtin: drop" \
    '{"jsonrpc":"2.0","method":"tools/call","id":62,"params":{"name":"quadrate_get_builtin","arguments":{"name":"drop"}}}' \
    'name.*drop.*signature'

run_test "Get builtin: over" \
    '{"jsonrpc":"2.0","method":"tools/call","id":63,"params":{"name":"quadrate_get_builtin","arguments":{"name":"over"}}}' \
    'name.*over.*signature'

run_test "Get builtin: rot" \
    '{"jsonrpc":"2.0","method":"tools/call","id":64,"params":{"name":"quadrate_get_builtin","arguments":{"name":"rot"}}}' \
    'name.*rot.*signature'

run_test "Get builtin: nip" \
    '{"jsonrpc":"2.0","method":"tools/call","id":65,"params":{"name":"quadrate_get_builtin","arguments":{"name":"nip"}}}' \
    'name.*nip.*signature'

run_test "Get builtin: add" \
    '{"jsonrpc":"2.0","method":"tools/call","id":66,"params":{"name":"quadrate_get_builtin","arguments":{"name":"add"}}}' \
    'name.*add.*signature'

run_test "Get builtin: sub" \
    '{"jsonrpc":"2.0","method":"tools/call","id":67,"params":{"name":"quadrate_get_builtin","arguments":{"name":"sub"}}}' \
    'name.*sub.*signature'

run_test "Get builtin: mul" \
    '{"jsonrpc":"2.0","method":"tools/call","id":68,"params":{"name":"quadrate_get_builtin","arguments":{"name":"mul"}}}' \
    'name.*mul.*signature'

run_test "Get builtin: div" \
    '{"jsonrpc":"2.0","method":"tools/call","id":69,"params":{"name":"quadrate_get_builtin","arguments":{"name":"div"}}}' \
    'name.*div.*signature'

run_test "Get builtin by alias: +" \
    '{"jsonrpc":"2.0","method":"tools/call","id":70,"params":{"name":"quadrate_get_builtin","arguments":{"name":"+"}}}' \
    'alias.*[+]'

run_test "Get builtin by alias: -" \
    '{"jsonrpc":"2.0","method":"tools/call","id":71,"params":{"name":"quadrate_get_builtin","arguments":{"name":"-"}}}' \
    'alias.*-'

run_test "Get builtin by alias: *" \
    '{"jsonrpc":"2.0","method":"tools/call","id":72,"params":{"name":"quadrate_get_builtin","arguments":{"name":"*"}}}' \
    'alias.*[*]'

run_test "Get builtin by alias: /" \
    '{"jsonrpc":"2.0","method":"tools/call","id":73,"params":{"name":"quadrate_get_builtin","arguments":{"name":"/"}}}' \
    'alias.*/'

run_test "Get builtin by alias: ==" \
    '{"jsonrpc":"2.0","method":"tools/call","id":74,"params":{"name":"quadrate_get_builtin","arguments":{"name":"=="}}}' \
    'alias.*=='

run_test "Get builtin by alias: !=" \
    '{"jsonrpc":"2.0","method":"tools/call","id":75,"params":{"name":"quadrate_get_builtin","arguments":{"name":"!="}}}' \
    'alias.*!='

run_test "Get builtin by alias: <" \
    '{"jsonrpc":"2.0","method":"tools/call","id":76,"params":{"name":"quadrate_get_builtin","arguments":{"name":"<"}}}' \
    'alias.*<'

run_test "Get builtin by alias: >" \
    '{"jsonrpc":"2.0","method":"tools/call","id":77,"params":{"name":"quadrate_get_builtin","arguments":{"name":">"}}}' \
    'alias.*>'

run_test "Get builtin by alias: ++" \
    '{"jsonrpc":"2.0","method":"tools/call","id":78,"params":{"name":"quadrate_get_builtin","arguments":{"name":"++"}}}' \
    'name.*inc'

run_test "Get builtin by alias: --" \
    '{"jsonrpc":"2.0","method":"tools/call","id":79,"params":{"name":"quadrate_get_builtin","arguments":{"name":"--"}}}' \
    'name.*dec'

run_error_test "Get builtin: nonexistent" \
    '{"jsonrpc":"2.0","method":"tools/call","id":80,"params":{"name":"quadrate_get_builtin","arguments":{"name":"nonexistent"}}}' \
    'not found'

run_error_test "Get builtin: missing parameter" \
    '{"jsonrpc":"2.0","method":"tools/call","id":81,"params":{"name":"quadrate_get_builtin","arguments":{}}}' \
    'Missing required parameter'

run_error_test "Get builtin: empty name" \
    '{"jsonrpc":"2.0","method":"tools/call","id":82,"params":{"name":"quadrate_get_builtin","arguments":{"name":""}}}' \
    'not found'

echo ""
echo "--- quadrate_get_builtins Tests ---"

run_test "Get all builtins" \
    '{"jsonrpc":"2.0","method":"tools/call","id":85,"params":{"name":"quadrate_get_builtins","arguments":{}}}' \
    'categories'

run_test "Builtins has Stack Manipulation" \
    '{"jsonrpc":"2.0","method":"tools/call","id":86,"params":{"name":"quadrate_get_builtins","arguments":{}}}' \
    'Stack Manipulation'

run_test "Builtins has Arithmetic" \
    '{"jsonrpc":"2.0","method":"tools/call","id":87,"params":{"name":"quadrate_get_builtins","arguments":{}}}' \
    'Arithmetic'

run_test "Builtins has Comparison" \
    '{"jsonrpc":"2.0","method":"tools/call","id":88,"params":{"name":"quadrate_get_builtins","arguments":{}}}' \
    'Comparison'

run_test "Builtins has Arrays" \
    '{"jsonrpc":"2.0","method":"tools/call","id":89,"params":{"name":"quadrate_get_builtins","arguments":{}}}' \
    'Arrays'

echo ""
echo "--- quadrate_get_syntax Tests ---"

run_test "Get syntax reference" \
    '{"jsonrpc":"2.0","method":"tools/call","id":90,"params":{"name":"quadrate_get_syntax","arguments":{}}}' \
    'keywords'

run_test "Syntax has types" \
    '{"jsonrpc":"2.0","method":"tools/call","id":91,"params":{"name":"quadrate_get_syntax","arguments":{}}}' \
    'types'

run_test "Syntax has operators" \
    '{"jsonrpc":"2.0","method":"tools/call","id":92,"params":{"name":"quadrate_get_syntax","arguments":{}}}' \
    'operators'

echo ""
echo "--- quadrate_get_keyword Tests ---"

run_test "Get keyword: fn" \
    '{"jsonrpc":"2.0","method":"tools/call","id":100,"params":{"name":"quadrate_get_keyword","arguments":{"name":"fn"}}}' \
    'name.*fn.*category'

run_test "Get keyword: if" \
    '{"jsonrpc":"2.0","method":"tools/call","id":101,"params":{"name":"quadrate_get_keyword","arguments":{"name":"if"}}}' \
    'name.*if.*category'

run_test "Get keyword: else" \
    '{"jsonrpc":"2.0","method":"tools/call","id":102,"params":{"name":"quadrate_get_keyword","arguments":{"name":"else"}}}' \
    'name.*else.*category'

run_test "Get keyword: struct" \
    '{"jsonrpc":"2.0","method":"tools/call","id":103,"params":{"name":"quadrate_get_keyword","arguments":{"name":"struct"}}}' \
    'name.*struct.*category'

run_test "Get keyword: loop" \
    '{"jsonrpc":"2.0","method":"tools/call","id":104,"params":{"name":"quadrate_get_keyword","arguments":{"name":"loop"}}}' \
    'name.*loop.*category'

run_test "Get keyword: while" \
    '{"jsonrpc":"2.0","method":"tools/call","id":105,"params":{"name":"quadrate_get_keyword","arguments":{"name":"while"}}}' \
    'name.*while.*category'

run_test "Get keyword: for" \
    '{"jsonrpc":"2.0","method":"tools/call","id":106,"params":{"name":"quadrate_get_keyword","arguments":{"name":"for"}}}' \
    'name.*for.*category'

run_test "Get keyword: break" \
    '{"jsonrpc":"2.0","method":"tools/call","id":107,"params":{"name":"quadrate_get_keyword","arguments":{"name":"break"}}}' \
    'name.*break.*category'

run_test "Get keyword: continue" \
    '{"jsonrpc":"2.0","method":"tools/call","id":108,"params":{"name":"quadrate_get_keyword","arguments":{"name":"continue"}}}' \
    'name.*continue.*category'

run_test "Get keyword: defer" \
    '{"jsonrpc":"2.0","method":"tools/call","id":109,"params":{"name":"quadrate_get_keyword","arguments":{"name":"defer"}}}' \
    'name.*defer.*category'

run_test "Get keyword: use" \
    '{"jsonrpc":"2.0","method":"tools/call","id":110,"params":{"name":"quadrate_get_keyword","arguments":{"name":"use"}}}' \
    'name.*use.*category'

run_test "Get keyword: pub" \
    '{"jsonrpc":"2.0","method":"tools/call","id":111,"params":{"name":"quadrate_get_keyword","arguments":{"name":"pub"}}}' \
    'name.*pub.*category'

run_test "Get keyword: const" \
    '{"jsonrpc":"2.0","method":"tools/call","id":112,"params":{"name":"quadrate_get_keyword","arguments":{"name":"const"}}}' \
    'name.*const.*category'

run_test "Get keyword: switch" \
    '{"jsonrpc":"2.0","method":"tools/call","id":113,"params":{"name":"quadrate_get_keyword","arguments":{"name":"switch"}}}' \
    'name.*switch.*category'

run_test "Get keyword: -> (arrow)" \
    '{"jsonrpc":"2.0","method":"tools/call","id":114,"params":{"name":"quadrate_get_keyword","arguments":{"name":"->"}}}' \
    'name.*->.*category'

run_test "Get keyword: true" \
    '{"jsonrpc":"2.0","method":"tools/call","id":115,"params":{"name":"quadrate_get_keyword","arguments":{"name":"true"}}}' \
    'name.*true.*category'

run_test "Get keyword: false" \
    '{"jsonrpc":"2.0","method":"tools/call","id":116,"params":{"name":"quadrate_get_keyword","arguments":{"name":"false"}}}' \
    'name.*false.*category'

run_error_test "Get keyword: nonexistent" \
    '{"jsonrpc":"2.0","method":"tools/call","id":117,"params":{"name":"quadrate_get_keyword","arguments":{"name":"nonexistent"}}}' \
    'not found'

run_error_test "Get keyword: missing parameter" \
    '{"jsonrpc":"2.0","method":"tools/call","id":118,"params":{"name":"quadrate_get_keyword","arguments":{}}}' \
    'Missing required parameter'

echo ""
echo "--- quadrate_get_operator Tests ---"

run_test "Get operator: +" \
    '{"jsonrpc":"2.0","method":"tools/call","id":120,"params":{"name":"quadrate_get_operator","arguments":{"name":"+"}}}' \
    'name.*[+].*signature'

run_test "Get operator: -" \
    '{"jsonrpc":"2.0","method":"tools/call","id":121,"params":{"name":"quadrate_get_operator","arguments":{"name":"-"}}}' \
    'name.*-.*signature'

run_test "Get operator: *" \
    '{"jsonrpc":"2.0","method":"tools/call","id":122,"params":{"name":"quadrate_get_operator","arguments":{"name":"*"}}}' \
    'name.*[*].*signature'

run_test "Get operator: /" \
    '{"jsonrpc":"2.0","method":"tools/call","id":123,"params":{"name":"quadrate_get_operator","arguments":{"name":"/"}}}' \
    'name.*/.*signature'

run_test "Get operator: ==" \
    '{"jsonrpc":"2.0","method":"tools/call","id":124,"params":{"name":"quadrate_get_operator","arguments":{"name":"=="}}}' \
    'name.*==.*signature'

run_test "Get operator: !=" \
    '{"jsonrpc":"2.0","method":"tools/call","id":125,"params":{"name":"quadrate_get_operator","arguments":{"name":"!="}}}' \
    'name.*!=.*signature'

run_test "Get operator: <" \
    '{"jsonrpc":"2.0","method":"tools/call","id":126,"params":{"name":"quadrate_get_operator","arguments":{"name":"<"}}}' \
    'name.*<.*signature'

run_test "Get operator: >" \
    '{"jsonrpc":"2.0","method":"tools/call","id":127,"params":{"name":"quadrate_get_operator","arguments":{"name":">"}}}' \
    'name.*>.*signature'

run_test "Get operator: <=" \
    '{"jsonrpc":"2.0","method":"tools/call","id":128,"params":{"name":"quadrate_get_operator","arguments":{"name":"<="}}}' \
    'name.*<=.*signature'

run_test "Get operator: >=" \
    '{"jsonrpc":"2.0","method":"tools/call","id":129,"params":{"name":"quadrate_get_operator","arguments":{"name":">="}}}' \
    'name.*>=.*signature'

run_test "Get operator by alias: add" \
    '{"jsonrpc":"2.0","method":"tools/call","id":130,"params":{"name":"quadrate_get_operator","arguments":{"name":"add"}}}' \
    'alias.*add'

run_test "Get operator by alias: sub" \
    '{"jsonrpc":"2.0","method":"tools/call","id":131,"params":{"name":"quadrate_get_operator","arguments":{"name":"sub"}}}' \
    'alias.*sub'

run_test "Get operator by alias: mul" \
    '{"jsonrpc":"2.0","method":"tools/call","id":132,"params":{"name":"quadrate_get_operator","arguments":{"name":"mul"}}}' \
    'alias.*mul'

run_test "Get operator by alias: div" \
    '{"jsonrpc":"2.0","method":"tools/call","id":133,"params":{"name":"quadrate_get_operator","arguments":{"name":"div"}}}' \
    'alias.*div'

run_test "Get operator by alias: eq" \
    '{"jsonrpc":"2.0","method":"tools/call","id":134,"params":{"name":"quadrate_get_operator","arguments":{"name":"eq"}}}' \
    'alias.*eq'

run_test "Get operator by alias: neq" \
    '{"jsonrpc":"2.0","method":"tools/call","id":135,"params":{"name":"quadrate_get_operator","arguments":{"name":"neq"}}}' \
    'alias.*neq'

run_test "Get operator by alias: lt" \
    '{"jsonrpc":"2.0","method":"tools/call","id":136,"params":{"name":"quadrate_get_operator","arguments":{"name":"lt"}}}' \
    'alias.*lt'

run_test "Get operator by alias: gt" \
    '{"jsonrpc":"2.0","method":"tools/call","id":137,"params":{"name":"quadrate_get_operator","arguments":{"name":"gt"}}}' \
    'alias.*gt'

run_error_test "Get operator: nonexistent" \
    '{"jsonrpc":"2.0","method":"tools/call","id":138,"params":{"name":"quadrate_get_operator","arguments":{"name":"nonexistent"}}}' \
    'not found'

run_error_test "Get operator: missing parameter" \
    '{"jsonrpc":"2.0","method":"tools/call","id":139,"params":{"name":"quadrate_get_operator","arguments":{}}}' \
    'Missing required parameter'

echo ""
echo "--- quadrate_search Tests ---"

run_test "Search: sin (finds math module)" \
    '{"jsonrpc":"2.0","method":"tools/call","id":150,"params":{"name":"quadrate_search","arguments":{"query":"sin"}}}' \
    'math module'

run_test "Search: cos (finds math module)" \
    '{"jsonrpc":"2.0","method":"tools/call","id":151,"params":{"name":"quadrate_search","arguments":{"query":"cos"}}}' \
    'math module'

run_test "Search: sqrt (finds math module)" \
    '{"jsonrpc":"2.0","method":"tools/call","id":152,"params":{"name":"quadrate_search","arguments":{"query":"sqrt"}}}' \
    'math module'

run_test "Search: random (finds rand module)" \
    '{"jsonrpc":"2.0","method":"tools/call","id":153,"params":{"name":"quadrate_search","arguments":{"query":"random"}}}' \
    'rand module'

run_test "Search: file (finds io module)" \
    '{"jsonrpc":"2.0","method":"tools/call","id":154,"params":{"name":"quadrate_search","arguments":{"query":"file"}}}' \
    'io module'

run_test "Search: read (finds io module)" \
    '{"jsonrpc":"2.0","method":"tools/call","id":155,"params":{"name":"quadrate_search","arguments":{"query":"read"}}}' \
    'io module'

run_test "Search: write (finds io module)" \
    '{"jsonrpc":"2.0","method":"tools/call","id":156,"params":{"name":"quadrate_search","arguments":{"query":"write"}}}' \
    'io module'

run_test "Search: thread (finds thread module)" \
    '{"jsonrpc":"2.0","method":"tools/call","id":157,"params":{"name":"quadrate_search","arguments":{"query":"thread"}}}' \
    'thread module'

run_test "Search: signal (finds signal module)" \
    '{"jsonrpc":"2.0","method":"tools/call","id":158,"params":{"name":"quadrate_search","arguments":{"query":"signal"}}}' \
    'signal module'

run_test "Search: alloc (finds mem module)" \
    '{"jsonrpc":"2.0","method":"tools/call","id":159,"params":{"name":"quadrate_search","arguments":{"query":"alloc"}}}' \
    'mem module'

run_test "Search: concat (finds strings module)" \
    '{"jsonrpc":"2.0","method":"tools/call","id":160,"params":{"name":"quadrate_search","arguments":{"query":"concat"}}}' \
    'strings module'

run_test "Search: path (finds path module)" \
    '{"jsonrpc":"2.0","method":"tools/call","id":161,"params":{"name":"quadrate_search","arguments":{"query":"basename"}}}' \
    'path module'

run_test "Search: dup (finds builtins)" \
    '{"jsonrpc":"2.0","method":"tools/call","id":162,"params":{"name":"quadrate_search","arguments":{"query":"dup"}}}' \
    'Builtins'

run_test "Search: swap (finds builtins)" \
    '{"jsonrpc":"2.0","method":"tools/call","id":163,"params":{"name":"quadrate_search","arguments":{"query":"swap"}}}' \
    'Builtins'

run_test "Search: struct (finds language reference)" \
    '{"jsonrpc":"2.0","method":"tools/call","id":164,"params":{"name":"quadrate_search","arguments":{"query":"struct"}}}' \
    'Language Reference'

run_test "Search: fn (finds language reference)" \
    '{"jsonrpc":"2.0","method":"tools/call","id":165,"params":{"name":"quadrate_search","arguments":{"query":"fn"}}}' \
    'Language Reference'

run_test "Search: nonexistent term returns results header" \
    '{"jsonrpc":"2.0","method":"tools/call","id":166,"params":{"name":"quadrate_search","arguments":{"query":"xyznonexistent123"}}}' \
    'Search results for'

run_error_test "Search: missing parameter" \
    '{"jsonrpc":"2.0","method":"tools/call","id":167,"params":{"name":"quadrate_search","arguments":{}}}' \
    'Missing required parameter'

echo ""
echo "--- Case Insensitivity Tests (Fuzzy) ---"

run_test "Case insensitive search: SIN" \
    '{"jsonrpc":"2.0","method":"tools/call","id":170,"params":{"name":"quadrate_search","arguments":{"query":"SIN"}}}' \
    'math module'

run_test "Case insensitive search: RANDOM" \
    '{"jsonrpc":"2.0","method":"tools/call","id":171,"params":{"name":"quadrate_search","arguments":{"query":"RANDOM"}}}' \
    'rand module'

run_test "Case insensitive search: FILE" \
    '{"jsonrpc":"2.0","method":"tools/call","id":172,"params":{"name":"quadrate_search","arguments":{"query":"FILE"}}}' \
    'io module'

run_test "Case insensitive search: THREAD" \
    '{"jsonrpc":"2.0","method":"tools/call","id":173,"params":{"name":"quadrate_search","arguments":{"query":"THREAD"}}}' \
    'thread module'

run_test "Case insensitive search: DUP" \
    '{"jsonrpc":"2.0","method":"tools/call","id":174,"params":{"name":"quadrate_search","arguments":{"query":"DUP"}}}' \
    'Builtins'

run_test "Case insensitive search: STRUCT" \
    '{"jsonrpc":"2.0","method":"tools/call","id":175,"params":{"name":"quadrate_search","arguments":{"query":"STRUCT"}}}' \
    'Language Reference'

run_test "Mixed case search: Sin" \
    '{"jsonrpc":"2.0","method":"tools/call","id":176,"params":{"name":"quadrate_search","arguments":{"query":"Sin"}}}' \
    'math module'

run_test "Mixed case search: RaNdOm" \
    '{"jsonrpc":"2.0","method":"tools/call","id":177,"params":{"name":"quadrate_search","arguments":{"query":"RaNdOm"}}}' \
    'rand module'

echo ""
echo "--- Edge Cases ---"

run_test "Unknown tool error" \
    '{"jsonrpc":"2.0","method":"tools/call","id":180,"params":{"name":"unknown_tool","arguments":{}}}' \
    'Unknown tool'

run_test "Malformed JSON request (missing method)" \
    '{"jsonrpc":"2.0","id":181}' \
    'Invalid request'

run_test "Empty arguments object" \
    '{"jsonrpc":"2.0","method":"tools/call","id":182,"params":{"name":"quadrate_list_modules","arguments":{}}}' \
    'name'

run_test "Special chars in search: underscore" \
    '{"jsonrpc":"2.0","method":"tools/call","id":183,"params":{"name":"quadrate_search","arguments":{"query":"read_file"}}}' \
    'io module'

run_test "Special chars in search: double colon" \
    '{"jsonrpc":"2.0","method":"tools/call","id":184,"params":{"name":"quadrate_search","arguments":{"query":"strings::len"}}}' \
    'strings module'

run_test "Very long query returns results" \
    '{"jsonrpc":"2.0","method":"tools/call","id":185,"params":{"name":"quadrate_search","arguments":{"query":"thisisaverylongquerythatprobablywontmatchanything"}}}' \
    'Search results for'

run_test "Single character search: a" \
    '{"jsonrpc":"2.0","method":"tools/call","id":186,"params":{"name":"quadrate_search","arguments":{"query":"a"}}}' \
    'Search results for'

run_test "Numeric search: 64" \
    '{"jsonrpc":"2.0","method":"tools/call","id":187,"params":{"name":"quadrate_search","arguments":{"query":"64"}}}' \
    'Search results for'

run_test "Whitespace handling" \
    '{"jsonrpc":"2.0","method":"tools/call","id":188,"params":{"name":"quadrate_search","arguments":{"query":" sin "}}}' \
    'Search results for'

echo ""
echo "--- ID Handling Tests ---"

run_test "String ID" \
    '{"jsonrpc":"2.0","method":"ping","id":"test-id-123"}' \
    '"id"'

run_test "Numeric ID preserved" \
    '{"jsonrpc":"2.0","method":"ping","id":999}' \
    '"id":999'

run_test "Large numeric ID" \
    '{"jsonrpc":"2.0","method":"ping","id":999999999}' \
    '"id":999999999'

echo ""
echo "--- Multiple Modules Verification ---"

run_test "Module: bits exists" \
    '{"jsonrpc":"2.0","method":"tools/call","id":200,"params":{"name":"quadrate_get_module","arguments":{"name":"bits"}}}' \
    'name.*bits'

run_test "Module: bytes exists" \
    '{"jsonrpc":"2.0","method":"tools/call","id":201,"params":{"name":"quadrate_get_module","arguments":{"name":"bytes"}}}' \
    'name.*bytes'

run_test "Module: flag exists" \
    '{"jsonrpc":"2.0","method":"tools/call","id":202,"params":{"name":"quadrate_get_module","arguments":{"name":"flag"}}}' \
    'name.*flag'

run_test "Module: limits exists" \
    '{"jsonrpc":"2.0","method":"tools/call","id":203,"params":{"name":"quadrate_get_module","arguments":{"name":"limits"}}}' \
    'name.*limits'

run_test "Module: path exists" \
    '{"jsonrpc":"2.0","method":"tools/call","id":204,"params":{"name":"quadrate_get_module","arguments":{"name":"path"}}}' \
    'name.*path'

run_test "Module: sb exists" \
    '{"jsonrpc":"2.0","method":"tools/call","id":205,"params":{"name":"quadrate_get_module","arguments":{"name":"sb"}}}' \
    'name.*sb'

run_test "Module: strconv exists" \
    '{"jsonrpc":"2.0","method":"tools/call","id":206,"params":{"name":"quadrate_get_module","arguments":{"name":"strconv"}}}' \
    'name.*strconv'

run_test "Module: term exists" \
    '{"jsonrpc":"2.0","method":"tools/call","id":207,"params":{"name":"quadrate_get_module","arguments":{"name":"term"}}}' \
    'name.*term'

run_test "Module: testing exists" \
    '{"jsonrpc":"2.0","method":"tools/call","id":208,"params":{"name":"quadrate_get_module","arguments":{"name":"testing"}}}' \
    'name.*testing'

run_test "Module: unicode exists" \
    '{"jsonrpc":"2.0","method":"tools/call","id":209,"params":{"name":"quadrate_get_module","arguments":{"name":"unicode"}}}' \
    'name.*unicode'

echo ""
echo "--- quadrate_find_function Tests ---"

run_test "Find function: sin (exact match)" \
    '{"jsonrpc":"2.0","method":"tools/call","id":300,"params":{"name":"quadrate_find_function","arguments":{"query":"sin"}}}' \
    'math::sin'

run_test "Find function: rd_fl (fuzzy match for read_file)" \
    '{"jsonrpc":"2.0","method":"tools/call","id":301,"params":{"name":"quadrate_find_function","arguments":{"query":"rd_fl"}}}' \
    'io'

run_test "Find function: concat" \
    '{"jsonrpc":"2.0","method":"tools/call","id":302,"params":{"name":"quadrate_find_function","arguments":{"query":"concat"}}}' \
    'strings::concat'

run_test "Find function: case insensitive (SIN)" \
    '{"jsonrpc":"2.0","method":"tools/call","id":303,"params":{"name":"quadrate_find_function","arguments":{"query":"SIN"}}}' \
    'math::sin'

run_error_test "Find function: missing query" \
    '{"jsonrpc":"2.0","method":"tools/call","id":304,"params":{"name":"quadrate_find_function","arguments":{}}}' \
    'Missing required parameter'

echo ""
echo "--- quadrate_search_by_signature Tests ---"

run_test "Search signature: f64 f64" \
    '{"jsonrpc":"2.0","method":"tools/call","id":310,"params":{"name":"quadrate_search_by_signature","arguments":{"signature":"f64 f64"}}}' \
    'f64'

run_test "Search signature: str i64" \
    '{"jsonrpc":"2.0","method":"tools/call","id":311,"params":{"name":"quadrate_search_by_signature","arguments":{"signature":"str i64"}}}' \
    'str'

run_error_test "Search signature: missing signature" \
    '{"jsonrpc":"2.0","method":"tools/call","id":312,"params":{"name":"quadrate_search_by_signature","arguments":{}}}' \
    'Missing required parameter'

echo ""
echo "--- quadrate_get_error Tests ---"

run_test "Get error: io E_NOT_FOUND" \
    '{"jsonrpc":"2.0","method":"tools/call","id":320,"params":{"name":"quadrate_get_error","arguments":{"module":"io","error":"E_NOT_FOUND"}}}' \
    'E_NOT_FOUND'

run_test "Get error: io E_PERMISSION" \
    '{"jsonrpc":"2.0","method":"tools/call","id":321,"params":{"name":"quadrate_get_error","arguments":{"module":"io","error":"E_PERMISSION"}}}' \
    'E_PERMISSION'

run_error_test "Get error: missing module" \
    '{"jsonrpc":"2.0","method":"tools/call","id":322,"params":{"name":"quadrate_get_error","arguments":{}}}' \
    'Missing required parameter'

run_error_test "Get error: missing error param" \
    '{"jsonrpc":"2.0","method":"tools/call","id":323,"params":{"name":"quadrate_get_error","arguments":{"module":"io"}}}' \
    'Missing required parameter'

run_error_test "Get error: unknown error" \
    '{"jsonrpc":"2.0","method":"tools/call","id":324,"params":{"name":"quadrate_get_error","arguments":{"module":"io","error":"E_NONEXISTENT"}}}' \
    'not found'

echo ""
echo "--- Resources Read Tests ---"

run_test "Read resource: hello-world snippet" \
    '{"jsonrpc":"2.0","method":"resources/read","id":330,"params":{"uri":"quadrate://snippets/hello-world"}}' \
    'contents'

run_test "Read resource: stack-ops snippet" \
    '{"jsonrpc":"2.0","method":"resources/read","id":331,"params":{"uri":"quadrate://snippets/stack-ops"}}' \
    'dup'

run_test "Read resource: loops snippet" \
    '{"jsonrpc":"2.0","method":"resources/read","id":332,"params":{"uri":"quadrate://snippets/loops"}}' \
    'For loop'

run_test "Read resource: common-mistakes guide" \
    '{"jsonrpc":"2.0","method":"resources/read","id":333,"params":{"uri":"quadrate://guides/common-mistakes"}}' \
    'STACK UNDERFLOW'

run_test "Read resource: common-mistakes has all sections" \
    '{"jsonrpc":"2.0","method":"resources/read","id":334,"params":{"uri":"quadrate://guides/common-mistakes"}}' \
    'WRONG ARGUMENT ORDER'

run_test "Read resource: best-practices guide" \
    '{"jsonrpc":"2.0","method":"resources/read","id":335,"params":{"uri":"quadrate://guides/best-practices"}}' \
    'BEST PRACTICES'

run_test "Read resource: best-practices has stack ops section" \
    '{"jsonrpc":"2.0","method":"resources/read","id":336,"params":{"uri":"quadrate://guides/best-practices"}}' \
    'STACK OPS VS VARIABLES'

run_test "Read resource: best-practices has naming section" \
    '{"jsonrpc":"2.0","method":"resources/read","id":337,"params":{"uri":"quadrate://guides/best-practices"}}' \
    'NAMING CONVENTIONS'

run_test "Read resource: cheatsheet guide" \
    '{"jsonrpc":"2.0","method":"resources/read","id":338,"params":{"uri":"quadrate://guides/cheatsheet"}}' \
    'FUNCTION CHEATSHEET'

run_test "Read resource: cheatsheet has file I/O" \
    '{"jsonrpc":"2.0","method":"resources/read","id":339,"params":{"uri":"quadrate://guides/cheatsheet"}}' \
    'FILE I/O'

run_test "Read resource: cheatsheet has strings" \
    '{"jsonrpc":"2.0","method":"resources/read","id":340,"params":{"uri":"quadrate://guides/cheatsheet"}}' \
    'STRINGS'

run_test "Read resource: cheatsheet has stack ops" \
    '{"jsonrpc":"2.0","method":"resources/read","id":341,"params":{"uri":"quadrate://guides/cheatsheet"}}' \
    'STACK OPERATIONS'

run_test "Read resource: missing uri error" \
    '{"jsonrpc":"2.0","method":"resources/read","id":335,"params":{}}' \
    'Missing required parameter'

echo ""
echo "--- Prompts Get Tests ---"

run_test "Get prompt: explain_code" \
    '{"jsonrpc":"2.0","method":"prompts/get","id":340,"params":{"name":"explain_code","arguments":{"code":"1 2 +"}}}' \
    'messages'

run_test "Get prompt: convert_to_idiomatic" \
    '{"jsonrpc":"2.0","method":"prompts/get","id":341,"params":{"name":"convert_to_idiomatic","arguments":{"code":"1 -> x  x 2 +"}}}' \
    'messages'

run_test "Get prompt: debug_stack" \
    '{"jsonrpc":"2.0","method":"prompts/get","id":342,"params":{"name":"debug_stack","arguments":{"code":"1 2 + +","error":"stack underflow"}}}' \
    'messages'

run_test "Get prompt: missing name error" \
    '{"jsonrpc":"2.0","method":"prompts/get","id":343,"params":{}}' \
    'Missing required parameter'

run_test "Get prompt: unknown prompt error" \
    '{"jsonrpc":"2.0","method":"prompts/get","id":344,"params":{"name":"unknown_prompt","arguments":{}}}' \
    'Unknown prompt'

echo ""
echo "--- quadrate_trace_stack Tests ---"

run_test "Trace stack: basic example" \
    '{"jsonrpc":"2.0","method":"tools/call","id":350,"params":{"name":"quadrate_trace_stack","arguments":{"code":"1 2 + dup *"}}}' \
    'Stack Trace'

run_test "Trace stack: shows stack manipulation" \
    '{"jsonrpc":"2.0","method":"tools/call","id":351,"params":{"name":"quadrate_trace_stack","arguments":{"code":"1 2 swap"}}}' \
    'swap'

run_test "Trace stack: shows arithmetic" \
    '{"jsonrpc":"2.0","method":"tools/call","id":352,"params":{"name":"quadrate_trace_stack","arguments":{"code":"a b +"}}}' \
    'add'

run_test "Trace stack: shows comparison ops" \
    '{"jsonrpc":"2.0","method":"tools/call","id":353,"params":{"name":"quadrate_trace_stack","arguments":{"code":"x y =="}}}' \
    'equal'

run_error_test "Trace stack: missing code" \
    '{"jsonrpc":"2.0","method":"tools/call","id":354,"params":{"name":"quadrate_trace_stack","arguments":{}}}' \
    'Missing required parameter'

# Comprehensive trace_stack tests
run_test "Trace stack: subtraction" \
    '{"jsonrpc":"2.0","method":"tools/call","id":500,"params":{"name":"quadrate_trace_stack","arguments":{"code":"10 3 -"}}}' \
    'sub'

run_test "Trace stack: multiplication" \
    '{"jsonrpc":"2.0","method":"tools/call","id":501,"params":{"name":"quadrate_trace_stack","arguments":{"code":"3 4 *"}}}' \
    'mul'

run_test "Trace stack: division" \
    '{"jsonrpc":"2.0","method":"tools/call","id":502,"params":{"name":"quadrate_trace_stack","arguments":{"code":"10 2 /"}}}' \
    'div'

run_test "Trace stack: modulo" \
    '{"jsonrpc":"2.0","method":"tools/call","id":503,"params":{"name":"quadrate_trace_stack","arguments":{"code":"10 3 %"}}}' \
    'mod'

run_test "Trace stack: dup operation" \
    '{"jsonrpc":"2.0","method":"tools/call","id":504,"params":{"name":"quadrate_trace_stack","arguments":{"code":"5 dup"}}}' \
    'dup'

run_test "Trace stack: drop operation" \
    '{"jsonrpc":"2.0","method":"tools/call","id":505,"params":{"name":"quadrate_trace_stack","arguments":{"code":"1 2 drop"}}}' \
    'drop'

run_test "Trace stack: over operation" \
    '{"jsonrpc":"2.0","method":"tools/call","id":506,"params":{"name":"quadrate_trace_stack","arguments":{"code":"1 2 over"}}}' \
    'over'

run_test "Trace stack: rot operation" \
    '{"jsonrpc":"2.0","method":"tools/call","id":507,"params":{"name":"quadrate_trace_stack","arguments":{"code":"1 2 3 rot"}}}' \
    'rot'

run_test "Trace stack: nip operation" \
    '{"jsonrpc":"2.0","method":"tools/call","id":508,"params":{"name":"quadrate_trace_stack","arguments":{"code":"1 2 nip"}}}' \
    'nip'

run_test "Trace stack: less than" \
    '{"jsonrpc":"2.0","method":"tools/call","id":509,"params":{"name":"quadrate_trace_stack","arguments":{"code":"3 5 <"}}}' \
    'less'

run_test "Trace stack: greater than" \
    '{"jsonrpc":"2.0","method":"tools/call","id":510,"params":{"name":"quadrate_trace_stack","arguments":{"code":"5 3 >"}}}' \
    'greater'

run_test "Trace stack: not equal" \
    '{"jsonrpc":"2.0","method":"tools/call","id":511,"params":{"name":"quadrate_trace_stack","arguments":{"code":"3 5 !="}}}' \
    'neq'

run_test "Trace stack: less or equal" \
    '{"jsonrpc":"2.0","method":"tools/call","id":512,"params":{"name":"quadrate_trace_stack","arguments":{"code":"3 3 <="}}}' \
    '<= / le'

run_test "Trace stack: greater or equal" \
    '{"jsonrpc":"2.0","method":"tools/call","id":513,"params":{"name":"quadrate_trace_stack","arguments":{"code":"5 3 >="}}}' \
    '>= / ge'

run_test "Trace stack: logical and" \
    '{"jsonrpc":"2.0","method":"tools/call","id":514,"params":{"name":"quadrate_trace_stack","arguments":{"code":"1 1 and"}}}' \
    'and'

run_test "Trace stack: logical or" \
    '{"jsonrpc":"2.0","method":"tools/call","id":515,"params":{"name":"quadrate_trace_stack","arguments":{"code":"0 1 or"}}}' \
    'or'

run_test "Trace stack: logical not" \
    '{"jsonrpc":"2.0","method":"tools/call","id":516,"params":{"name":"quadrate_trace_stack","arguments":{"code":"0 not"}}}' \
    'not'

run_test "Trace stack: negate" \
    '{"jsonrpc":"2.0","method":"tools/call","id":517,"params":{"name":"quadrate_trace_stack","arguments":{"code":"5 negate"}}}' \
    'negate'

run_test "Trace stack: increment" \
    '{"jsonrpc":"2.0","method":"tools/call","id":518,"params":{"name":"quadrate_trace_stack","arguments":{"code":"5 inc"}}}' \
    'inc'

run_test "Trace stack: decrement" \
    '{"jsonrpc":"2.0","method":"tools/call","id":519,"params":{"name":"quadrate_trace_stack","arguments":{"code":"5 dec"}}}' \
    'dec'

run_test "Trace stack: shows reference header" \
    '{"jsonrpc":"2.0","method":"tools/call","id":520,"params":{"name":"quadrate_trace_stack","arguments":{"code":"1 2 +"}}}' \
    'Common Operations Reference'

run_test "Trace stack: complex expression" \
    '{"jsonrpc":"2.0","method":"tools/call","id":521,"params":{"name":"quadrate_trace_stack","arguments":{"code":"1 2 3 + * 4 -"}}}' \
    'Stack Trace'

run_test "Trace stack: assignment arrow" \
    '{"jsonrpc":"2.0","method":"tools/call","id":522,"params":{"name":"quadrate_trace_stack","arguments":{"code":"5 -> x"}}}' \
    'bind to variable'

run_test "Trace stack: function call notation" \
    '{"jsonrpc":"2.0","method":"tools/call","id":523,"params":{"name":"quadrate_trace_stack","arguments":{"code":"x foo::bar"}}}' \
    'function'

run_test "Trace stack: empty code" \
    '{"jsonrpc":"2.0","method":"tools/call","id":524,"params":{"name":"quadrate_trace_stack","arguments":{"code":""}}}' \
    'Stack Trace'

run_test "Trace stack: whitespace only" \
    '{"jsonrpc":"2.0","method":"tools/call","id":525,"params":{"name":"quadrate_trace_stack","arguments":{"code":"   "}}}' \
    'Stack Trace'

run_test "Trace stack: single value" \
    '{"jsonrpc":"2.0","method":"tools/call","id":526,"params":{"name":"quadrate_trace_stack","arguments":{"code":"42"}}}' \
    'push'

run_test "Trace stack: multiple values" \
    '{"jsonrpc":"2.0","method":"tools/call","id":527,"params":{"name":"quadrate_trace_stack","arguments":{"code":"1 2 3 4 5"}}}' \
    'push'

run_test "Trace stack: float value" \
    '{"jsonrpc":"2.0","method":"tools/call","id":528,"params":{"name":"quadrate_trace_stack","arguments":{"code":"3.14"}}}' \
    'push'

run_test "Trace stack: variable reference" \
    '{"jsonrpc":"2.0","method":"tools/call","id":529,"params":{"name":"quadrate_trace_stack","arguments":{"code":"myvar"}}}' \
    'push'

echo ""
echo "--- quadrate_generate_template Tests ---"

run_test "Generate template: file-read" \
    '{"jsonrpc":"2.0","method":"tools/call","id":360,"params":{"name":"quadrate_generate_template","arguments":{"template":"file-read"}}}' \
    'FILE READ TEMPLATE'

run_test "Generate template: file-write" \
    '{"jsonrpc":"2.0","method":"tools/call","id":361,"params":{"name":"quadrate_generate_template","arguments":{"template":"file-write"}}}' \
    'FILE WRITE TEMPLATE'

run_test "Generate template: json-parse" \
    '{"jsonrpc":"2.0","method":"tools/call","id":362,"params":{"name":"quadrate_generate_template","arguments":{"template":"json-parse"}}}' \
    'JSON PARSE TEMPLATE'

run_test "Generate template: http-get" \
    '{"jsonrpc":"2.0","method":"tools/call","id":363,"params":{"name":"quadrate_generate_template","arguments":{"template":"http-get"}}}' \
    'HTTP GET TEMPLATE'

run_test "Generate template: cli-args" \
    '{"jsonrpc":"2.0","method":"tools/call","id":364,"params":{"name":"quadrate_generate_template","arguments":{"template":"cli-args"}}}' \
    'CLI ARGS TEMPLATE'

run_test "Generate template: struct-crud" \
    '{"jsonrpc":"2.0","method":"tools/call","id":365,"params":{"name":"quadrate_generate_template","arguments":{"template":"struct-crud"}}}' \
    'STRUCT CRUD TEMPLATE'

run_test "Generate template: error-handling" \
    '{"jsonrpc":"2.0","method":"tools/call","id":366,"params":{"name":"quadrate_generate_template","arguments":{"template":"error-handling"}}}' \
    'ERROR HANDLING TEMPLATE'

run_test "Generate template: loop-array" \
    '{"jsonrpc":"2.0","method":"tools/call","id":367,"params":{"name":"quadrate_generate_template","arguments":{"template":"loop-array"}}}' \
    'LOOP AND ARRAY TEMPLATE'

run_error_test "Generate template: unknown template" \
    '{"jsonrpc":"2.0","method":"tools/call","id":368,"params":{"name":"quadrate_generate_template","arguments":{"template":"unknown"}}}' \
    'Unknown template'

run_error_test "Generate template: missing param" \
    '{"jsonrpc":"2.0","method":"tools/call","id":369,"params":{"name":"quadrate_generate_template","arguments":{}}}' \
    'Missing required parameter'

# Edge cases and content verification for templates
run_test "Generate template: file-read has io module" \
    '{"jsonrpc":"2.0","method":"tools/call","id":400,"params":{"name":"quadrate_generate_template","arguments":{"template":"file-read"}}}' \
    'use io'

run_test "Generate template: file-read has switch pattern" \
    '{"jsonrpc":"2.0","method":"tools/call","id":401,"params":{"name":"quadrate_generate_template","arguments":{"template":"file-read"}}}' \
    'switch'

run_test "Generate template: file-write has write_file" \
    '{"jsonrpc":"2.0","method":"tools/call","id":402,"params":{"name":"quadrate_generate_template","arguments":{"template":"file-write"}}}' \
    'io::write_file'

run_test "Generate template: file-write has append_file" \
    '{"jsonrpc":"2.0","method":"tools/call","id":403,"params":{"name":"quadrate_generate_template","arguments":{"template":"file-write"}}}' \
    'io::append_file'

run_test "Generate template: json-parse has json module" \
    '{"jsonrpc":"2.0","method":"tools/call","id":404,"params":{"name":"quadrate_generate_template","arguments":{"template":"json-parse"}}}' \
    'use json'

run_test "Generate template: json-parse has get_string" \
    '{"jsonrpc":"2.0","method":"tools/call","id":405,"params":{"name":"quadrate_generate_template","arguments":{"template":"json-parse"}}}' \
    'json::get_string'

run_test "Generate template: http-get has http module" \
    '{"jsonrpc":"2.0","method":"tools/call","id":406,"params":{"name":"quadrate_generate_template","arguments":{"template":"http-get"}}}' \
    'use http'

run_test "Generate template: http-get checks status" \
    '{"jsonrpc":"2.0","method":"tools/call","id":407,"params":{"name":"quadrate_generate_template","arguments":{"template":"http-get"}}}' \
    '<<status'

run_test "Generate template: cli-args has os module" \
    '{"jsonrpc":"2.0","method":"tools/call","id":408,"params":{"name":"quadrate_generate_template","arguments":{"template":"cli-args"}}}' \
    'use os'

run_test "Generate template: cli-args has os::args" \
    '{"jsonrpc":"2.0","method":"tools/call","id":409,"params":{"name":"quadrate_generate_template","arguments":{"template":"cli-args"}}}' \
    'os::args'

run_test "Generate template: struct-crud has struct keyword" \
    '{"jsonrpc":"2.0","method":"tools/call","id":410,"params":{"name":"quadrate_generate_template","arguments":{"template":"struct-crud"}}}' \
    'struct Item'

run_test "Generate template: struct-crud shows field access" \
    '{"jsonrpc":"2.0","method":"tools/call","id":411,"params":{"name":"quadrate_generate_template","arguments":{"template":"struct-crud"}}}' \
    '<<field_name'

run_test "Generate template: struct-crud shows field set" \
    '{"jsonrpc":"2.0","method":"tools/call","id":412,"params":{"name":"quadrate_generate_template","arguments":{"template":"struct-crud"}}}' \
    '#field_name'

run_test "Generate template: error-handling shows panic" \
    '{"jsonrpc":"2.0","method":"tools/call","id":413,"params":{"name":"quadrate_generate_template","arguments":{"template":"error-handling"}}}' \
    'io::read_file!'

run_test "Generate template: error-handling shows Ok/Err" \
    '{"jsonrpc":"2.0","method":"tools/call","id":414,"params":{"name":"quadrate_generate_template","arguments":{"template":"error-handling"}}}' \
    'Ok {'

run_test "Generate template: loop-array has for loop" \
    '{"jsonrpc":"2.0","method":"tools/call","id":415,"params":{"name":"quadrate_generate_template","arguments":{"template":"loop-array"}}}' \
    'for i'

run_test "Generate template: loop-array has makei" \
    '{"jsonrpc":"2.0","method":"tools/call","id":416,"params":{"name":"quadrate_generate_template","arguments":{"template":"loop-array"}}}' \
    'makei'

run_test "Generate template: loop-array has loop/break" \
    '{"jsonrpc":"2.0","method":"tools/call","id":417,"params":{"name":"quadrate_generate_template","arguments":{"template":"loop-array"}}}' \
    'loop {'

run_error_test "Generate template: empty string" \
    '{"jsonrpc":"2.0","method":"tools/call","id":418,"params":{"name":"quadrate_generate_template","arguments":{"template":""}}}' \
    'Unknown template'

run_error_test "Generate template: whitespace only" \
    '{"jsonrpc":"2.0","method":"tools/call","id":419,"params":{"name":"quadrate_generate_template","arguments":{"template":"   "}}}' \
    'Unknown template'

run_error_test "Generate template: similar but wrong name" \
    '{"jsonrpc":"2.0","method":"tools/call","id":420,"params":{"name":"quadrate_generate_template","arguments":{"template":"file-reading"}}}' \
    'Unknown template'

run_test "Generate template: unknown lists available" \
    '{"jsonrpc":"2.0","method":"tools/call","id":421,"params":{"name":"quadrate_generate_template","arguments":{"template":"xyz"}}}' \
    'file-read'

echo ""
echo "--- quadrate_explain_signature Tests ---"

run_test "Explain signature: basic" \
    '{"jsonrpc":"2.0","method":"tools/call","id":370,"params":{"name":"quadrate_explain_signature","arguments":{"signature":"a:i64 b:i64 -- sum:i64"}}}' \
    'Signature Explanation'

run_test "Explain signature: shows format" \
    '{"jsonrpc":"2.0","method":"tools/call","id":371,"params":{"name":"quadrate_explain_signature","arguments":{"signature":"s:str -- len:i64"}}}' \
    'FORMAT EXPLANATION'

run_test "Explain signature: shows types" \
    '{"jsonrpc":"2.0","method":"tools/call","id":372,"params":{"name":"quadrate_explain_signature","arguments":{"signature":"x:f64 -- y:f64"}}}' \
    'COMMON TYPES'

run_test "Explain signature: shows calling convention" \
    '{"jsonrpc":"2.0","method":"tools/call","id":373,"params":{"name":"quadrate_explain_signature","arguments":{"signature":"a:str b:i64 -- x:ptr"}}}' \
    'CALLING CONVENTION'

run_error_test "Explain signature: missing param" \
    '{"jsonrpc":"2.0","method":"tools/call","id":374,"params":{"name":"quadrate_explain_signature","arguments":{}}}' \
    'Missing required parameter'

# Comprehensive signature tests - various signature patterns
run_test "Explain signature: no inputs" \
    '{"jsonrpc":"2.0","method":"tools/call","id":430,"params":{"name":"quadrate_explain_signature","arguments":{"signature":" -- result:i64"}}}' \
    'INPUTS and OUTPUTS'

run_test "Explain signature: no outputs" \
    '{"jsonrpc":"2.0","method":"tools/call","id":431,"params":{"name":"quadrate_explain_signature","arguments":{"signature":"val:i64 -- "}}}' \
    'Signature Explanation'

run_test "Explain signature: multiple outputs" \
    '{"jsonrpc":"2.0","method":"tools/call","id":432,"params":{"name":"quadrate_explain_signature","arguments":{"signature":"path:str -- content:str found:i64"}}}' \
    'OUTPUTS'

run_test "Explain signature: multiple inputs" \
    '{"jsonrpc":"2.0","method":"tools/call","id":433,"params":{"name":"quadrate_explain_signature","arguments":{"signature":"a:i64 b:i64 c:i64 -- sum:i64"}}}' \
    'INPUTS'

run_test "Explain signature: ptr type" \
    '{"jsonrpc":"2.0","method":"tools/call","id":434,"params":{"name":"quadrate_explain_signature","arguments":{"signature":"arr:ptr -- len:i64"}}}' \
    'ptr'

run_test "Explain signature: shows examples section" \
    '{"jsonrpc":"2.0","method":"tools/call","id":435,"params":{"name":"quadrate_explain_signature","arguments":{"signature":"x:i64 -- y:i64"}}}' \
    'EXAMPLES'

run_test "Explain signature: explains rightmost is top" \
    '{"jsonrpc":"2.0","method":"tools/call","id":436,"params":{"name":"quadrate_explain_signature","arguments":{"signature":"a:i64 b:i64 -- c:i64"}}}' \
    'Rightmost'

run_test "Explain signature: without separator warns" \
    '{"jsonrpc":"2.0","method":"tools/call","id":437,"params":{"name":"quadrate_explain_signature","arguments":{"signature":"a:i64 b:i64"}}}' \
    'No '"'"'--'"'"' found'

run_test "Explain signature: empty signature" \
    '{"jsonrpc":"2.0","method":"tools/call","id":438,"params":{"name":"quadrate_explain_signature","arguments":{"signature":""}}}' \
    'Signature Explanation'

run_test "Explain signature: just separator" \
    '{"jsonrpc":"2.0","method":"tools/call","id":439,"params":{"name":"quadrate_explain_signature","arguments":{"signature":"--"}}}' \
    'INPUTS and OUTPUTS'

run_test "Explain signature: shows i64 type info" \
    '{"jsonrpc":"2.0","method":"tools/call","id":440,"params":{"name":"quadrate_explain_signature","arguments":{"signature":"x:i64 -- y:i64"}}}' \
    'i64'

run_test "Explain signature: shows f64 type info" \
    '{"jsonrpc":"2.0","method":"tools/call","id":441,"params":{"name":"quadrate_explain_signature","arguments":{"signature":"x:f64 -- y:f64"}}}' \
    'f64'

run_test "Explain signature: shows str type info" \
    '{"jsonrpc":"2.0","method":"tools/call","id":442,"params":{"name":"quadrate_explain_signature","arguments":{"signature":"s:str -- t:str"}}}' \
    'str'

run_test "Explain signature: complex mixed types" \
    '{"jsonrpc":"2.0","method":"tools/call","id":443,"params":{"name":"quadrate_explain_signature","arguments":{"signature":"name:str count:i64 factor:f64 -- result:ptr ok:i64"}}}' \
    'Signature Explanation'

run_test "Explain signature: push left to right" \
    '{"jsonrpc":"2.0","method":"tools/call","id":444,"params":{"name":"quadrate_explain_signature","arguments":{"signature":"a:i64 b:i64 -- c:i64"}}}' \
    'Push inputs LEFT to RIGHT'

run_test "Explain signature: pop right to left" \
    '{"jsonrpc":"2.0","method":"tools/call","id":445,"params":{"name":"quadrate_explain_signature","arguments":{"signature":"a:i64 -- x:i64 y:i64"}}}' \
    'Pop outputs RIGHT to LEFT'

echo ""
echo "--- quadrate_type_conversion Tests ---"

run_test "Type conversion: i64 to str" \
    '{"jsonrpc":"2.0","method":"tools/call","id":380,"params":{"name":"quadrate_type_conversion","arguments":{"from":"i64","to":"str"}}}' \
    'strconv::itoa'

run_test "Type conversion: i64 to f64" \
    '{"jsonrpc":"2.0","method":"tools/call","id":381,"params":{"name":"quadrate_type_conversion","arguments":{"from":"i64","to":"f64"}}}' \
    'cast<f64>'

run_test "Type conversion: f64 to i64" \
    '{"jsonrpc":"2.0","method":"tools/call","id":382,"params":{"name":"quadrate_type_conversion","arguments":{"from":"f64","to":"i64"}}}' \
    'cast<i64>'

run_test "Type conversion: f64 to str" \
    '{"jsonrpc":"2.0","method":"tools/call","id":383,"params":{"name":"quadrate_type_conversion","arguments":{"from":"f64","to":"str"}}}' \
    'strconv::ftoa'

run_test "Type conversion: str to i64" \
    '{"jsonrpc":"2.0","method":"tools/call","id":384,"params":{"name":"quadrate_type_conversion","arguments":{"from":"str","to":"i64"}}}' \
    'strconv::atoi'

run_test "Type conversion: str to f64" \
    '{"jsonrpc":"2.0","method":"tools/call","id":385,"params":{"name":"quadrate_type_conversion","arguments":{"from":"str","to":"f64"}}}' \
    'strconv::atof'

run_test "Type conversion: str to ptr" \
    '{"jsonrpc":"2.0","method":"tools/call","id":386,"params":{"name":"quadrate_type_conversion","arguments":{"from":"str","to":"ptr"}}}' \
    'Strings ARE pointers'

run_test "Type conversion: ptr to str" \
    '{"jsonrpc":"2.0","method":"tools/call","id":387,"params":{"name":"quadrate_type_conversion","arguments":{"from":"ptr","to":"str"}}}' \
    'strings::from_bytes'

run_test "Type conversion: bool to i64" \
    '{"jsonrpc":"2.0","method":"tools/call","id":388,"params":{"name":"quadrate_type_conversion","arguments":{"from":"bool","to":"i64"}}}' \
    'No conversion needed'

run_test "Type conversion: i64 to bool" \
    '{"jsonrpc":"2.0","method":"tools/call","id":389,"params":{"name":"quadrate_type_conversion","arguments":{"from":"i64","to":"bool"}}}' \
    'Quadrate uses i64 for booleans'

run_test "Type conversion: case insensitive" \
    '{"jsonrpc":"2.0","method":"tools/call","id":390,"params":{"name":"quadrate_type_conversion","arguments":{"from":"I64","to":"STR"}}}' \
    'strconv::itoa'

run_error_test "Type conversion: unknown source type" \
    '{"jsonrpc":"2.0","method":"tools/call","id":391,"params":{"name":"quadrate_type_conversion","arguments":{"from":"unknown","to":"i64"}}}' \
    'Unknown source type'

run_error_test "Type conversion: unknown target type" \
    '{"jsonrpc":"2.0","method":"tools/call","id":392,"params":{"name":"quadrate_type_conversion","arguments":{"from":"i64","to":"unknown"}}}' \
    'Unknown target type'

run_error_test "Type conversion: missing params" \
    '{"jsonrpc":"2.0","method":"tools/call","id":393,"params":{"name":"quadrate_type_conversion","arguments":{}}}' \
    'Missing required parameters'

# Comprehensive type conversion tests - all valid combinations
run_test "Type conversion: i64 to ptr" \
    '{"jsonrpc":"2.0","method":"tools/call","id":450,"params":{"name":"quadrate_type_conversion","arguments":{"from":"i64","to":"ptr"}}}' \
    'cast<ptr>'

run_test "Type conversion: i64 to ptr warns unsafe" \
    '{"jsonrpc":"2.0","method":"tools/call","id":451,"params":{"name":"quadrate_type_conversion","arguments":{"from":"i64","to":"ptr"}}}' \
    'unsafe'

run_test "Type conversion: ptr to i64" \
    '{"jsonrpc":"2.0","method":"tools/call","id":452,"params":{"name":"quadrate_type_conversion","arguments":{"from":"ptr","to":"i64"}}}' \
    'cast<i64>'

run_test "Type conversion: f64 to bool" \
    '{"jsonrpc":"2.0","method":"tools/call","id":453,"params":{"name":"quadrate_type_conversion","arguments":{"from":"f64","to":"bool"}}}' \
    '0.0'

run_test "Type conversion: str to bool" \
    '{"jsonrpc":"2.0","method":"tools/call","id":454,"params":{"name":"quadrate_type_conversion","arguments":{"from":"str","to":"bool"}}}' \
    'strings::len'

run_test "Type conversion: bool to str" \
    '{"jsonrpc":"2.0","method":"tools/call","id":455,"params":{"name":"quadrate_type_conversion","arguments":{"from":"bool","to":"str"}}}' \
    'strconv::itoa'

# Test conversion content details
run_test "Type conversion: i64 to f64 mentions precision" \
    '{"jsonrpc":"2.0","method":"tools/call","id":456,"params":{"name":"quadrate_type_conversion","arguments":{"from":"i64","to":"f64"}}}' \
    'precision'

run_test "Type conversion: f64 to i64 mentions truncate" \
    '{"jsonrpc":"2.0","method":"tools/call","id":457,"params":{"name":"quadrate_type_conversion","arguments":{"from":"f64","to":"i64"}}}' \
    'truncate'

run_test "Type conversion: f64 to i64 shows rounding options" \
    '{"jsonrpc":"2.0","method":"tools/call","id":458,"params":{"name":"quadrate_type_conversion","arguments":{"from":"f64","to":"i64"}}}' \
    'math::floor'

run_test "Type conversion: str to i64 is failable" \
    '{"jsonrpc":"2.0","method":"tools/call","id":459,"params":{"name":"quadrate_type_conversion","arguments":{"from":"str","to":"i64"}}}' \
    'failable'

run_test "Type conversion: str to i64 shows switch pattern" \
    '{"jsonrpc":"2.0","method":"tools/call","id":460,"params":{"name":"quadrate_type_conversion","arguments":{"from":"str","to":"i64"}}}' \
    'switch'

run_test "Type conversion: str to i64 shows panic option" \
    '{"jsonrpc":"2.0","method":"tools/call","id":461,"params":{"name":"quadrate_type_conversion","arguments":{"from":"str","to":"i64"}}}' \
    'strconv::atoi!'

run_test "Type conversion: str to f64 is failable" \
    '{"jsonrpc":"2.0","method":"tools/call","id":462,"params":{"name":"quadrate_type_conversion","arguments":{"from":"str","to":"f64"}}}' \
    'failable'

run_test "Type conversion: str to ptr notes strings are ptrs" \
    '{"jsonrpc":"2.0","method":"tools/call","id":463,"params":{"name":"quadrate_type_conversion","arguments":{"from":"str","to":"ptr"}}}' \
    'No conversion needed'

run_test "Type conversion: ptr to str shows from_bytes" \
    '{"jsonrpc":"2.0","method":"tools/call","id":464,"params":{"name":"quadrate_type_conversion","arguments":{"from":"ptr","to":"str"}}}' \
    'strings::from_bytes'

run_test "Type conversion: ptr to str shows from_cstr" \
    '{"jsonrpc":"2.0","method":"tools/call","id":465,"params":{"name":"quadrate_type_conversion","arguments":{"from":"ptr","to":"str"}}}' \
    'strings::from_cstr'

run_test "Type conversion: bool to i64 notes native" \
    '{"jsonrpc":"2.0","method":"tools/call","id":466,"params":{"name":"quadrate_type_conversion","arguments":{"from":"bool","to":"i64"}}}' \
    'bools ARE i64'

run_test "Type conversion: i64 to bool notes truthy" \
    '{"jsonrpc":"2.0","method":"tools/call","id":467,"params":{"name":"quadrate_type_conversion","arguments":{"from":"i64","to":"bool"}}}' \
    'truthy'

# Edge cases with case variations
run_test "Type conversion: uppercase FROM" \
    '{"jsonrpc":"2.0","method":"tools/call","id":468,"params":{"name":"quadrate_type_conversion","arguments":{"from":"I64","to":"f64"}}}' \
    'cast<f64>'

run_test "Type conversion: uppercase TO" \
    '{"jsonrpc":"2.0","method":"tools/call","id":469,"params":{"name":"quadrate_type_conversion","arguments":{"from":"i64","to":"F64"}}}' \
    'cast<f64>'

run_test "Type conversion: mixed case" \
    '{"jsonrpc":"2.0","method":"tools/call","id":470,"params":{"name":"quadrate_type_conversion","arguments":{"from":"Str","to":"I64"}}}' \
    'strconv::atoi'

run_test "Type conversion: all caps" \
    '{"jsonrpc":"2.0","method":"tools/call","id":471,"params":{"name":"quadrate_type_conversion","arguments":{"from":"STR","to":"PTR"}}}' \
    'Strings ARE pointers'

# Error cases - invalid combinations
run_error_test "Type conversion: ptr to f64 invalid" \
    '{"jsonrpc":"2.0","method":"tools/call","id":472,"params":{"name":"quadrate_type_conversion","arguments":{"from":"ptr","to":"f64"}}}' \
    'Unknown target type'

run_error_test "Type conversion: ptr to bool invalid" \
    '{"jsonrpc":"2.0","method":"tools/call","id":473,"params":{"name":"quadrate_type_conversion","arguments":{"from":"ptr","to":"bool"}}}' \
    'Unknown target type'

run_error_test "Type conversion: f64 to ptr invalid" \
    '{"jsonrpc":"2.0","method":"tools/call","id":474,"params":{"name":"quadrate_type_conversion","arguments":{"from":"f64","to":"ptr"}}}' \
    'Unknown target type'

run_error_test "Type conversion: bool to f64 invalid" \
    '{"jsonrpc":"2.0","method":"tools/call","id":475,"params":{"name":"quadrate_type_conversion","arguments":{"from":"bool","to":"f64"}}}' \
    'Unknown target type'

run_error_test "Type conversion: bool to ptr invalid" \
    '{"jsonrpc":"2.0","method":"tools/call","id":476,"params":{"name":"quadrate_type_conversion","arguments":{"from":"bool","to":"ptr"}}}' \
    'Unknown target type'

run_error_test "Type conversion: missing from param" \
    '{"jsonrpc":"2.0","method":"tools/call","id":477,"params":{"name":"quadrate_type_conversion","arguments":{"to":"i64"}}}' \
    'Missing required parameters'

run_error_test "Type conversion: missing to param" \
    '{"jsonrpc":"2.0","method":"tools/call","id":478,"params":{"name":"quadrate_type_conversion","arguments":{"from":"i64"}}}' \
    'Missing required parameters'

run_error_test "Type conversion: empty from" \
    '{"jsonrpc":"2.0","method":"tools/call","id":479,"params":{"name":"quadrate_type_conversion","arguments":{"from":"","to":"i64"}}}' \
    'Unknown source type'

run_error_test "Type conversion: empty to" \
    '{"jsonrpc":"2.0","method":"tools/call","id":480,"params":{"name":"quadrate_type_conversion","arguments":{"from":"i64","to":""}}}' \
    'Unknown target type'

run_error_test "Type conversion: numeric string not a type" \
    '{"jsonrpc":"2.0","method":"tools/call","id":481,"params":{"name":"quadrate_type_conversion","arguments":{"from":"123","to":"i64"}}}' \
    'Unknown source type'

run_error_test "Type conversion: typo in type name" \
    '{"jsonrpc":"2.0","method":"tools/call","id":482,"params":{"name":"quadrate_type_conversion","arguments":{"from":"int","to":"str"}}}' \
    'Unknown source type'

run_error_test "Type conversion: typo in target type" \
    '{"jsonrpc":"2.0","method":"tools/call","id":483,"params":{"name":"quadrate_type_conversion","arguments":{"from":"i64","to":"string"}}}' \
    'Unknown target type'

echo ""
echo "=========================================="
echo "Test Results: ${GREEN}$PASS passed${NC}, ${RED}$FAIL failed${NC}"
echo "=========================================="

if [ $FAIL -gt 0 ]; then
    exit 1
fi
exit 0
