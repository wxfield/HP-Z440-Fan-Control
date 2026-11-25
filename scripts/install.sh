#!/usr/bin/env bash
set -euo pipefail

if (( EUID != 0 )); then
    echo "Run this installer as root." >&2
    exit 1
fi

repo_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
product=$(cat /sys/class/dmi/id/product_name 2>/dev/null || true)
if [[ $product != *Z440* ]]; then
    echo "Refusing to install: DMI product is '$product', not an HP Z440." >&2
    exit 1
fi

for command in gcc install systemctl systemd-analyze; do
    command -v "$command" >/dev/null || {
        echo "Missing required command: $command" >&2
        exit 1
    }
done

tmp_dir=$(mktemp -d /var/tmp/z440-fan-control.XXXXXX)
cleanup() {
    rm -f -- "$tmp_dir/z440-hwmio"
    rmdir -- "$tmp_dir" 2>/dev/null || true
}
trap cleanup EXIT

gcc -O2 -Wall -Wextra -Werror \
    -o "$tmp_dir/z440-hwmio" "$repo_dir/src/z440_hwmio.c"

install -m 0750 "$tmp_dir/z440-hwmio" /usr/local/sbin/z440-hwmio
install -m 0750 "$repo_dir/scripts/z440-fan-control" /usr/local/sbin/z440-fan-control
install -m 0750 "$repo_dir/scripts/z440-fan-boost" /usr/local/sbin/z440-fan-boost
install -m 0644 "$repo_dir/systemd/z440-fan-control.service" \
    /etc/systemd/system/z440-fan-control.service
if [[ ! -e /etc/sysconfig/z440-fan-control ]]; then
    install -m 0644 "$repo_dir/config/z440-fan-control" /etc/sysconfig/z440-fan-control
else
    echo "Preserving existing /etc/sysconfig/z440-fan-control"
fi

/usr/local/sbin/z440-hwmio snapshot
systemctl daemon-reload
systemd-analyze verify /etc/systemd/system/z440-fan-control.service
systemctl enable z440-fan-control.service
# Restart even when upgrading an already-active installation so the process in
# memory always matches the files just installed. ExecStopPost and ExecStartPre
# both force maximum speed around the transition.
systemctl restart z440-fan-control.service
systemctl --no-pager --full status z440-fan-control.service

echo "Installation complete. Monitor with:"
echo "  journalctl -u z440-fan-control.service -f"
