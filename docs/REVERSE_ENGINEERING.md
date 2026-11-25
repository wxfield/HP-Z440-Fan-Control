# Reverse-engineering the HP Z440 fan controller

This document records how the control mechanism was discovered. It is intended
to make the result reproducible and to prevent future work from repeating unsafe
guesses.

## 1. Problem statement

The test HP Z440 ran sustained CPU inference workloads without an audible CPU
fan ramp. Standard Linux interfaces exposed `coretemp`, but neither `pwmconfig`
nor a normal `hwmon` PWM device could control the proprietary HP fan logic.
Setting HP's BIOS **Fan Idle Mode** to its highest value proved that all fans
could run substantially faster, so the problem was control rather than failed
motors or insufficient electrical power.

The target was not merely a fixed full-speed setting. The desired result was a
Linux controller that could:

1. start and fail at maximum airflow;
2. use measured temperature and CPU load;
3. pre-cool before a benchmark;
4. validate commands against tachometer feedback; and
5. return to a quieter idle state.

## 2. Establishing high and low reference states

HP BIOS settings were changed with HP's Replicated Setup utility. A reboot was
performed for each reference state, and all 16 pages of the hardware-monitor
I/O region were sampled repeatedly.

The two useful reference states were:

- lowest BIOS idle preset: CPU fan approximately 549 RPM;
- highest BIOS idle preset: CPU fan approximately 3,560 RPM.

The page-1 bytes that changed consistently were offsets `0x98`, `0x9a`,
`0x9b`, `0x9c`, and `0x9d`. At the maximum preset they were:

```text
0x0198 = 0x7e
0x019a = 0xb7
0x019b = 0xb7
0x019c = 0x7e
0x019d = 0x7e
```

The important notation here is `page << 8 | offset`: `0x0198` means page 1,
offset `0x98` from the HWM base.

## 3. HWM access mechanism

The HP board uses an NPCD378/NPCD379-family Nuvoton Super I/O with a custom HP
hardware-monitor block. On this machine:

```text
HWM I/O base       0x800
host write control 0x804 (page 0, offset 0x04)
page selector      0x8ff
```

The low four bits written to `0x8ff` select one of 16 pages. The public coreboot
documentation describes the same broad mechanism and warns that most registers
remain reverse engineered:

- <https://doc.coreboot.org/superio/nuvoton/npcd378.html>

## 4. The failed write and recovery

An early experiment assumed that disabling writes meant clearing only bit 0 of
the host-control byte. The observed value was `0xe5`, so the experiment wrote
`0xe4`, changed a target register, and restored `0xe5`. Tachometer counters then
stopped updating.

No storage or firmware was modified, but monitoring state was wedged until a
controlled reboot. The machine rebooted with SSH and networking intact, and the
BIOS maximum-fan preset restored safe airflow.

This failure was valuable: the vague public description was not precise enough
for this firmware. Further writes stopped until HP's own implementation was
recovered.

## 5. Recovering HP's exact write protocol

