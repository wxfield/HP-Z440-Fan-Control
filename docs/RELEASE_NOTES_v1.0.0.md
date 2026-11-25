# HP Z440 Fan Control v1.0.0

Initial public-ready release of the reverse-engineered HP Z440 Linux fan
controller.

## Features

- DMI-guarded, locked raw HWM I/O helper.
- Exact HP HhmDxe host-control write protocol.
- Strict allowlist of Z440 fan registers and HP-derived preset values.
- Physical mapping for CPU0, rear chassis, front chassis, and Memory Fan0/1.
- Six-level temperature curve with CPU-load feed-forward.
- Immediate ramp-up and delayed ramp-down.
- Feedback checks for every populated fan tachometer.
- Fail-to-maximum behavior on startup, shutdown, invalid sensors, and crashes.
- Timed full-speed boost for workload pre-cooling.
- Hardened systemd service.
- Installer, uninstaller, static checks, and CI workflow.
- Complete reverse-engineering, safety, porting, register, and validation docs.

## Tested configuration

HP Z440 board 212B, M60 v02.61 BIOS, Xeon E5-2697 v3, Rocky Linux 9.8.
The packaged installation passed static checks, guarded-write rejection tests,
a verified 14-worker CPU load test, and an enabled-service reboot test while
preserving both LAN SSH and Tailscale access.

## Known limitations

- Other HP systems and BIOS versions are not supported merely because they use
  a related Nuvoton controller.
- Root and `CAP_SYS_RAWIO` are inherently required by this implementation.
- This is not an upstream Linux `hwmon` driver.
- Fan RPM and thermal performance vary with the installed fan/cooler revision.

## Upgrade and rollback

This is the first release. Stop the service to force maximum before replacing
files. `scripts/uninstall.sh` disables the service, forces maximum, removes the
installed executables and unit, and preserves the sysconfig file.
