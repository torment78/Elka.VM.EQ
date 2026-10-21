# Elka VM EQ 0.3.2

Six-band EQ control for VoiceMeeter, with a live channel-level readout and refreshed ElkaSoft artwork.

## Changes

- Added the live dBFS readout beside the engine label. It follows the selected strip/bus and EQ channel, works with RTA off, and clears unavailable or stale readings.
- Added the new app icon, horizontal banner and portrait artwork, including sharing versions below 1 MB.
- Updated the dark installer to use the matching artwork.
- Added compact orange heart / Donate buttons to the app and installer. Ko-fi opens only when clicked.
- Updated the repository front page with the banner, installation instructions, controls and analyzer guidance.

## Downloads

- **Setup-x64.exe**: per-user installer, with Start menu and optional desktop shortcuts.
- **Portable-win-x64.zip**: extract and run `ElkaVmEq.exe`; the native engine and .NET runtime are bundled.
- **Artwork.zip**: icons and portrait/landscape banners, including versions under 1 MB.
- **SHA256SUMS-0.3.2.txt**: checksums for all three downloads.

Close an older Elka VM EQ before updating. VoiceMeeter must be installed separately. Potato supports hardware-input and bus EQ; Banana supports bus EQ. Standard does not provide the six-band EQ controlled here.

**Unsigned Windows x64 build:** Windows may show an unknown-publisher warning. Download from this repository and verify the supplied checksums.

## Optional analyzer

RTA starts off. Close ElkaFX and other VoiceMeeter callback software before enabling it; turn RTA off before restarting them. Use a named post-master bus mix to inspect processed output. The strip tap is not documented as post-strip-EQ, and a bus contains all routed inputs.

The live header reports overall level while the RTA reports individual frequency components. Speech/music spectrum peaks normally sit below the overall reading. The user's live 500 Hz tone comparison matched.

## Validation

Four native test suites and 68 WPF integration checks passed. The final bundled executable passed the same offline checks after installation. Separate-identity installer tests verified payload hashes, shortcuts, reinstall, uninstall and preservation of user-created files. These checks did not activate an audio callback.

[Setup and controls](https://github.com/torment78/Elka.VM.EQ#quick-start) · [Report an issue](https://github.com/torment78/Elka.VM.EQ/issues) · [♥ Donate](https://ko-fi.com/msffixit)
