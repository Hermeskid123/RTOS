#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "${script_dir}/.." && pwd)"
build_dir="${BUILD_DIR:-${project_root}/.build}"
simulator="${build_dir}/rtos_sim"

if [[ ! -x "${simulator}" ]]; then
    "${project_root}/build"
fi

exec "${simulator}" "$@"
