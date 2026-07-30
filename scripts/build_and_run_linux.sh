#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd -- "${script_dir}/.." && pwd)"

cd "${project_dir}"
cmake --preset linux-desktop-release
cmake --build --preset linux-desktop-release
exec "${project_dir}/build/linux-desktop/galileosky_test_project"
