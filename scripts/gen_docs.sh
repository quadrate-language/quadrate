#!/bin/bash
#
# Generate mkdocs markdown from documented Quadrate module files.
#
# Parses /// doc comments with @param, @return, @example, @error, @field tags
# and generates structured markdown documentation.
#
# Also generates the language reference from lib/qc/include/qc/reference.def.
#
# Usage:
#     ./scripts/gen_docs.sh                           # Generate all docs
#     ./scripts/gen_docs.sh lib/qdmath/qd/math/module.qd  # Single file
#

set -euo pipefail

# Find project root (directory containing this script's parent)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Output directories
DOCS_DIR="$PROJECT_ROOT/docs/docs/docs/stdlib"
JSON_DIR="$PROJECT_ROOT/docs/api"

# Standard library modules
declare -A STDLIB_MODULES=(
    ["base64"]="lib/qdbase64/qd/base64/module.qd"
    ["bits"]="lib/qdbits/qd/bits/module.qd"
    ["bytes"]="lib/qdbytes/qd/bytes/module.qd"
    ["crc32"]="lib/qdcrc32/qd/crc32/module.qd"
    ["flag"]="lib/qdflag/qd/flag/module.qd"
    ["fmt"]="lib/qdfmt/qd/fmt/module.qd"
    ["hex"]="lib/qdhex/qd/hex/module.qd"
    ["hof"]="lib/qdhof/qd/hof/module.qd"
    ["io"]="lib/qdio/qd/io/module.qd"
    ["json"]="lib/qdjson/qd/json/module.qd"
    ["limits"]="lib/qdlimits/qd/limits/module.qd"
    ["math"]="lib/qdmath/qd/math/module.qd"
    ["mem"]="lib/qdmem/qd/mem/module.qd"
    ["net"]="lib/qdnet/qd/net/module.qd"
    ["os"]="lib/qdos/qd/os/module.qd"
    ["path"]="lib/qdpath/qd/path/module.qd"
    ["rand"]="lib/qdrand/qd/rand/module.qd"
    ["regex"]="lib/qdregex/qd/regex/module.qd"
    ["sb"]="lib/qdsb/qd/sb/module.qd"
    ["sha256"]="lib/qdsha256/qd/sha256/module.qd"
    ["signal"]="lib/qdsignal/qd/signal/module.qd"
    ["sort"]="lib/qdsort/qd/sort/module.qd"
    ["str"]="lib/qdstr/qd/str/module.qd"
    ["strconv"]="lib/qdstrconv/qd/strconv/module.qd"
    ["testing"]="lib/qdtesting/qd/testing/module.qd"
    ["time"]="lib/qdtime/qd/time/module.qd"
    ["unicode"]="lib/qdunicode/qd/unicode/module.qd"
    ["uri"]="lib/qduri/qd/uri/module.qd"
    ["uuid"]="lib/qduuid/qd/uuid/module.qd"
)

# Get module name from file path
get_module_name() {
    local filepath="$1"
    # Extract module name from path like lib/qdmath/qd/math/module.qd -> math
    echo "$filepath" | sed -n 's|.*/qd/\([^/]*\)/module\.qd|\1|p'
}

# Escape special characters for JSON
json_escape() {
    local s="$1"
    s="${s//\\/\\\\}"
    s="${s//\"/\\\"}"
    s="${s//$'\n'/\\n}"
    s="${s//$'\t'/\\t}"
    echo "$s"
}

