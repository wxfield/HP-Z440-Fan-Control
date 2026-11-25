#!/usr/bin/env bash
set -euo pipefail

if (( EUID != 0 )); then
    echo "Run this uninstaller as root." >&2
    exit 1
fi

# Stopping invokes ExecStopPost, which commands maximum speed. Repeat that
# explicitly before removing the helper so a partial unit failure is also safe.
systemctl disable --now z440-fan-control.service 2>/dev/null || true
if [[ -x /usr/local/sbin/z440-hwmio ]]; then
    /usr/local/sbin/z440-hwmio write-fan 0x0198 0x7e || true
    /usr/local/sbin/z440-hwmio write-fan 0x019a 0xb7 || true
    /usr/local/sbin/z440-hwmio write-fan 0x019b 0xb7 || true
    /usr/local/sbin/z440-hwmio write-fan 0x019c 0x7e || true
    /usr/local/sbin/z440-hwmio write-fan 0x019d 0x7e || true
fi

rm -f -- /etc/systemd/system/z440-fan-control.service
rm -f -- /usr/local/sbin/z440-hwmio
rm -f -- /usr/local/sbin/z440-fan-control
rm -f -- /usr/local/sbin/z440-fan-boost
systemctl daemon-reload

echo "Removed programs and service; managed fans were left at maximum."
echo "Preserved /etc/sysconfig/z440-fan-control for possible reinstallation."
