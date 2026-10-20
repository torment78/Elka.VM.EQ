ELKA VM EQ - 0.3.1
Standalone Windows x64 VoiceMeeter EQ controller by ElkaSoft.

DOWNLOAD
Portable ZIP and Inno Setup installer:
https://github.com/torment78/Elka.VM.EQ/releases/latest

PORTABLE
Extract the ZIP and run ElkaVmEq.exe. The application is a single executable:
the .NET runtime and native EQ engine are bundled. No separate .NET installation,
loose DLLs, or runtimeconfig.json are needed. Keep the included Notices folder
when redistributing the package. You can move the portable folder or run it
from another location.

Native runtime components unpack automatically into Windows' temporary .net
cache when the program starts. This is normal single-file .NET behavior: it is
not a completely zero-footprint application. No audio is recorded to disk.
The app does not create an installer entry when run from the portable ZIP.

INSTALLER
Run Elka-VM-EQ-0.3.1-Setup-x64.exe. It installs for the current user under:
%LOCALAPPDATA%\Programs\ElkaSoft\Elka VM EQ
A Start menu shortcut is included; a desktop shortcut is optional.
Remove it through Windows Installed apps. No separate runtime download is needed.
This release is unsigned; Windows may show an unknown-publisher warning.

REQUIREMENTS
Windows 10/11 x64 and an installed compatible VoiceMeeter.
Potato: six-band EQ on IN 1-5 and buses A1-A5/B1-B3.
Banana: six-band bus EQ on A1-A3/B1-B2; not Potato's input EQ.
Standard does not provide the six-band EQ controlled by this app.
VoiceMeeter performs the actual audio processing. This is a controller, not a
standalone replacement for VoiceMeeter. Its API DLL is not redistributed here.

QUICK START
Start VoiceMeeter, then open ElkaVmEq.exe.
Choose Strip or Bus, then the target. Select one of the six bands.
Drag its graph point or turn the Frequency, Gain and Q knobs.
Scroll for adjustments; hold Shift for fine adjustment.
Right-click a knob for numeric entry. Double-click a knob/point to reset it.
All links the EQ channels; numbered channel buttons edit a single channel.
A/B selects VoiceMeeter's own EQ bank. EQ ON/OFF preserves your values.
Solo curve changes only the drawing, not the audio routing.
Drag the title area to move; drag the window edge to resize proportionally.

OPTIONAL RTA
RTA is OFF at startup. Close ElkaFX and any competing callback application before
turning it on. Turn RTA off or close this EQ before restarting callback software.
It will not take over a busy callback. Select a named post-master bus mix to
monitor processed output. The strip tap is documented only after input insert,
not as post-strip-EQ. A bus mix includes all inputs routed to that bus.
All displays the maximum spectrum of the channels without summing their phase.
The muted spectrum shares the curve's frequency axis and left dB scale. It uses
peak dBFS, not an RMS or broadband VU reading. The cyan line is a calculated EQ
response, not a measurement of VoiceMeeter's unpublished filter coefficients.

PROJECT
https://github.com/torment78/Elka.VM.EQ
Report issues: https://github.com/torment78/Elka.VM.EQ/issues
Build and validation details: BUILDING.txt in the source repository.
More detailed controls and analyzer notes: README-HYBRID.txt in the repository.

Independent project; not affiliated with VB-Audio. VoiceMeeter is required
separately. Bundled Microsoft runtime licenses/notices are in Notices/.
