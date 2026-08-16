#!/usr/bin/env bash
# @file scripts/build.sh
# @brief Preserves the legacy build-script entry point.

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

exec "${script_dir}/../build" "$@"
