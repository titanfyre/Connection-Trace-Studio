# Validation status

## Returned Microsoft package (2026-10-25)

The Microsoft Hardware Compatibility Publisher catalog, SYS/INF membership and
preserved submitted executable content were checked. The self-contained console
installer then installed the exact returned package on Windows 10 build 19045
x64; existing real-driver functional tests passed, and the owned node was removed
and the driver unloaded. The VM's original baseline and nine historical snapshots
were preserved. This campaign used the Microsoft-signed SYS/CAT without adding a
guest test signature. The guest was in test mode for a separate unsigned network
project, so Secure Boot/HVCI compatibility is not established by this result.
The returned package also passed the repeated real-driver campaign with targeted
standard Driver Verifier enabled. The UI install/uninstall/recovery paths and
Windows 11 still need broader testing.

This is a new engineering preview. The final sensor passed one isolated,
test-signed Windows 10 x64 VM runtime campaign on 2026-10-24, then repeated it
with targeted Driver Verifier enabled (flags 0x001209BB). It is not yet a
Microsoft-signed production package. No driver was installed or activated on the
host. Test signing was limited to the disposable guest and its baseline restored.

## Windows 10 runtime results

Guest Windows version: 10.0.19045.0. Each loopback fixture was matched to its real
generator PID, protocol, address family and ephemeral destination port:

| Fixture | Authorize | Established / authorized | Endpoint closed |
|---|---:|---:|---:|
| TCP IPv4 | 1 | 1 | 1 |
| TCP IPv6 | 1 | 1 | 1 |
| UDP IPv4 | 1 | 1 | 1 |
| UDP IPv6 | 1 | 1 | 1 |

The same SDK session passed Start/Stop with path Options **0 -> 1 -> 0**.
The default capture had no application-path metadata, no dropped events and no
report truncation. Abrupt termination of the owned CLI client released capture
and dynamic WFP objects: the first new-owner probe succeeded. The exact tested
driver was removed and unloaded. The VM returned to its original Off baseline
with all nine historical checkpoints preserved; host security was unchanged.
The four fixture results, options, crash cleanup and unload also passed with
Verifier enabled for the sensor. This is not an exhaustive stress or fuzz campaign.

The unsigned submission input and the guest test-signed installed file have
different whole-file SHA-256 values because test signing changes the file. The
preserved Verifier guest image and unsigned submission have matching SHA-256
Authenticode-protected PE content. The private evidence records those digests and
the manifest-to-install pipeline; this does not make the test certificate a
Microsoft signature.

The VM campaigns used the archived CLI build before its final pure JSON-format
change: endpoint identifiers are now exact hexadecimal strings instead of JSON
numbers. The final CLI binary has since passed the returned-package campaign below. The kernel and core SDK
are unchanged; the formatter was checked offline in Release and ASAN with 0,
2^53+1 and UINT64_MAX, including exact `ConvertFrom-Json` string round trips.

## Offline checks

Offline validation covers the public protocol decoder, bounded event correlation,
partial lifecycle data, invalid lengths/reserved fields, randomized records,
untrusted catalog rejection and unrelated INF rejection. The private actual ring
indexing helper is compared against a reference queue for 1,100,000 operations,
including empty reads, wraparound, overwrite/drop counts and resets under ASAN.
These checks never open
a sensor or WFP session and cannot prove kernel runtime stability.

Private build validation uses WDK 26100, W4/WX, PREfast, CFG, InfVerif /w and /u,
Inf2Cat, and SHA-256 verification of expanded submission payloads. Driver source
remains private. The installer verifies Microsoft catalog trust, exact package
identity and INF/SYS catalog membership. The signed evaluation package is now included in the release installers.

Required next validation: the signed package on Windows 11 x64,
extended one-owner/Start/Stop/crash stress, ring overload with visible drops,
suspend/restart, longer Verifier stress and memory integrity, BFE restart and
coexistence with Windows Firewall/VPN/endpoint protection.

The private guest campaign uses the pinned CLI SHA-256, a readiness handshake,
own-process loopback TCP/UDP on IPv4/IPv6, actual event field assertions,
off/on/off path options and termination of its own child to test cleanup.
Those cases were executed in the test-signed Windows 10 guest described above.
They are functional smoke checks, not extended stress or proof of compatibility
with every network stack, firewall or Windows installation.

On an unexpected persistent WFP unregistration failure, device removal retains
the device and retries at PASSIVE_LEVEL rather than releasing still-registered
callback code. This exceptional path can delay removal; no such delay occurs in
event classification. It remains a required runtime/unload validation item.
