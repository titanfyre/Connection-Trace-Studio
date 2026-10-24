# Validation status

This is a new engineering preview. No host or guest driver installation, live WFP
capture, Microsoft signing, HVCI runtime test, load/unload stress or Windows network
coexistence validation has been performed for this package yet.

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
identity and INF/SYS catalog membership. Installation remains unavailable until
the signed package is returned.

Required next validation: signed Win10/11 x64 VM install/uninstall, one-owner
exclusion, Start/Stop/crash cleanup, parallel IPv4/IPv6 TCP and UDP generation,
ring overload with visible drops, suspend/restart, verifier and memory integrity,
BFE restart and coexistence with Windows Firewall/VPN/endpoint protection.

The private guest campaign uses the pinned CLI SHA-256, a readiness handshake,
own-process loopback TCP/UDP on IPv4/IPv6, actual event field assertions,
off/on/off path options and termination of its own child to test cleanup.
Preparing and parsing this campaign is not a runtime pass.

On an unexpected persistent WFP unregistration failure, device removal retains
the device and retries at PASSIVE_LEVEL rather than releasing still-registered
callback code. This exceptional path can delay removal; no such delay occurs in
event classification. It remains a required runtime/unload validation item.
