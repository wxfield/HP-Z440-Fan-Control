#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)

bash -n \
    "$repo_dir/scripts/z440-fan-control" \
    "$repo_dir/scripts/z440-fan-boost" \
    "$repo_dir/scripts/install.sh" \
    "$repo_dir/scripts/uninstall.sh"

if command -v shellcheck >/dev/null; then
    shellcheck \
        "$repo_dir/scripts/z440-fan-control" \
        "$repo_dir/scripts/z440-fan-boost" \
        "$repo_dir/scripts/install.sh" \
        "$repo_dir/scripts/uninstall.sh"
fi

if [[ $(uname -s) == Linux ]]; then
    make -C "$repo_dir" clean all
fi

if command -v systemd-analyze >/dev/null && \
   [[ -x /usr/local/sbin/z440-hwmio && -x /usr/local/sbin/z440-fan-control ]]; then
    systemd-analyze verify "$repo_dir/systemd/z440-fan-control.service"
else
    echo "Skipping target unit verification until installed."
fi

echo "Static checks passed."
