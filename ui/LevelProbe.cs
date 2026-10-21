using System.IO;
namespace Elka.VM.EQ;
internal static class LevelProbe
{
    // Read-only live API check. Never registers an audio callback or writes EQ.
    internal static async Task<int> Run()
    {
        var lines = new List<string>(); int failures = 0;
        using var engine = Native.Open(false);
        await Task.Delay(1500); var initial = Native.Snapshot(engine);
        lines.Add($"GetLevel live probe: edition={initial.Edition}, API={initial.Version:X8}. No audio callbacks or parameter writes.");
        if (initial.Edition == 0) failures++;
        for (int kind = 0; kind < 2; kind++) for (int index = 0; index < Native.TargetCount(initial.Edition, kind); index++)
        {
            Native.Select(engine, kind, index); await Task.Delay(150);
            var selected = Native.Snapshot(engine); int valid = 0; float peak = 0; uint sequence = uint.MaxValue; int updates = 0;
            for (int i = 0; i < 20; i++)
            {
                await Task.Delay(20); Native.ReadLevels(engine, out var meter, Native.LevelsBytes);
                if (LevelDisplay.Format(meter, selected, -1) != "—") { valid++; peak = Math.Max(peak, meter.Values.Max()); }
                if (meter.Sequence != sequence) { updates++; sequence = meter.Sequence; }
            }
            Native.SpectrumRead(engine, out var spectrum, Native.SpectrumBytes);
            if (valid == 0 || updates < 2 || spectrum.State != 0 || spectrum.Requested != 0) failures++;
            lines.Add($"{Native.TargetName(initial.Edition, kind, index)}: valid={valid}/20; updates={updates}; highest level={(peak > 0 ? (20 * Math.Log10(peak)).ToString("F1") : "-inf")} dBFS; RTA state={spectrum.State}.");
        }
        lines.Add(failures == 0 ? "PASS: live level samples received with RTA off." : $"FAIL: {failures} target/connection checks failed.");
        File.WriteAllLines(Path.Combine(AppContext.BaseDirectory, "level-probe.txt"), lines);
        return failures == 0 ? 0 : 1;
    }
}
