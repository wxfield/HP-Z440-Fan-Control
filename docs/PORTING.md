# Porting the method to another HP workstation

Do not begin a port by running the Z440 writer. Begin with evidence.

HP used related Nuvoton controllers across multiple workstation and business-PC
families, but register-to-fan mappings are platform specific. Even the public
coreboot name for `0x0198` on older boards differs from the physical Z440 fan
identified by this machine's ACPI descriptors.

## Stage 0: recovery and physical safety

Before any low-level work:

1. Arrange physical console and power access.
2. Stop production workloads.
3. Confirm the firmware thermal shutdown remains enabled.
4. Record network and boot recovery steps.
5. Verify a known BIOS setting can force high fan speed.
6. Do not work on a machine whose fan is already unreliable.

Remote-only reverse engineering is not recommended.

## Stage 1: inventory

Record at minimum:

```bash
cat /sys/class/dmi/id/{sys_vendor,product_name,board_name,bios_version,bios_date}
uname -a
sudo dmidecode --type system --type baseboard --type bios
sudo acpidump
```

Store exact firmware filenames, official source URLs, sizes, and SHA256 hashes.
Never redistribute a proprietary BIOS image in this repository.

## Stage 2: collect read-only high/low snapshots

Use the vendor's supported BIOS fan-idle setting to create a clearly low state
and a clearly high state. Reboot between states. Collect repeated HWM snapshots
without writing HWM registers.

The Z440 used an HWM base of `0x800` and page selector `0x8ff`; do not assume
those addresses on another board. Derive them from ACPI, firmware, Super I/O
configuration, or a board-specific source.

Compare only stable changes across multiple samples. Temperatures, tachometers,
and controller state bytes change naturally, so a one-shot binary diff creates
many false leads.

## Stage 3: analyze firmware

Use the exact official firmware version running on the machine.

Suggested tools:

- [UEFITool/UEFIExtract](https://github.com/LongSoft/UEFITool)
- [Ghidra](https://github.com/NationalSecurityAgency/ghidra)
- `iasl` from ACPICA

Search extracted modules for HWM base addresses, page-selection writes, the
host-control register, and the stable high/low bytes. Recover the platform ID,
preset table, and actual write sequence from code rather than inferring it from
a nearby model.

For the Z440, HP's function saved the complete host-control byte, wrote zero,
performed one target write, and restored the saved byte. A superficially similar
bit-clear sequence froze telemetry and was rejected.

## Stage 4: decode physical sensor names

Disassemble DSDT/SSDT tables and look for HP's `Plat_Wmi` table or equivalent.
On the Z440, six-byte sensor descriptors connected human names such as
`CPU0 Fan` and `Rear Chassis Fan0` to direct HWM tach offsets.

Do not hardcode an ACPI `SystemMemory` operation-region address. It can be
allocated differently by another firmware revision or boot.

If a supported Linux `hwmon` driver exposes the same names, use it as a second
source. The upstream Linux documentation for HP WMI sensors is useful context:

- <https://github.com/torvalds/linux/blob/master/Documentation/hwmon/hp-wmi-sensors.rst>

## Stage 5: build a read-only helper first

Your first platform helper should support only page selection and register
reads. Confirm that:

- high/low reference bytes reproduce;
- tachometer values are stable and plausible;
- reads do not disturb the page or host-control state; and
- concurrent readers are serialized.

Do not add a generic `write REGISTER VALUE` command to a public tool.

## Stage 6: controlled one-register probes

Only after the vendor's exact write sequence and known presets have been
recovered:

1. Begin at the known maximum state.
2. Arm an independent timed restoration to maximum.
3. Change one candidate to a vendor-known lower value.
4. Sample every tach and temperature.
5. Restore maximum immediately.
6. Allow RPM to recover before the next candidate.
7. Stop if feedback freezes, becomes implausible, or any unexpected channel
   changes.

Some physical fans can have multiple demand inputs. If lowering candidate A or
B alone has no effect, it can mean the fan selects `max(A, B)`. Test a combined
change only when both candidates are already supported by firmware evidence.

## Stage 7: calibrate discrete levels

Use vendor-derived steps rather than arbitrary bytes. Allow enough time for
each fan's inertia and firmware ramp rate. Record raw samples, ambient/package
temperature, firmware version, and the exact command bytes.

Reject a level if a populated fan approaches stall speed or feedback becomes
unstable.

## Stage 8: design failure toward cooling

A production controller should:

- command maximum before starting;
- command maximum on every exit path;
- raise immediately and lower slowly;
- validate temperature and every populated tach;
- treat sensor loss as a failure;
- limit writable registers and values in the privileged helper;
- use DMI/platform guards;
- serialize raw I/O;
- restart with rate limiting; and
- offer a time-limited manual boost.

## Stage 9: validation order

1. Static compile and shell/unit checks.
2. Read-only snapshot.
3. Idle transition with continuous tach monitoring.
4. Manual timed boost.
5. Short load with a thermal abort threshold.
6. Longer representative workload.
7. Service stop/failure test (must leave maximum).
8. Reboot/startup test with physical recovery available.

## What to include in a contribution

- exact HP model, board ID, BIOS version/date;
- firmware source and SHA256 (not the binary);
- extracted module GUID and hash;
- ACPI sensor descriptors;
- high/low snapshots;
- register/preset table;
- controlled mapping evidence;
- calibrated RPM data;
- failure and rollback results; and
- a clear list of tests not performed.
