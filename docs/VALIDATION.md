# Validation record

Date: 2026-11-24 (America/New_York)

## Platform

| Item | Value |
|---|---|
| Product | HP Z440 Workstation |
| Board | HP 212B |
| BIOS | M60 v02.61, 2023-03-23 |
| CPU | Intel Xeon E5-2697 v3, 14 cores / 28 threads |
| Memory | 128 GiB DDR4 ECC |
| OS | Rocky Linux 9.8 |
| Kernel | 5.14.0-687.10.1.el9_8.0.1.x86_64 |

## Static and structural checks

- C helper compiled with `gcc -O2 -Wall -Wextra -Werror`.
- Bash programs passed `bash -n`.
- systemd unit passed `systemd-analyze verify`.
- Service ran with a restricted capability set containing `CAP_SYS_RAWIO`.
- Register writes were read back and compared.
- The helper denied non-fan registers and values outside its preset allowlist.

## CPU command calibration

Five two-second samples were collected at every HP-derived CPU command. The
settled end of each interval was:

| Command | Representative settled RPM | Package temperature |
|---:|---:|---:|
| `0x7e` | 3,562 | 28 C |
| `0x6c` | 3,211 | 29 C |
| `0x5b` | 2,711 | 29 C |
| `0x49` | 2,184 | 28 C |
| `0x37` | 1,657 | 29 C |
| `0x25` | 1,121 | 28 C |

Raw summary data is in
[`evidence/cpu-calibration-summary.csv`](../evidence/cpu-calibration-summary.csv).

## Dynamic-load tests

`stress-ng` ran 14 `matrixprod` CPU workers for 20 seconds. A separate monitor
would terminate the test above 75 C.

Observed behavior:

- controller moved from idle to an intermediate load level on the first sample;
- controller selected maximum within approximately four seconds;
- CPU fan reached approximately 3,580 RPM;
- package temperature peaked at 52 C;
- no thermal abort, fan fault, or service restart occurred;
- the controller waited after load completion and returned to idle.

`stress-ng --verify` reported all 14 workers passed, with no failed or skipped
stressors.

The exact packaged v1.0.0 installation was then tested again with 14
`matrixprod` workers for 15 seconds. All five managed channels selected their
maximum presets. Package temperature peaked at 53 C, the thermal guard did not
fire, all workers passed verification, and `z440-fan-control.service` remained
active. The raw series is in
[`evidence/v1.0.0-release-loadtest.csv`](../evidence/v1.0.0-release-loadtest.csv).

## Final all-channel measurements

The final controller state at idle:

| Fan | Idle RPM |
|---|---:|
| CPU0 | 1,138 |
| Rear chassis Fan0 | 1,395 |
| Front chassis Fan0 | 1,103 |
| Memory Fan0 | 1,329 |
| Memory Fan1 | 1,288 |

During timed boost, the channels reached approximately:

| Fan | Boost RPM |
|---|---:|
| CPU0 | 3,607 |
| Rear chassis Fan0 | 3,482 |
| Front chassis Fan0 | 3,629 |
| Memory Fan0 | 5,514 |
| Memory Fan1 | 5,509 |

See [`evidence/all-channel-boost.csv`](../evidence/all-channel-boost.csv).

## Fail-safe tests

- Stopping the service invoked maximum-speed `ExecStopPost` commands.
- A deliberately immediate post-write tach read returned a transient zero;
  the daemon commanded maximum, exited safely, and systemd restarted it.
- The implementation was then corrected to wait one second after commits while
  retaining a three-consecutive-sample failure threshold.
- A timed systemd restoration to maximum was armed during every exploratory
  low-speed probe.
- The BIOS idle preset was left at maximum during development as an additional
  reboot-level recovery state.

## Reboot and remote-access test

The exact v1.0.0 installation was enabled and the Z440 was rebooted once. On
the first boot attempt:

- LAN SSH access returned;
- Tailscale access returned;
- `z440-fan-control.service` started automatically and remained active;
- every maximum-speed `ExecStartPre` write completed successfully;
- the controller transitioned from maximum to level 0 after its lowering dwell;
- all five fan tachometers remained valid; and
- idle package temperature settled near 30–32 C.

The post-boot snapshot reported approximately 1,134 RPM CPU, 1,401 RPM rear,
1,101 RPM front, and 1,335/1,294 RPM for the paired memory fans.

## Not claimed

- No other HP model or BIOS release has been validated.
- No firmware was flashed or modified.
- The repository does not contain HP firmware.
- Long-duration thermal qualification remains workload-, cooler-, ambient-, and
  dust-dependent. Operators must monitor their own system.