The official HP M60 v02.61 firmware payload was extracted with
[UEFITool/UEFIExtract](https://github.com/LongSoft/UEFITool). The fan/HWM DXE
module was located by GUID:

```text
HhmDxe GUID  0B81B13E-E595-4D35-9E7B-A3C85E01A23F
```

Files used during the analysis, identified by hash only (not redistributed):

```text
M60_0261.BIN SHA256
99e71099ab42faddeba215adb9df76bbadda7ba0799af120895c1df0ef31d738

HhmDxe PE32 SHA256
4c64ca7a5c8e4c00f6401f6a8edddbe8b73acc249637d48b3c31b0b4a8ca6bd1
```

The PE image was analyzed with [Ghidra](https://github.com/NationalSecurityAgency/ghidra).
The decompiled byte-write routine at RVA `0x7528` performed this exact sequence:

```c
saved = read_hwm(0x0004);
select_page(0);
outb(0x00, 0x804);        /* write zero, not saved & ~1 */
select_page(target >> 8);
outb(value, 0x800 + (target & 0xff));
select_page(0);
outb(saved, 0x804);
```

Two details explained the earlier failure:

1. HP writes **zero** to the whole host-control byte before a configuration
   write; it does not merely clear bit 0.
2. HP restores the byte that was read immediately before the transaction; it
   does not restore a value captured during another boot or controller state.

`z440-hwmio` implements that sequence and serializes all processes with
`flock(2)`.

## 6. Recovering the Z440 preset table

The same module contains platform-specific six-step tables. The Z440 platform
marker is `0x20`; the relevant table begins at image offset/RVA `0x3f8` in the
analyzed module:

| Register | Step 0 | Step 1 | Step 2 | Step 3 | Step 4 | Step 5 |
|---|---:|---:|---:|---:|---:|---:|
| `0x0198` | `25` | `37` | `49` | `5b` | `6c` | `7e` |
| `0x019a` | `2c` | `48` | `64` | `7f` | `9b` | `b7` |
| `0x019b` | `2c` | `48` | `64` | `7f` | `9b` | `b7` |
| `0x019c` | `23` | `35` | `47` | `5a` | `6c` | `7e` |
| `0x019d` | `25` | `37` | `49` | `5b` | `6c` | `7e` |

These values were treated as an allowlist. The public helper will not write an
arbitrary byte to an arbitrary HWM register.

The no-plus BIOS baseline used lower internal defaults (`0x0e` or `0x15` on
several channels). They are accepted by the helper for research and recovery,
but the normal daemon deliberately uses the more conservative first table step.

## 7. Identifying physical fan tachometers through ACPI

RPM changes proved that a command worked, but not which physical fan it drove.
The authoritative names came from the `COMPAQ/Plat_Wmi` SSDT.

The SSDT publishes a six-byte descriptor for each named sensor. On the tested
boot, the WMI operation region happened to be at `0xABEF3000`; that physical
address is firmware/boot-specific and must never be hardcoded. Decoding the
descriptors gave:

| ACPI sensor | Descriptor | Direct tach register |
|---|---|---:|
| CPU0 Fan | `00 00 00 72 01 00` | `0x0072` |
| Rear Chassis Fan0 | `00 00 00 6a 01 02` | `0x006a` |
| Front Chassis Fan0 | `00 00 00 6e 00 04` | `0x006e` |
| Memory Fan0 | `00 00 00 64 00 06` | `0x0064` |
| Memory Fan1 | `00 00 00 66 00 07` | `0x0066` |

The values at these offsets are little-endian 16-bit RPM readings on this Z440.
This direct mapping also avoids relying on fan noise, which was misleading when
several other fans remained at maximum.

## 8. Controlled channel mapping

Each command register was lowered one at a time from HP's maximum to a known HP
preset. An independent systemd timer was armed before every probe to restore the
maximum even if the SSH session failed.

Results:

| Command register(s) | Physical result |
|---|---|
| `0x0198` | CPU0 fan (`0x0072`) |
| `0x019a` | Memory Fan0 and Fan1 together (`0x0064`, `0x0066`) |
| `0x019d` | Front chassis fan (`0x006e`) |
| `0x019b` and `0x019c` together | Rear chassis fan (`0x006a`) |

The rear fan was the subtle case. Lowering either `0x019b` or `0x019c` alone
did nothing because the other maximum demand still won. Lowering both caused
the rear fan to fall from roughly 3,500 to 1,380 RPM.

## 9. Transient zero tach readings

A host-control commit briefly clears or invalidates the tach latch. Reading a
tach immediately after a write can therefore return zero even while the fan is
spinning normally. Independent reads one second later were valid.

The daemon handles this in two ways:

- it waits one second after changing any command;
- it requires three consecutive unsafe samples before exiting.

The first unsafe sample still commands maximum immediately. systemd then
restarts a failed daemon, and both `ExecStartPre` and `ExecStopPost` set every
managed channel to maximum.

## 10. From discovery to controller

The final design combines:

- a small C helper with DMI, register, value, and process-lock safeguards;
- a Bash policy daemon using `coretemp` package temperature;
- CPU busy-time feed-forward so airflow rises before temperature;
- immediate ramp-up and delayed ramp-down;
- feedback validation for all five populated tach channels;
- a timed boost file for pre-cooling; and
- a hardened systemd unit with `CAP_SYS_RAWIO` as its only added capability.

Measured validation is recorded in [VALIDATION.md](VALIDATION.md). A method for
porting the research—not the Z440 values—to another HP platform is documented in
[PORTING.md](PORTING.md).
