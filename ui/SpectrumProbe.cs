using System.IO;
namespace Elka.VM.EQ;

// Explicit opt-in integration check. Never called by the default build or UI tests.
internal static class SpectrumProbe
{
    internal static async Task<int> Run()
    {
        var log = new List<string>(); bool passed = false;
        using var engine = Native.Open(false);
        try
        {
            await Task.Delay(1500);
            var before = Native.Snapshot(engine);
            log.Add($"Live MAIN callback probe: edition={before.Edition}, API={before.Version:X8}, EQ ready={before.Ready}");
            if (before.Edition == 0) throw new InvalidOperationException("VoiceMeeter is not connected.");
            for (int cycle = 0; cycle < 2; cycle++)
            {
                if (Native.SpectrumRequest(engine, 1, 1, 0, -1) == 0) throw new InvalidOperationException("Spectrum request rejected.");
                int updates = 0, frames = 0; uint sequence = uint.MaxValue; float maximum = -120;
                string message = "";
                for (int i = 0; i < 150; i++)
                {
                    await Task.Delay(20); Native.SpectrumRead(engine, out var s, Native.SpectrumBytes);
                    message = s.Message;
                    if (s.State is 3 or 4) throw new InvalidOperationException(message);
                    if (s.State == 2 && s.Sequence != sequence) { updates++; sequence = s.Sequence; maximum = Math.Max(maximum, s.Db.Max()); frames = s.AudioFrames; }
                }
                log.Add($"Cycle {cycle + 1}: {updates} spectrum updates; {frames} total callback blocks; highest bin={maximum:F2} dBFS; {message}");
                if (updates < 20) throw new InvalidOperationException("Insufficient live spectrum updates.");
                Native.SpectrumRequest(engine, 0, 0, 0, -1); await Task.Delay(250);
                Native.SpectrumRead(engine, out var released, Native.SpectrumBytes);
                log.Add($"Release: state={released.State}; {released.Message}");
                if (released.State != 0) throw new InvalidOperationException("Callback did not release.");
            }
            await Task.Delay(150); var after = Native.Snapshot(engine);
            if (!before.Bands.AsSpan().SequenceEqual(after.Bands) || before.Enabled != after.Enabled || before.Bank != after.Bank)
                throw new InvalidOperationException("EQ values changed during the probe; investigate before claiming unchanged settings.");
            log.Add("PASS: callback reacquired and released twice; selected EQ parameters unchanged. No audio was saved. Listening validation remains manual.");
            passed = true;
        }
        catch (Exception ex) { log.Add("FAIL: " + ex.Message); }
        finally { Native.SpectrumRequest(engine, 0, 0, 0, -1); await Task.Delay(250); }
        File.WriteAllLines(Path.Combine(AppContext.BaseDirectory, "spectrum-probe.txt"), log);
        return passed ? 0 : 1;
    }
}
