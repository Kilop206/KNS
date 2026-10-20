
#!/usr/bin/env bash

set -euo pipefail

TOPOLOGY=""
INDEX=0
BANDWIDTH_NEW_VALUE=""
NEW_TOPOLOGY="./results/topology.json"

usage() {
    printf 'Usage: %s -t <topology.json> -bw <bandwidth> [-i <index>] [-o <output.json>]\n' "$0"
}

while (($# > 0)); do
    case "$1" in
        -t|--topology)
            if (($# < 2)); then
                echo "Error: missing topology path." >&2
                exit 2
            fi
            TOPOLOGY="$2"
            shift 2
            ;;

        -bw|--bandwidth)
            if (($# < 2)); then
                echo "Error: missing bandwidth value." >&2
                exit 2
            fi
            BANDWIDTH_NEW_VALUE="$2"
            shift 2
            ;;

        -i|--index)
            if (($# < 2)); then
                echo "Error: missing link index." >&2
                exit 2
            fi
            INDEX="$2"
            shift 2
            ;;

        -o|--output)
            if (($# < 2)); then
                echo "Error: missing output path." >&2
                exit 2
            fi
            NEW_TOPOLOGY="$2"
            shift 2
            ;;

        -h|--help)
            usage
            exit 0
            ;;

        *)
            printf 'Unknown option: %s\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ -z "$TOPOLOGY" || -z "$BANDWIDTH_NEW_VALUE" ]]; then
    usage >&2
    exit 2
fi

if [[ ! -f "$TOPOLOGY" ]]; then
    echo "Error: topology file not found: $TOPOLOGY" >&2
    exit 1
fi

if ! command -v jq >/dev/null 2>&1; then
    echo "Error: jq is not installed or not in PATH." >&2
    exit 1
fi

if [[ ! "$INDEX" =~ ^[0-9]+$ ]]; then
    echo "Error: index must be a non-negative integer." >&2
    exit 2
fi

if [[ -e "$NEW_TOPOLOGY" && "$TOPOLOGY" -ef "$NEW_TOPOLOGY" ]]; then
    echo "Error: input and output refer to the same file." >&2
    exit 1
fi

OUTPUT_DIR="$(dirname -- "$NEW_TOPOLOGY")"
mkdir -p -- "$OUTPUT_DIR"

TEMP_FILE="$(mktemp "$OUTPUT_DIR/.topology.XXXXXX")"
trap 'rm -f -- "$TEMP_FILE"' EXIT

if jq \
    --argjson bw "$BANDWIDTH_NEW_VALUE" \
    --argjson idx "$INDEX" \
    '
    if (.links | type) != "array" then
        error("The topology must contain a links array")
    elif $idx >= (.links | length) then
        error("The requested link index does not exist")
    elif (.links[$idx] | type) != "object" then
        error("The selected link is not an object")
    elif ((.links[$idx] | has("bandwidth")) | not) then
        error("The selected link has no bandwidth field")
    elif ($bw | type) != "number" or $bw <= 0 then
        error("Bandwidth must be a positive number")
    else
        .links[$idx].bandwidth = $bw
    end
    ' "$TOPOLOGY" > "$TEMP_FILE"
then
    mv -- "$TEMP_FILE" "$NEW_TOPOLOGY"
    trap - EXIT
    printf 'Updated topology saved to: %s\n' "$NEW_TOPOLOGY"
else
    echo "Error: failed to update topology." >&2
    exit 1
fi