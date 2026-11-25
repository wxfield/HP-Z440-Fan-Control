# Safety and recovery

## Safety boundaries in v1.0.0

- Refuses non-root execution.
- Refuses raw I/O unless DMI contains `Z440`.
- Serializes every operation with an exclusive lock.
- Writes only five discovered fan command registers.
- Writes only HP-derived preset values for each register.
- Restores HP's host-control byte after every transaction.
- Verifies each command by reading it back.
- Starts and stops with all managed fans at maximum.
- Raises speed immediately and delays lowering.
- Treats a missing/invalid temperature as a failure.
- Treats tachometer values below 500 RPM, `0xffff`, or nonnumeric data as unsafe.
- Commands maximum on the first bad tach sample and exits after three consecutive
  bad samples so systemd can recover it.
- Provides a timed maximum-speed boost which expires automatically.

## Normal emergency command

The safest supported way to stop dynamic control is:

```bash
sudo systemctl stop z440-fan-control.service
```

`ExecStopPost` writes every managed channel to its maximum preset.

To keep the service running but force maximum for ten minutes:

```bash
sudo z440-fan-boost 600
```

## Verify actual feedback

```bash
sudo z440-hwmio snapshot
cat /sys/class/hwmon/hwmon*/temp*_input
journalctl -u z440-fan-control.service -n 100 --no-pager
```

At maximum on the tested machine, expected approximate RPM was:

- CPU0: 3,560;
- rear chassis: 3,500;
- front chassis: 3,650;
- memory Fan0/1: 5,500 / 5,500.

At controller level 0, expected approximate RPM was:

- CPU0: 1,140;
- rear chassis: 1,390;
- front chassis: 1,110;
- memory Fan0/1: 1,380 / 1,310.

Different fan revisions can legitimately report different RPM. The critical
properties are nonzero stable feedback and rising RPM as the level increases.

## If tachometers freeze

Do not continue writing registers. Stop workloads and reboot the machine once.
The original research found that an incorrect host-control sequence could freeze
telemetry until reboot even though fans continued spinning.

After reboot, verify:

```bash
sudo z440-hwmio snapshot
systemctl is-active z440-fan-control.service
```

Do not loop reboots on a remotely inaccessible system.

## Unsupported machines

The DMI override exists only for a developer who has independently completed
the read-only and controlled-probe work in [PORTING.md](PORTING.md):

```bash
export Z440_HWMIO_UNSAFE_ALLOW_OTHER_PLATFORM=YES-I-ACCEPT-THE-RISK
```

That variable disables only the product-name check. It does not make Z440
registers correct for another board. The write allowlist still applies and must
be changed in source as part of a real port.