# Expand use *.qd includes and concatenate all content
expand_includes() {
    local filepath="$1"
    local dir
    dir=$(dirname "$filepath")

    while IFS= read -r line || [[ -n "$line" ]]; do
        local trimmed="${line#"${line%%[![:space:]]*}"}"
        trimmed="${trimmed%"${trimmed##*[![:space:]]}"}"

        # Handle use *.qd includes
        if [[ "$trimmed" =~ ^use[[:space:]]+([a-zA-Z0-9_]+\.qd) ]]; then
            local include_file="$dir/${BASH_REMATCH[1]}"
            if [[ -f "$include_file" ]]; then
                expand_includes "$include_file"
            fi
        else
            echo "$line"
        fi
    done < "$filepath"
}

# Parse a single module file and generate markdown + JSON
parse_module() {
    local filepath="$1"
    local module_name
    module_name=$(get_module_name "$filepath")

    if [[ -z "$module_name" ]]; then
        module_name=$(basename "$filepath" .qd)
    fi

    # Arrays to hold parsed items
    local -a module_desc=()
    local -a constants=()      # "name|value|description"
    local -a structs=()        # "name|description|field1:type1:desc1,field2:type2:desc2"
    local -a functions=()      # "name|signature|failable|description|params|returns|errors|examples"

    # Current doc buffer
    local -a doc_buffer=()
    local module_doc_found=0
    local in_struct=0
    local struct_name=""
    local struct_desc=""
    local struct_fields=""
    local struct_doc_fields=""

    # Expand includes and parse the combined content
    while IFS= read -r line || [[ -n "$line" ]]; do
        local trimmed="${line#"${line%%[![:space:]]*}"}"
        trimmed="${trimmed%"${trimmed##*[![:space:]]}"}"

        # Collect doc comments
        if [[ "$trimmed" =~ ^/// ]]; then
            local doc_text="${trimmed#///}"
            doc_text="${doc_text# }"
            doc_buffer+=("$doc_text")
            continue
        fi

        # Skip regular comments
        if [[ "$trimmed" =~ ^// ]] || [[ "$trimmed" =~ ^/\* ]]; then
            continue
        fi

        # Skip empty lines but check for module doc
        if [[ -z "$trimmed" ]]; then
            if [[ ${#doc_buffer[@]} -gt 0 && $module_doc_found -eq 0 && ${#module_desc[@]} -eq 0 ]]; then
                module_desc=("${doc_buffer[@]}")
                module_doc_found=1
                doc_buffer=()
            fi
            continue
        fi

        # Check if public
        local is_public=0
        if [[ "$trimmed" =~ ^pub[[:space:]] ]]; then
            is_public=1
        fi

        # Parse function (including method syntax)
        local fn_regex='fn[[:space:]]+(\([^)]*\)[[:space:]]+)?([a-zA-Z_][a-zA-Z0-9_]*)[[:space:]]*\(([^)]*)\)[[:space:]]*(!?)'
        if [[ "$trimmed" =~ $fn_regex ]]; then
            local fn_receiver="${BASH_REMATCH[1]}"
            local fn_name="${BASH_REMATCH[2]}"
            local fn_sig="${BASH_REMATCH[3]}"
            local fn_failable="${BASH_REMATCH[4]}"

            if [[ $is_public -eq 1 ]]; then
                # Parse doc comment
                local desc="" params="" returns="" errors="" examples=""
                for doc_line in "${doc_buffer[@]}"; do
                    if [[ "$doc_line" =~ ^@param[[:space:]]+([a-zA-Z_][a-zA-Z0-9_]*)[[:space:]]+([a-zA-Z0-9_]+)[[:space:]]*(.*) ]]; then
                        local p_name="${BASH_REMATCH[1]}"
                        local p_type="${BASH_REMATCH[2]}"
                        local p_desc="${BASH_REMATCH[3]}"
                        params+="${p_name}:${p_type}:${p_desc};"
                    elif [[ "$doc_line" =~ ^@return[[:space:]]+([a-zA-Z_][a-zA-Z0-9_]*)[[:space:]]+([a-zA-Z0-9_]+)[[:space:]]*(.*) ]]; then
                        local r_name="${BASH_REMATCH[1]}"
                        local r_type="${BASH_REMATCH[2]}"
                        local r_desc="${BASH_REMATCH[3]}"
                        returns+="${r_name}:${r_type}:${r_desc};"
                    elif [[ "$doc_line" =~ ^@example[[:space:]]+(.*) ]]; then
                        examples+="${BASH_REMATCH[1]};"
                    elif [[ "$doc_line" =~ ^@error[[:space:]]+(Err[a-zA-Z0-9_]+)[[:space:]]+(.*) ]]; then
                        local e_code="${BASH_REMATCH[1]}"
                        local e_desc="${BASH_REMATCH[2]}"
                        errors+="${e_code}:${e_desc};"
                    elif [[ "$doc_line" =~ ^@error[[:space:]]+(.*) ]]; then
                        errors+=":${BASH_REMATCH[1]};"
                    elif [[ ! "$doc_line" =~ ^@ ]]; then
                        if [[ -n "$desc" ]]; then
                            desc+=" "
                        fi
                        desc+="$doc_line"
                    fi
                done

                local sig_str
                if [[ -n "$fn_sig" ]]; then
                    sig_str="($fn_sig)"
                else
                    sig_str="()"
                fi

                # For methods, prepend receiver to signature display
                if [[ -n "$fn_receiver" ]]; then
                    fn_receiver="${fn_receiver%% }"
                    sig_str="$fn_receiver $fn_name$sig_str"
                fi

                functions+=("$fn_name|$sig_str|$fn_failable|$desc|$params|$returns|$errors|$examples")
            fi
            doc_buffer=()

        # Parse constant
        elif [[ "$trimmed" =~ const[[:space:]]+([a-zA-Z_][a-zA-Z0-9_]*)[[:space:]]*=[[:space:]]*(.+) ]]; then
            local const_name="${BASH_REMATCH[1]}"
            local const_value="${BASH_REMATCH[2]}"

            if [[ $is_public -eq 1 ]]; then
                local desc=""
                for doc_line in "${doc_buffer[@]}"; do
                    if [[ ! "$doc_line" =~ ^@ ]]; then
                        if [[ -n "$desc" ]]; then
                            desc+=" "
                        fi
                        desc+="$doc_line"
                    fi
                done
                constants+=("$const_name|$const_value|$desc")
            fi
            doc_buffer=()

        # Parse struct
        elif [[ "$trimmed" =~ struct[[:space:]]+([a-zA-Z_][a-zA-Z0-9_]*) ]]; then
            struct_name="${BASH_REMATCH[1]}"

            if [[ $is_public -eq 1 ]]; then
                struct_desc=""
                struct_doc_fields=""
                for doc_line in "${doc_buffer[@]}"; do
                    if [[ "$doc_line" =~ ^@field[[:space:]]+([a-zA-Z_][a-zA-Z0-9_]*)[[:space:]]+([a-zA-Z0-9_]+)[[:space:]]*(.*) ]]; then
                        local f_name="${BASH_REMATCH[1]}"
                        local f_type="${BASH_REMATCH[2]}"
                        local f_desc="${BASH_REMATCH[3]}"
                        struct_doc_fields+="${f_name}~${f_type}~${f_desc}§"
                    elif [[ ! "$doc_line" =~ ^@ ]]; then
                        if [[ -n "$struct_desc" ]]; then
                            struct_desc+=" "
                        fi
                        struct_desc+="$doc_line"
                    fi
                done

                if [[ -n "$struct_doc_fields" ]]; then
                    structs+=("$struct_name|$struct_desc|$struct_doc_fields")
                else
                    in_struct=1
                    struct_fields=""
                fi
            fi
            doc_buffer=()

        # Parse struct fields (if in struct body and no @field docs)
        elif [[ $in_struct -eq 1 ]]; then
            if [[ "$trimmed" == "}" ]]; then
                structs+=("$struct_name|$struct_desc|$struct_fields")
                in_struct=0
            elif [[ "$trimmed" =~ ^([a-zA-Z_][a-zA-Z0-9_]*)[[:space:]]*:[[:space:]]*([a-zA-Z0-9_]+) ]]; then
                local f_name="${BASH_REMATCH[1]}"
                local f_type="${BASH_REMATCH[2]}"
                struct_fields+="${f_name}~${f_type}~§"
            fi

        # Import/use statement - module doc comes before this
        elif [[ "$trimmed" =~ ^import[[:space:]] ]] || [[ "$trimmed" =~ ^use[[:space:]] ]]; then
            if [[ ${#doc_buffer[@]} -gt 0 && ${#module_desc[@]} -eq 0 ]]; then
                module_desc=("${doc_buffer[@]}")
                module_doc_found=1
            fi
            doc_buffer=()

        else
            doc_buffer=()
        fi

    done < <(expand_includes "$filepath")

    # Generate markdown
    generate_markdown "$module_name" module_desc constants structs functions

    # Generate JSON
    generate_json "$module_name" module_desc constants structs functions
}

# Generate markdown documentation
generate_markdown() {
    local module_name="$1"
    local -n _module_desc=$2
    local -n _constants=$3
    local -n _structs=$4
    local -n _functions=$5

    local output=""

    # Title with `use` prefix
    output+="# \`use\` $module_name"$'\n\n'

    # Module description (with example block detection)
    local in_example=0
    for desc in "${_module_desc[@]}"; do
        if [[ "$desc" =~ ^Example: ]]; then
            output+="**Example:**"$'\n\n'
            output+="\`\`\`qd"$'\n'
            in_example=1
        elif [[ $in_example -eq 1 ]]; then
            # Check if still in indented example code
            if [[ "$desc" =~ ^[[:space:]] ]] || [[ -z "$desc" ]]; then
                # Remove leading indentation (2 spaces) from example lines
                local unindented="${desc#  }"
                output+="$unindented"$'\n'
            else
                # End of example block
                output+="\`\`\`"$'\n\n'
                in_example=0
                output+="$desc"$'\n'
            fi
        else
            output+="$desc"$'\n'
        fi
    done
    # Close example block if still open at end
    if [[ $in_example -eq 1 ]]; then
        output+="\`\`\`"$'\n'
    fi
    if [[ ${#_module_desc[@]} -gt 0 ]]; then
        output+=$'\n'
    fi

    # Sort constants by name
    IFS=$'\n' sorted_constants=($(sort <<<"${_constants[*]}")); unset IFS

    # Constants table
    if [[ ${#sorted_constants[@]} -gt 0 && -n "${sorted_constants[0]}" ]]; then
        output+="## Constants"$'\n\n'
        output+="| Name | Value | Description |"$'\n'
        output+="|------|-------|-------------|"$'\n'
        for const in "${sorted_constants[@]}"; do
            [[ -z "$const" ]] && continue
            IFS='|' read -r c_name c_value c_desc <<< "$const"
            output+="| \`$c_name\` | \`$c_value\` | $c_desc |"$'\n'
        done
        output+=$'\n'
    fi

    # Sort structs by name
    IFS=$'\n' sorted_structs=($(sort <<<"${_structs[*]}")); unset IFS

    # Structs
    if [[ ${#sorted_structs[@]} -gt 0 && -n "${sorted_structs[0]}" ]]; then
        output+="## Structs"$'\n\n'
        for struct in "${sorted_structs[@]}"; do
            [[ -z "$struct" ]] && continue
            IFS='|' read -r s_name s_desc s_fields <<< "$struct"
            output+="### \`struct\` $s_name"$'\n\n'
            if [[ -n "$s_desc" ]]; then
                output+="$s_desc"$'\n\n'
            fi
            # Fields table (using § as field separator, ~ as internal separator)
            if [[ -n "$s_fields" ]]; then
                output+="| Field | Type | Description |"$'\n'
                output+="|-------|------|-------------|"$'\n'
                IFS='§' read -ra field_arr <<< "$s_fields"
                for field in "${field_arr[@]}"; do
                    [[ -z "$field" ]] && continue
                    IFS='~' read -r f_name f_type f_desc <<< "$field"
                    output+="| \`$f_name\` | \`$f_type\` | $f_desc |"$'\n'
                done
                output+=$'\n'
            fi
        done
    fi

    # Sort functions by name
    IFS=$'\n' sorted_functions=($(sort <<<"${_functions[*]}")); unset IFS

    # Functions
    if [[ ${#sorted_functions[@]} -gt 0 && -n "${sorted_functions[0]}" ]]; then
        output+="## Functions"$'\n\n'
        local first=1
        for func in "${sorted_functions[@]}"; do
            [[ -z "$func" ]] && continue
            IFS='|' read -r f_name f_sig f_failable f_desc f_params f_returns f_errors f_examples <<< "$func"

            if [[ $first -eq 0 ]]; then
                output+="---"$'\n\n'
            fi
            first=0

            output+="### \`fn\` $f_name"$'\n\n'

            # Description
            if [[ -n "$f_desc" ]]; then
                output+="$f_desc"$'\n\n'
            fi

            # Signature
            output+="**Signature:** \`$f_sig$f_failable\`"$'\n\n'

            # Parameters table
            if [[ -n "$f_params" ]]; then
                output+="| Parameter | Type | Description |"$'\n'
                output+="|-----------|------|-------------|"$'\n'
                IFS=';' read -ra param_arr <<< "$f_params"
                for param in "${param_arr[@]}"; do
                    [[ -z "$param" ]] && continue
                    IFS=':' read -r p_name p_type p_desc <<< "$param"
                    output+="| \`$p_name\` | \`$p_type\` | $p_desc |"$'\n'
                done
                output+=$'\n'
            fi

            # Returns table
            if [[ -n "$f_returns" ]]; then
                output+="| Output | Type | Description |"$'\n'
                output+="|--------|------|-------------|"$'\n'
                IFS=';' read -ra return_arr <<< "$f_returns"
                for ret in "${return_arr[@]}"; do
                    [[ -z "$ret" ]] && continue
                    IFS=':' read -r r_name r_type r_desc <<< "$ret"
                    output+="| \`$r_name\` | \`$r_type\` | $r_desc |"$'\n'
                done
                output+=$'\n'
            fi

            # Errors table
            if [[ -n "$f_errors" ]]; then
                output+="| Error | Description |"$'\n'
                output+="|-------|-------------|"$'\n'
                IFS=';' read -ra error_arr <<< "$f_errors"
                for err in "${error_arr[@]}"; do
                    [[ -z "$err" ]] && continue
                    IFS=':' read -r e_code e_desc <<< "$err"
                    if [[ -n "$e_code" ]]; then
                        output+="| \`$module_name::$e_code\` | $e_desc |"$'\n'
                    else
                        output+="| - | $e_desc |"$'\n'
                    fi
                done
                output+=$'\n'
            fi

            # Examples
            if [[ -n "$f_examples" ]]; then
                output+="**Example:**"$'\n\n'
                output+="\`\`\`qd"$'\n'
                IFS=';' read -ra example_arr <<< "$f_examples"
                for ex in "${example_arr[@]}"; do
                    [[ -z "$ex" ]] && continue
                    # Transform "code -> output" format
                    if [[ "$ex" =~ ^(.+)[[:space:]]-\>[[:space:]](.+)$ ]]; then
                        output+="${BASH_REMATCH[1]}  // ${BASH_REMATCH[2]}"$'\n'
                    else
                        output+="$ex"$'\n'
                    fi
                done
                output+="\`\`\`"$'\n\n'
            fi
        done
    fi

    # Write to file
    local md_path="$DOCS_DIR/$module_name.md"
    echo -n "$output" > "$md_path"
}

# Generate JSON documentation
generate_json() {
    local module_name="$1"
    local -n __module_desc=$2
    local -n __constants=$3
    local -n __structs=$4
    local -n __functions=$5

    local desc_joined=""
    for d in "${__module_desc[@]}"; do
        if [[ -n "$desc_joined" ]]; then
            desc_joined+=" "
        fi
        desc_joined+="$d"
    done

    local output="{"$'\n'
    output+="  \"name\": \"$module_name\","$'\n'
    output+="  \"description\": \"$(json_escape "$desc_joined")\","$'\n'

    # Constants
    output+="  \"constants\": ["$'\n'
    local first=1
    IFS=$'\n' sorted_constants=($(sort <<<"${__constants[*]}")); unset IFS
    for const in "${sorted_constants[@]}"; do
        [[ -z "$const" ]] && continue
        IFS='|' read -r c_name c_value c_desc <<< "$const"
        if [[ $first -eq 0 ]]; then
            output+=","$'\n'
        fi
        first=0
        output+="    {"$'\n'
        output+="      \"name\": \"$c_name\","$'\n'
        output+="      \"value\": \"$(json_escape "$c_value")\","$'\n'
        output+="      \"description\": \"$(json_escape "$c_desc")\""$'\n'
        output+="    }"
    done
    output+=$'\n  ],'$'\n'

    # Structs
    output+="  \"structs\": ["$'\n'
    first=1
    IFS=$'\n' sorted_structs=($(sort <<<"${__structs[*]}")); unset IFS
    for struct in "${sorted_structs[@]}"; do
        [[ -z "$struct" ]] && continue
        IFS='|' read -r s_name s_desc s_fields <<< "$struct"
        if [[ $first -eq 0 ]]; then
            output+=","$'\n'
        fi
        first=0
        output+="    {"$'\n'
        output+="      \"name\": \"$s_name\","$'\n'
        output+="      \"description\": \"$(json_escape "$s_desc")\","$'\n'
        output+="      \"fields\": ["$'\n'
        local first_field=1
        IFS='§' read -ra field_arr <<< "$s_fields"
        for field in "${field_arr[@]}"; do
            [[ -z "$field" ]] && continue
            IFS='~' read -r f_name f_type f_desc <<< "$field"
            if [[ $first_field -eq 0 ]]; then
                output+=","$'\n'
            fi
            first_field=0
            output+="        {\"name\": \"$f_name\", \"type\": \"$f_type\", \"description\": \"$(json_escape "$f_desc")\"}"
        done
        output+=$'\n      ]'$'\n'
        output+="    }"
    done
    output+=$'\n  ],'$'\n'

    # Functions
    output+="  \"functions\": ["$'\n'
    first=1
    IFS=$'\n' sorted_functions=($(sort <<<"${__functions[*]}")); unset IFS
    for func in "${sorted_functions[@]}"; do
        [[ -z "$func" ]] && continue
        IFS='|' read -r f_name f_sig f_failable f_desc f_params f_returns f_errors f_examples <<< "$func"
        if [[ $first -eq 0 ]]; then
            output+=","$'\n'
        fi
        first=0

        local failable_bool="false"
        [[ "$f_failable" == "!" ]] && failable_bool="true"

        output+="    {"$'\n'
        output+="      \"name\": \"$f_name\","$'\n'
        output+="      \"signature\": \"$(json_escape "$f_sig")\","$'\n'
        output+="      \"failable\": $failable_bool,"$'\n'
        output+="      \"description\": \"$(json_escape "$f_desc")\","$'\n'

        # Params
        output+="      \"params\": ["
        local first_p=1
        IFS=';' read -ra param_arr <<< "$f_params"
        for param in "${param_arr[@]}"; do
            [[ -z "$param" ]] && continue
            IFS=':' read -r p_name p_type p_desc <<< "$param"
            if [[ $first_p -eq 0 ]]; then
                output+=", "
            fi
            first_p=0
            output+="{\"name\": \"$p_name\", \"type\": \"$p_type\", \"description\": \"$(json_escape "$p_desc")\"}"
        done
        output+="],"$'\n'

        # Returns
        output+="      \"returns\": ["
        local first_r=1
        IFS=';' read -ra return_arr <<< "$f_returns"
        for ret in "${return_arr[@]}"; do
            [[ -z "$ret" ]] && continue
            IFS=':' read -r r_name r_type r_desc <<< "$ret"
            if [[ $first_r -eq 0 ]]; then
                output+=", "
            fi
            first_r=0
            output+="{\"name\": \"$r_name\", \"type\": \"$r_type\", \"description\": \"$(json_escape "$r_desc")\"}"
        done
        output+="],"$'\n'

        # Errors
        output+="      \"errors\": ["
        local first_e=1
        IFS=';' read -ra error_arr <<< "$f_errors"
        for err in "${error_arr[@]}"; do
            [[ -z "$err" ]] && continue
            IFS=':' read -r e_code e_desc <<< "$err"
            if [[ $first_e -eq 0 ]]; then
                output+=", "
            fi
            first_e=0
            output+="{\"code\": \"$e_code\", \"description\": \"$(json_escape "$e_desc")\"}"
        done
        output+="],"$'\n'

        # Examples
        output+="      \"examples\": ["
        local first_ex=1
        IFS=';' read -ra example_arr <<< "$f_examples"
        for ex in "${example_arr[@]}"; do
            [[ -z "$ex" ]] && continue
            if [[ $first_ex -eq 0 ]]; then
                output+=", "
            fi
            first_ex=0
            output+="\"$(json_escape "$ex")\""
        done
        output+="]"$'\n'
        output+="    }"
    done
    output+=$'\n  ]'$'\n'
    output+="}"$'\n'

    # Write to file
    local json_path="$JSON_DIR/$module_name.json"
    echo -n "$output" > "$json_path"
}

# Generate language reference from reference.def
generate_reference() {
    local ref_def="$PROJECT_ROOT/lib/qc/include/qc/reference.def"

    if [[ ! -f "$ref_def" ]]; then
        echo "Warning: $ref_def not found, skipping reference generation"
        return
    fi

    echo "Generating language reference..."

    local -a keywords=()      # "name|description|examples"
    local -a builtins=()      # "name|signature|description|category|examples"
    local current_category=""
    local -a doc_buffer=()

    while IFS= read -r line || [[ -n "$line" ]]; do
        # Track category from section comments
        if [[ "$line" =~ ^//[[:space:]]*===.* ]]; then
            continue
        fi

        if [[ "$line" =~ ^///  ]]; then
            : # Skip doc comments here, handle below
        elif [[ "$line" =~ ^//[[:space:]]+([^=].*)$ ]]; then
            current_category="${BASH_REMATCH[1]}"
            current_category="${current_category#"${current_category%%[![:space:]]*}"}"
            current_category="${current_category%"${current_category##*[![:space:]]}"}"
            continue
        fi

        # Collect doc comments
        if [[ "$line" =~ ^/// ]]; then
            local doc_text="${line#///}"
            doc_text="${doc_text# }"
            doc_buffer+=("$doc_text")
            continue
        fi

        # Parse KEYWORD(name, "description")
        if [[ "$line" =~ KEYWORD\(([^,]+),[[:space:]]*\"([^\"]*)\"\) ]]; then
            local kw_name="${BASH_REMATCH[1]}"
            local kw_desc="${BASH_REMATCH[2]}"

            local examples=""
            for doc in "${doc_buffer[@]}"; do
                if [[ "$doc" =~ ^@example[[:space:]]+(.*) ]]; then
                    examples+="${BASH_REMATCH[1]};"
                fi
            done

            keywords+=("$kw_name|$kw_desc|$examples")
            doc_buffer=()
            continue
        fi

        # Parse BUILTIN(name, "signature", "description")
        if [[ "$line" =~ BUILTIN\(([^,]+),[[:space:]]*\"([^\"]*)\"\,[[:space:]]*\"([^\"]*)\"\) ]]; then
            local bi_name="${BASH_REMATCH[1]}"
            local bi_sig="${BASH_REMATCH[2]}"
            local bi_desc="${BASH_REMATCH[3]}"

            local examples=""
            for doc in "${doc_buffer[@]}"; do
                if [[ "$doc" =~ ^@example[[:space:]]+(.*) ]]; then
                    examples+="${BASH_REMATCH[1]};"
                fi
            done

            builtins+=("$bi_name|$bi_sig|$bi_desc|$current_category|$examples")
            doc_buffer=()
            continue
        fi

        # Clear buffer on non-matching lines
        if [[ ! "$line" =~ ^// ]]; then
            doc_buffer=()
        fi

    done < "$ref_def"

    # Generate markdown
    local output=""
    output+="# Language Reference"$'\n\n'
    output+="This page documents all Quadrate keywords and built-in instructions."$'\n\n'

    # Keywords section
    if [[ ${#keywords[@]} -gt 0 ]]; then
        output+="## Keywords"$'\n\n'
        output+="| Keyword | Description |"$'\n'
        output+="|---------|-------------|"$'\n'
        for kw in "${keywords[@]}"; do
            IFS='|' read -r kw_name kw_desc kw_examples <<< "$kw"
            local anchor="${kw_name//->/arrow}"
            anchor="${anchor//=>/case-arrow}"
            output+="| [\`$kw_name\`](#$anchor) | $kw_desc |"$'\n'
        done
        output+=$'\n'

        for kw in "${keywords[@]}"; do
            IFS='|' read -r kw_name kw_desc kw_examples <<< "$kw"
            output+="### $kw_name"$'\n\n'
            output+="$kw_desc"$'\n\n'
            if [[ -n "$kw_examples" ]]; then
                output+="**Example:**"$'\n\n'
                output+="\`\`\`qd"$'\n'
                IFS=';' read -ra ex_arr <<< "$kw_examples"
                for ex in "${ex_arr[@]}"; do
                    [[ -z "$ex" ]] && continue
                    output+="$ex"$'\n'
                done
                output+="\`\`\`"$'\n\n'
            fi
            output+="---"$'\n\n'
        done
    fi

    # Builtins by category
    output+="## Built-in Instructions"$'\n\n'

    declare -A categories
    for bi in "${builtins[@]}"; do
        IFS='|' read -r bi_name bi_sig bi_desc bi_cat bi_examples <<< "$bi"
        [[ -z "$bi_cat" ]] && bi_cat="Other"
        categories["$bi_cat"]+="$bi"$'\n'
    done

    for cat in "${!categories[@]}"; do
        output+="### $cat"$'\n\n'
        output+="| Instruction | Signature | Description |"$'\n'
        output+="|-------------|-----------|-------------|"$'\n'

        while IFS= read -r bi; do
            [[ -z "$bi" ]] && continue
            IFS='|' read -r bi_name bi_sig bi_desc bi_cat bi_examples <<< "$bi"
            local sig_display="-"
            [[ -n "$bi_sig" ]] && sig_display="\`$bi_sig\`"
            local anchor="${bi_name//+/plus}"
            anchor="${anchor//-/minus}"
            anchor="${anchor//\*/star}"
            anchor="${anchor//\//slash}"
            anchor="${anchor//%/percent}"
            anchor="${anchor//!/bang}"
            anchor="${anchor//</lt}"
            anchor="${anchor//>/gt}"
            anchor="${anchor//=/eq}"
            output+="| [\`$bi_name\`](#$anchor) | $sig_display | $bi_desc |"$'\n'
        done <<< "${categories[$cat]}"
        output+=$'\n'

        while IFS= read -r bi; do
            [[ -z "$bi" ]] && continue
            IFS='|' read -r bi_name bi_sig bi_desc bi_cat bi_examples <<< "$bi"
            output+="#### $bi_name"$'\n\n'
            output+="$bi_desc"$'\n\n'
            if [[ -n "$bi_sig" ]]; then
                output+="**Signature:** \`$bi_sig\`"$'\n\n'
            fi
            if [[ -n "$bi_examples" ]]; then
                output+="**Example:**"$'\n\n'
                output+="\`\`\`qd"$'\n'
                IFS=';' read -ra ex_arr <<< "$bi_examples"
                for ex in "${ex_arr[@]}"; do
                    [[ -z "$ex" ]] && continue
                    output+="$ex"$'\n'
                done
                output+="\`\`\`"$'\n\n'
            fi
            output+="---"$'\n\n'
        done <<< "${categories[$cat]}"
    done

    # Write reference
    local ref_path="$PROJECT_ROOT/docs/docs/docs/reference.md"
    echo -n "$output" > "$ref_path"

    echo "  ${#keywords[@]} keywords, ${#builtins[@]} built-in instructions"
    echo "  Written to: $ref_path"
}

# Generate index page
generate_index() {
    local index_path="$DOCS_DIR/index.md"

    cat > "$index_path" << 'EOF'
# Standard Library

The Quadrate standard library provides modules for common programming tasks.

## Using Modules

Import a module with `use`:

```qd
use str
use math

fn main() {
	"hello" str::upper print nl  // HELLO
	16.0 math::sqrt print nl  // 4
}
```

## Fallible Functions

Functions marked with `!` can fail and require error handling:

```qd
use str

fn main() {
	"hello" 0 3 str::substring! print nl  // "hel"
}
```

## Available Modules

EOF

    echo "| Module | Description |" >> "$index_path"
    echo "|--------|-------------|" >> "$index_path"

    for name in $(echo "${!STDLIB_MODULES[@]}" | tr ' ' '\n' | sort); do
        local path="${STDLIB_MODULES[$name]}"
        local filepath="$PROJECT_ROOT/$path"
        if [[ -f "$filepath" ]]; then
            # Get first line of module description
            local desc=""
            while IFS= read -r line; do
                if [[ "$line" =~ ^///[[:space:]]*(.*) ]]; then
                    desc="${BASH_REMATCH[1]}"
                    break
                fi
            done < "$filepath"

            # Truncate if too long
            if [[ ${#desc} -gt 60 ]]; then
                desc="${desc:0:60}..."
            fi

            echo "| [$name]($name.md) | $desc |" >> "$index_path"
        fi
    done
}

# Main
main() {
    if [[ $# -gt 0 ]]; then
        # Single file mode
        local filepath="$1"
        parse_module "$filepath"
        echo "Generated documentation for $(get_module_name "$filepath")"
    else
        # Generate all stdlib docs
        mkdir -p "$DOCS_DIR"
        mkdir -p "$JSON_DIR"

        echo "Generating documentation to $DOCS_DIR"

        local fn_total=0
        local const_total=0

        for name in $(echo "${!STDLIB_MODULES[@]}" | tr ' ' '\n' | sort); do
            local path="${STDLIB_MODULES[$name]}"
            local filepath="$PROJECT_ROOT/$path"

            if [[ ! -f "$filepath" ]]; then
                echo "  SKIP $name: $path not found"
                continue
            fi

            parse_module "$filepath"

            # Count items (rough count from generated files)
            local fn_count
            fn_count=$(grep -c "^### \`fn\`" "$DOCS_DIR/$name.md" 2>/dev/null) || fn_count=0
            # Count constants - lines between "## Constants" and next "##" that start with "| `"
            local const_count
            const_count=$(sed -n '/^## Constants$/,/^## /{/^\| `[A-Z]/p}' "$DOCS_DIR/$name.md" 2>/dev/null | wc -l)
            const_count=${const_count// /}

            echo "  $name: $fn_count functions, $const_count constants"
        done

        # Generate index
        generate_index

        # Generate reference
        generate_reference

        echo ""
        echo "Generated ${#STDLIB_MODULES[@]} module docs + index"
        echo "Markdown: $DOCS_DIR"
        echo "JSON API: $JSON_DIR"
    fi
}

main "$@"
