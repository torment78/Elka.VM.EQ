ELKA VM EQ HYBRID - 0.3.1 preview

A standalone EQ controller with a native C backend and WPF interface.
The interface uses retained vector graphics and GPU acceleration when WPF
supports it. The C DLL owns the VoiceMeeter API connection and EQ calculations.
VoiceMeeter still performs all actual audio processing. The optional spectrum
analyzer uses a pass-through MAIN audio callback only when you enable RTA.

START
Public releases use a single self-contained ElkaVmEq.exe; .NET is bundled.
The developer folder built by package-hybrid.ps1 still requires .NET 8 Desktop
Runtime x64 and its adjacent DLLs. Both versions require installed VoiceMeeter.
This is separate from VM Mini and ElkaFX; neither app is modified.
The older all-GDI build remains available in artifacts/Elka VM EQ Native.

SELECT AN EQ
Use the Strip/Bus dropdown, then choose the target in the next dropdown.
Potato: IN 1-5 (2 EQ channels) and A1-A5/B1-B3 buses (8 EQ channels).
Banana: A1-A3/B1-B2 buses. Its IN 1-3 do not expose Potato's six-band input EQ.
Standard does not expose this six-band EQ. Unsupported EQ controls are disabled.
Selecting an EQ only reads it; opening the app does not reset your settings.

CONTROLS
Select one of the six bands, then drag its point or the Frequency/Gain/Q knobs.
Scroll to adjust; hold Shift for fine adjustment. Scroll over the graph for Q.
Double-click a knob to reset its value; double-click a point to reset its band.
Right-click a knob for numeric entry. Enter applies and Escape cancels.
Tab focuses controls; knob arrow keys adjust, Home resets, Enter edits numerically.
Pass/notch/bandpass filters use zero gain; their points move horizontally only.
Numbered points represent band frequency/gain, not the combined gain of all bands.

All links edits across the selected EQ's channels. Individual channel buttons
edit only that channel. A/B selects VoiceMeeter's native EQ bank.
EQ ON/OFF bypasses/enables the EQ while retaining its settings.
BAND ON toggles only the selected band. FLAT resets all selected-channel bands;
Reset band resets one band, and Flat band makes it a flat peak filter.
Solo curve isolates the drawing only; it does not solo or reroute audio.
Ch.Copy copies one channel; Copy All copies every channel. Paste applies to
selected channels. Cross-strip/bus copying works with Ch.Copy; Copy All requires
matching channel counts. The app clipboard is kept only for the current session.

DISPLAY
The vertical grid defaults to +24/-54 dB, with zero above the centre. The range
dropdown offers +24/-54, +36/-72, +18/-18, and +18/-36 dB. This changes the view,
not the audio or knob limits. Positive ticks are 0, +3, +6, +12, +18, +24, +36
where the chosen range includes them. Negative ticks use -3, -6, -12, -18, -24,
-36, -48, -60, -72 where included, plus the selected lower endpoint (-54, etc.).
Grid lines, response curves, band points and drag sensitivity share the scale.
The cyan curve is the calculated response. Fainter curves show individual bands
and, in All mode, other channels when their settings differ. A selected-band
fill helps identify the band being edited. Offscale curves/points are clipped
at the display edge; knobs and numeric entry still reach the full control range.
Curve calculations are cached and run off the UI thread. Resizing scales retained
graphics without recomputing EQ response. Minimized windows stop UI polling.
Drag the title area to move; drag the edges to resize with the aspect ratio locked.

OPTIONAL LIVE SPECTRUM
RTA is OFF every time the app opens. Close ElkaFX and other audio callback
applications before enabling it. Click RTA OFF and confirm to connect.
Turn RTA off (or close this EQ) before starting another callback application.
Busy registration is reported without stopping or taking over another owner.
Disconnects and stream changes require manual re-enabling; no engine is restarted.

The source dropdown can follow the selected EQ or monitor a named bus:
- Bus: MAIN post-master output, after the bus processing and master gain.
- Strip: MAIN input after the input insert. The SDK does not identify this as
  post-strip-EQ, so do not use it as proof of that EQ's processed output.
- While editing a strip, choose its destination bus to see the final mix.
  This includes every strip routed to that bus, not an isolated strip.
The channel buttons also choose the analyzer channel. All shows the per-frequency
maximum of the channels, without phase cancellation from summing them together.

The muted filled spectrum sits behind the grid/curve and uses the same 20 Hz to
20 kHz logarithmic frequency axis and left dB scale. It is peak-amplitude dBFS,
not an RMS meter, not a gain-response curve, and has no automatic level offset.
A sine wave peaking at -6 dBFS should show -6 dBFS. Complex broadband signals
spread energy across bins and do not equal a broadband VU/peak meter reading.
The analyzer has fast rise and smooth decay, a 4096-sample Hann window, and
zero-padded FFT. Frequency detail depends on sample rate and window length.
It copies samples to a bounded queue and performs FFT work outside the callback.
The callback does not change gain, routing, EQ or samples; it passes all buses
through even when spectrum analysis cannot keep up. No audio is saved to disk.

ACCURACY / LIMITS
The curves use RBJ biquad maths, not VoiceMeeter's unpublished coefficients.
The cyan line is a conventional calculated EQ preview, not an exact copy of
VoiceMeeter's graph. The optional background RTA is measured audio, independent
of that preview. Boosting a calculated EQ line does not fabricate RTA peaks.
Banana bus editing uses documented legacy bounds (-12..+12 dB, Q 1..100).
Potato editing uses -36..+18 dB and Q 0.3..100. API values are read back after edits.
Rendering smoothness still depends on graphics hardware, drivers and display size.

BUILD / TEST
Open Elka.VM.EQ.slnx in Visual Studio 2026 with C++ desktop tools and .NET/WPF.
Elka.VM.EQ is the executable startup project. Building it also builds the C DLL.
Alternatively run scripts/build.ps1: native tests plus WPF integration tests.
Run scripts/package-hybrid.ps1 for the portable framework-dependent folder.
Native C-only solution: build/ElkaVmEqNative.slnx (kept for engine development).

ElkaVmEq.exe --demo                Offline interactive preview; no API connection.
ElkaVmEq.exe --demo --demo-bus     Offline eight-channel bus preview.
ElkaVmEq.exe --render preview.png  Offline PNG render without showing a window.
Add --demo-spectrum to an offline render/demo for a labelled synthetic preview.
ElkaVmEq.exe --self-test           Offline C/WPF tests with an offscreen test window.
ElkaVmEq.exe --probe               Read-only API probe; writes hybrid-probe.txt.
The probe checks available EQ targets without writing any VoiceMeeter parameters.
ElkaVmEq.exe --spectrum-probe      Explicit LIVE callback test, not an offline test.
Only run this with other callback software closed. It monitors A1 briefly,
releases/reacquires its own callback, and writes spectrum-probe.txt (no audio).

SOURCES
https://github.com/vburel2018/Voicemeeter-SDK
https://download.vb-audio.com/Download_CABLE/VoicemeeterRemoteAPI.pdf
https://vb-audio.com/Voicemeeter/VoicemeeterPotato_UserManual.pdf
https://www.w3.org/TR/audio-eq-cookbook/

This independent project does not redistribute VB-Audio's Remote API DLL.
