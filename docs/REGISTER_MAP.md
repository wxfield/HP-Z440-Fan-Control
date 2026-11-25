# HP Z440 register map

This map is specific to the tested HP Z440 board 212B with M60 v02.61 BIOS.

## HWM access

| Item | Address |
|---|---:|
| HWM base | `0x800` |
| Host write control | page 0, offset `0x04` (`0x804`) |
| Page selector | `0x8ff` |

A register written as `0xPPAA` means page `PP`, base offset `AA`.

## Commands and feedback

| Physical fan | Command register(s) | Tach register | Notes |
|---|---:|---:|---|
| CPU0 | `0x0198` | `0x0072` | Direct demand and direct RPM |
| Rear chassis Fan0 | `0x019b`, `0x019c` | `0x006a` | Both demands must be changed |
| Front chassis Fan0 | `0x019d` | `0x006e` | Direct demand and direct RPM |
| Memory Fan0 | `0x019a` | `0x0064` | Shared command with Memory Fan1 |
| Memory Fan1 | `0x019a` | `0x0066` | Shared command with Memory Fan0 |

`0x0198` is described as a PSU PWM on some older HP/coreboot targets. The Z440
mapping above is based on this machine's ACPI sensor descriptors plus controlled
tachometer experiments. Do not transfer physical labels between HP models.

## Six controller levels

| Level | CPU0 `0198` | Rear A `019b` | Rear B `019c` | Front `019d` | Memory `019a` |
|---:|---:|---:|---:|---:|---:|
| 0 | `25` | `2c` | `23` | `25` | `2c` |
| 1 | `37` | `48` | `35` | `37` | `48` |
| 2 | `49` | `64` | `47` | `49` | `64` |
| 3 | `5b` | `7f` | `5a` | `5b` | `7f` |
| 4 | `6c` | `9b` | `6c` | `6c` | `9b` |
| 5 | `7e` | `b7` | `7e` | `7e` | `b7` |

All numbers in this table are hexadecimal bytes recovered from HP's HhmDxe
module and then confirmed on hardware.

## CPU0 calibration

The CPU0 channel was measured long enough to settle at each HP step:

| Command | Approximate settled RPM |
|---:|---:|
| `0x25` | 1,120 |
| `0x37` | 1,660 |
| `0x49` | 2,185 |
| `0x5b` | 2,715 |
| `0x6c` | 3,215 |
| `0x7e` | 3,560 |

RPM is not linearly proportional to the command byte. The daemon uses discrete
validated steps rather than trying to synthesize an arbitrary PWM percentage.

## Write transaction

For each byte:

1. Read page-0 register `0x04` and save the whole byte.
2. Select page 0 and write `0x00` to offset `0x04`.
3. Select the target page.
4. Write the allowlisted value to the target offset.
5. Select page 0 and restore the saved host-control byte.
6. Re-read the target byte and require it to match.

All transactions use `/run/lock/z440-hwmio.lock` to prevent interleaving.
