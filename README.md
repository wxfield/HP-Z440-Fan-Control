# HP Z440 Fan Control

Linux fan monitoring and fail-safe dynamic control for the HP Z440 Workstation.

This project controls the Z440's CPU, rear chassis, front chassis, and paired
memory fans through the Nuvoton/HP hardware-monitor block. It was developed by
comparing HP firmware presets, reverse engineering HP's `HhmDxe` UEFI module,
decoding the platform's ACPI WMI sensor descriptors, and validating each
channel against measured tachometer feedback.

> **Danger:** This code performs direct x86 I/O-port writes as root. An
> incorrect register, value, or write sequence can stop a fan, corrupt hardware
> monitoring state, overheat components, or require a reboot to recover. Version
> 1.0.0 refuses writes outside the discovered Z440 fan registers and HP-validated
> preset values, but it is still low-level experimental software. Read
> [SAFETY.md](docs/SAFETY.md) before installing it.

## Tested platform

| Item | Tested value |
|---|---|
| System | HP Z440 Workstation |
| System board | HP 212B |
| BIOS | M60 v02.61, 2023-03-23 |
| CPU | Intel Xeon E5-2697 v3 |
| OS | Rocky Linux 9.8, x86-64 |
| Kernel | 5.14.0-687.10.1.el9_8.0.1.x86_64 |
| Super I/O HWM base | `0x800` |

The I/O helper checks DMI and refuses to run on a machine whose product name
does not contain `Z440`. Register meanings can differ on other HP products,
even when they use an NPCD378/NPCD379-family controller.

## What it provides

- `z440-hwmio`: locked, DMI-guarded register and tachometer access.
- `z440-fan-control`: temperature and CPU-load controller with six fan levels.
- `z440-fan-boost`: timed full-speed pre-cooling for builds and benchmarks.
- `z440-fan-control.service`: hardened systemd service with maximum-speed
  startup, shutdown, failure, and restart behavior.
- Reproducible research notes, register maps, calibration data, and validation.

The controller raises speed immediately and waits before lowering it. CPU load
acts as feed-forward control, so a heavy job ramps airflow before the heatsink
becomes hot. Invalid temperature or tachometer data forces all managed fans to
maximum.

## Quick start

Requirements are Linux on x86, root access, GCC, Bash, systemd, the `coretemp`
driver, and an HP Z440 matching the documented register layout.

```bash
git clone https://github.com/wxfield/HP-Z440-Fan-Control.git
cd HP-Z440-Fan-Control
make check
sudo ./scripts/install.sh
```

The installer compiles the helper, installs files under `/usr/local/sbin`,
installs `/etc/systemd/system/z440-fan-control.service`, and enables the service.

Check operation:

```bash
sudo systemctl status z440-fan-control.service
sudo journalctl -u z440-fan-control.service -f
sudo /usr/local/sbin/z440-hwmio snapshot
```

Pre-cool at full speed for five minutes:

```bash
sudo /usr/local/sbin/z440-fan-boost 300
```

Stopping the controller intentionally leaves every managed channel at maximum:

```bash
sudo systemctl stop z440-fan-control.service
```

## Control policy

| Level | Package temperature | CPU PWM | Rear PWM A/B | Front PWM | Memory PWM |
|---:|---:|---:|---:|---:|---:|
| 0 | below 36 C | `0x25` | `0x2c` / `0x23` | `0x25` | `0x2c` |
| 1 | 36–40 C | `0x37` | `0x48` / `0x35` | `0x37` | `0x48` |
| 2 | 41–45 C | `0x49` | `0x64` / `0x47` | `0x49` | `0x64` |
| 3 | 46–50 C | `0x5b` | `0x7f` / `0x5a` | `0x5b` | `0x7f` |
| 4 | 51–55 C | `0x6c` | `0x9b` / `0x6c` | `0x6c` | `0x9b` |
| 5 | 56 C and above | `0x7e` | `0xb7` / `0x7e` | `0x7e` | `0xb7` |

CPU busy time can impose a higher minimum: 10% selects at least level 2, 20%
selects at least level 3, and 35% selects level 5. Upward changes are immediate;
downward changes require five consecutive two-second cycles by default.

Settings in `/etc/sysconfig/z440-fan-control` can change polling, logging,
lowering dwell, and the minimum safe tachometer value.

## Documentation

- [How the control mechanism was discovered](docs/REVERSE_ENGINEERING.md)
- [Registers, presets, and physical fan mapping](docs/REGISTER_MAP.md)
- [Safety model and recovery](docs/SAFETY.md)
- [Measured validation results](docs/VALIDATION.md)
- [Adapting the method to another HP platform](docs/PORTING.md)
- [v1.0.0 release notes](docs/RELEASE_NOTES_v1.0.0.md)

## Important limitations

- Only the platform above has been physically tested.
- HP does not publish a complete NPCD378/NPCD379 HWM datasheet. Much of this
  interface is proprietary and was determined empirically.
- ACPI and firmware layouts may change with another BIOS release.
- This is not a generic `hwmon` driver. It deliberately exposes only the small
  validated command surface required by this controller.
- A system firmware thermal shutdown remains the final protection layer. Do not
  disable it.

## License

MIT. See [LICENSE](LICENSE).
