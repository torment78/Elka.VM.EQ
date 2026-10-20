using System.Diagnostics;
using System.IO;
using System.Windows;
using Elka.VM.EQ.Controls;

namespace Elka.VM.EQ;
internal static class SelfTest
{
    internal static async Task<int> Run()
    {
        var log = new List<string>(); int checks = 0;
        void Check(bool value, string name) { checks++; if (!value) throw new InvalidOperationException(name); log.Add("PASS " + name); }
        try
        {
            int[][] ticks = [
                [24,18,12,6,3,0,-3,-6,-12,-18,-24,-36,-48,-54],
                [36,24,18,12,6,3,0,-3,-6,-12,-18,-24,-36,-48,-60,-72],
                [18,12,6,3,0,-3,-6,-12,-18],
                [18,12,6,3,0,-3,-6,-12,-18,-24,-36]
            ];
            for (int i = 0; i < DbScale.Presets.Length; i++)
            {
                var scale = DbScale.Presets[i];
                Check(scale.Ticks.SequenceEqual(ticks[i]), $"{scale.Label}: exact grid ticks");
                Check(scale.Normalize(scale.Top) == 0 && scale.Normalize(scale.Bottom) == 1, $"{scale.Label}: display endpoints");
                Check(Math.Abs(scale.Normalize(0) - scale.Top / scale.Span) < 1e-12, $"{scale.Label}: correct zero position");
                Check(scale.Ticks.All(db => Math.Abs(scale.FromNormalized(scale.Normalize(db)) - db) < 1e-10), $"{scale.Label}: grid/curve mapping roundtrip");
                Check(Math.Abs(scale.DragDelta(307 * 6 / scale.Span, 307) - 6) < 1e-10, $"{scale.Label}: six-dB drag alignment");
            }
            using (var engine = Native.Open(true))
            {
                var frame = Native.Snapshot(engine); Check(frame.Ready == 1 && frame.Channels == 2, "C ABI / managed frame layout");
                Check(Native.SpectrumRead(engine, out var spectrum, Native.SpectrumBytes) == 1 && spectrum.State == 0, "spectrum ABI and default off");
                Check(Native.SpectrumRequest(engine, 1, 1, 0, -1) == 0, "demo cannot register a live callback");
                Check(Native.Edit(engine, -1, 1, Native.Gain, 8) == 1, "managed-to-native edit");
                frame = Native.Snapshot(engine); Check(frame.Get(0, 1, Native.Gain) == 8 && frame.Get(1, 1, Native.Gain) == 8, "linked channel readback");
                Native.Bank(engine, 1); Native.Bank(engine, 0); Check(Native.Snapshot(engine).Get(0, 1, Native.Gain) == 8, "A/B persistence");
                Native.Enabled(engine, 0); Native.Enabled(engine, 1); Check(Native.Snapshot(engine).Get(0, 1, Native.Gain) == 8, "bypass preserves parameters");
                Check(Native.Edit(engine, 0, 0, Native.Gain, double.NaN) == 0, "reject non-finite edit");
                var watch = Stopwatch.StartNew(); for (int i = 0; i < 40; i++) Native.Response(frame, 0); watch.Stop();
                log.Add($"Native 1024-point curve average: {watch.Elapsed.TotalMilliseconds / 40:0.000} ms (offline)");
            }
            var window = new MainWindow(true);
            try
            {
                window.ShowActivated = false; window.ShowInTaskbar = false; window.WindowStartupLocation = WindowStartupLocation.Manual; window.Left = -20000; window.Top = -20000; window.Show();
                await window.RenderReady(); Check(window.Response.CurveBuilds > 0, "WPF graph generated");
                int before = window.Response.CurveBuilds;
                Check(window.Response.ActiveScale == DbScale.Presets[0], "default scale is +24 / -54 dB");
                var unchanged = window.CurrentFrame;
                for (int i = 0; i < DbScale.Presets.Length; i++)
                {
                    window.SetDisplayScale(i); await window.RenderReady();
                    Check(window.Response.ActiveScale == DbScale.Presets[i], $"range dropdown selects preset {i + 1}");
                    Check(window.CurrentFrame.Same(unchanged), $"preset {i + 1} leaves EQ parameters untouched");
                }
                window.SetDisplayScale(0);
                Check(window.Response.CurveBuilds == before, "range changes reuse native curve calculations");
                window.ShowDemoSpectrum(); Check(window.Response.HasSpectrum, "background spectrum layer visible");
                for (int i = 0; i < DbScale.Presets.Length; i++)
                {
                    window.SetDisplayScale(i);
                    Check(Math.Abs(window.Response.SpectrumY(-6) - (12 + DbScale.Presets[i].Normalize(-6) * 307)) < .0001, $"spectrum shares EQ dB scale {i + 1}");
                }
                window.SetDisplayScale(0); window.Response.SetSpectrum(null);
                Check(!window.Response.HasSpectrum && window.Response.CurveBuilds == before, "spectrum clear does not recalculate EQ");
                for (int i = 0; i < 50; i++) { window.Width = 700 + i * 8; window.Height = window.Width * 900 / 1140; window.DesignRoot.Measure(new Size(1140, 900)); window.DesignRoot.Arrange(new Rect(0, 0, 1140, 900)); window.DesignRoot.UpdateLayout(); }
                await window.RenderReady(); Check(window.Response.CurveBuilds == before, "resize does not recalculate native curves");
                window.WindowState = WindowState.Minimized; await Task.Delay(30); window.WindowState = WindowState.Normal; await Task.Delay(30);
                Check(window.Response.CurveBuilds == before, "minimize / restore retains cached curve");
                for (int i = 0; i < 60; i++) window.Edit(0, Native.Gain, -6 + i * .2);
                await window.RenderReady(); Check(Math.Abs(window.CurrentFrame.Get(0, 0, Native.Gain) - 5.8) < .001, "rapid edits retain latest value");
                Check(window.Response.CurveBuilds <= before + 2, "curve work is coalesced");
                window.SelectDemoBus(); await window.RenderReady(); Check(window.CurrentFrame.Channels == 8, "eight-channel bus UI");
                window.Reset(-1); await window.RenderReady(); Check(window.CurrentFrame.Get(7, 0, Native.Gain) == 0, "reset linked eight-channel bus");
                Check(Native.Response(window.CurrentFrame, 7).All(v => Math.Abs(v) < .0001), "flat curve");
            }
            finally { window.Close(); }
            log.Add($"{checks} integration checks passed. No live API connection was opened.");
            File.WriteAllLines(Path.Combine(AppContext.BaseDirectory, "self-test.txt"), log); return 0;
        }
        catch (Exception ex) { log.Add(ex.ToString()); File.WriteAllLines(Path.Combine(AppContext.BaseDirectory, "self-test.txt"), log); return 1; }
    }
}
