#!/usr/bin/env bash

set -euo pipefail

# Profile execution uses the portable runner; the single-link editor below
# remains available with -t/--topology.
if [[ "${1:-}" == "--profile" ]]; then
    SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
    if command -v python3 >/dev/null 2>&1; then
        exec python3 "$SCRIPT_DIR/../scripts/experiment_profile.py" "$@"
    elif command -v python >/dev/null 2>&1; then
        exec python "$SCRIPT_DIR/../scripts/experiment_profile.py" "$@"
    else
        echo "Error: Python 3 is required to run an experiment profile." >&2
        exit 1
    fi
fi

TOPOLOGY=""
INDEX=0
BANDWIDTH_NEW_VALUE=""
DELAY_NEW_VALUE=""
LOSS_NEW_VALUE=""
NEW_TOPOLOGY="./KNS Experiments/results/topology.json"

usage() {
    printf 'Usage: %s -t <topology.json> [-bw <bandwidth>] [-d <delay>] [-l <loss>] [-i <index>] [-o <output.json>]\n' "$0"
    printf '       %s --profile <profile.json> [--dry-run | --generate-only] [--exe <KNS>] [--output <directory>] [--timeout <seconds>]\n' "$0"
}

while (( $# > 0 )); do
    case "$1" in
        -t|--topology)
            if (( $# < 2 )); then
                echo "Error: missing topology path." >&2
                exit 2
            fi
            TOPOLOGY="$2"
            shift 2
            ;;

        -bw|--bandwidth)
            if (( $# < 2 )); then
                echo "Error: missing bandwidth value." >&2
                exit 2
            fi
            BANDWIDTH_NEW_VALUE="$2"
            shift 2
            ;;

        -d|--delay)
            if (( $# < 2 )); then
                echo "Error: missing delay value." >&2
                exit 2
            fi
            DELAY_NEW_VALUE="$2"
            shift 2
            ;;

        -l|--loss)
            if (( $# < 2 )); then
                echo "Error: missing loss value." >&2
                exit 2
            fi
            LOSS_NEW_VALUE="$2"
            shift 2
            ;;

        -i|--index)
            if (( $# < 2 )); then
                echo "Error: missing link index." >&2
                exit 2
            fi
            INDEX="$2"
            shift 2
            ;;

        -o|--output)
            if (( $# < 2 )); then
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

if [[ -z "$TOPOLOGY" ]]; then
    echo "Error: topology path is required." >&2
    usage >&2
    exit 2
fi

if [[ -z "$BANDWIDTH_NEW_VALUE" &&
      -z "$DELAY_NEW_VALUE" &&
      -z "$LOSS_NEW_VALUE" ]]; then
    echo "Error: provide at least one parameter to modify." >&2
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

for value in "$BANDWIDTH_NEW_VALUE" "$DELAY_NEW_VALUE" "$LOSS_NEW_VALUE"; do
    if [[ -n "$value" && ! "$value" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
        printf 'Error: invalid numeric value: %s\n' "$value" >&2
        exit 2
    fi
done

if [[ -e "$NEW_TOPOLOGY" && "$TOPOLOGY" -ef "$NEW_TOPOLOGY" ]]; then
    echo "Error: input and output refer to the same file." >&2
    exit 1
fi

OUTPUT_DIR="$(dirname -- "$NEW_TOPOLOGY")"
mkdir -p -- "$OUTPUT_DIR"

TEMP_FILE="$(mktemp "$OUTPUT_DIR/.topology.XXXXXX")"
trap 'rm -f -- "$TEMP_FILE"' EXIT

if jq \
    --argjson idx "$INDEX" \
    --argjson bw "${BANDWIDTH_NEW_VALUE:-null}" \
    --argjson delay "${DELAY_NEW_VALUE:-null}" \
    --argjson loss "${LOSS_NEW_VALUE:-null}" \
    '
    if type != "object" then
        error("Topology root must be an object")
    elif (.links | type) != "array" then
        error("Topology must contain a links array")
    elif $idx >= (.links | length) then
        error("Link index does not exist")
    elif (.links[$idx] | type) != "object" then
        error("Selected link is not an object")
    elif $bw != null and
         ((.links[$idx] | has("bandwidth")) | not) then
        error("Link has no bandwidth field")
    elif $delay != null and
         ((.links[$idx] | has("delay")) | not) then
        error("Link has no delay field")
    elif $loss != null and
         ((.links[$idx] | has("loss")) | not) then
        error("Link has no loss field")
    elif $bw != null and
         (($bw | type) != "number" or $bw <= 0) then
        error("Bandwidth must be positive")
    elif $delay != null and
         (($delay | type) != "number" or $delay < 0) then
        error("Delay cannot be negative")
    elif $loss != null and
         (($loss | type) != "number" or $loss < 0 or $loss > 1) then
        error("Loss probability must be between 0 and 1")
    else
        .links[$idx] |= (
            if $bw != null then .bandwidth = $bw else . end
            | if $delay != null then .delay = $delay else . end
            | if $loss != null then .loss = $loss else . end
        )
    end
    ' "$TOPOLOGY" > "$TEMP_FILE"
then
    mv -f -- "$TEMP_FILE" "$NEW_TOPOLOGY"
    trap - EXIT
    printf 'Updated topology saved to: %s\n' "$NEW_TOPOLOGY"
else
    echo "Error: failed to update topology." >&2
    exit 1
fi
