using System.IO;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;

namespace Elka.VM.EQ;
public partial class App : Application
{
    protected override async void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);
        try
        {
            if (e.Args.Contains("--self-test") || e.Args.Contains("--render") || e.Args.Contains("--render-donate") || e.Args.Contains("--probe") || e.Args.Contains("--spectrum-probe") || e.Args.Contains("--level-probe")) ShutdownMode = ShutdownMode.OnExplicitShutdown;
            if (e.Args.Contains("--self-test")) { Shutdown(await SelfTest.Run()); return; }
            if (e.Args.Contains("--level-probe")) { Shutdown(await LevelProbe.Run()); return; }
            if (e.Args.Contains("--spectrum-probe")) { Shutdown(await SpectrumProbe.Run()); return; }
            if (e.Args.Contains("--probe"))
            {
                using var engine = Native.Open(false);
                await Task.Delay(1200); var first = Native.Snapshot(engine);
                var lines = new List<string> { $"Hybrid C ABI probe: edition={first.Edition}, version={first.Version:X8}, ready={first.Ready}", first.Status };
                for (int kind = 0; kind < 2; kind++) for (int index = 0; index < Native.TargetCount(first.Edition, kind); index++)
                {
                    Native.Select(engine, kind, index); await Task.Delay(350); var f = Native.Snapshot(engine);
                    lines.Add($"{(kind == 0 ? "Strip" : "Bus")} {Native.TargetName(f.Edition, kind, index)}: ready={f.Ready}, channels={f.Channels}, bank={f.Bank}, {f.Status}");
                }
                File.WriteAllLines(Path.Combine(AppContext.BaseDirectory, "hybrid-probe.txt"), lines); Shutdown(); return;
            }
            var render = Array.IndexOf(e.Args, "--render");
            var donateRender = Array.IndexOf(e.Args, "--render-donate");
            bool demo = render >= 0 || donateRender >= 0 || e.Args.Contains("--demo");
            var window = new MainWindow(demo);
            MainWindow = window;
            int scaleArg = Array.IndexOf(e.Args, "--scale");
            if (scaleArg >= 0 && scaleArg + 1 < e.Args.Length && int.TryParse(e.Args[scaleArg + 1], out int scale)) window.SetDisplayScale(scale);
            if (e.Args.Contains("--demo-bus")) window.SelectDemoBus();
            if (e.Args.Contains("--demo-spectrum")) window.ShowDemoSpectrum();
            if (donateRender >= 0)
            {
                if (donateRender + 1 >= e.Args.Length) throw new ArgumentException("--render-donate requires a PNG path.");
                await window.RenderReady();
                window.DesignRoot.Measure(new Size(1140, 900)); window.DesignRoot.Arrange(new Rect(0, 0, 1140, 900)); window.DesignRoot.UpdateLayout();
                // Render the actual code-native button at 4x for the installer.
                var control = window.DonationVisual;
                var bitmap = RenderDonationButton(control);
                var png = new PngBitmapEncoder(); png.Frames.Add(BitmapFrame.Create(bitmap));
                using (var file = File.Create(Path.GetFullPath(e.Args[donateRender + 1]))) png.Save(file);
                window.Close(); Shutdown(); return;
            }
            if (render >= 0)
            {
                if (render + 1 >= e.Args.Length) throw new ArgumentException("--render requires a PNG path.");
                // Offline retained-visual render: no live API and no visible window.
                await window.RenderReady();
                var root = window.DesignRoot;
                root.Measure(new Size(1140, 900)); root.Arrange(new Rect(0, 0, 1140, 900)); root.UpdateLayout();
                await Dispatcher.InvokeAsync(() => { }, DispatcherPriority.Render);
                var bitmap = new RenderTargetBitmap(1140, 900, 96, 96, PixelFormats.Pbgra32); bitmap.Render(root);
                var png = new PngBitmapEncoder(); png.Frames.Add(BitmapFrame.Create(bitmap));
                using (var file = File.Create(Path.GetFullPath(e.Args[render + 1]))) png.Save(file);
                window.Close(); Shutdown(); return;
            }
            window.Show();
        }
        catch (Exception ex)
        {
            if (e.Args.Length > 0) File.WriteAllText(Path.Combine(AppContext.BaseDirectory,"startup-error.txt"),ex.ToString());
            else MessageBox.Show(ex.Message, "Elka VM EQ", MessageBoxButton.OK, MessageBoxImage.Error);
            Shutdown(1);
        }
    }

    internal static RenderTargetBitmap RenderDonationButton(FrameworkElement control)
    {
        var bitmap = new RenderTargetBitmap((int)control.ActualWidth * 4, (int)control.ActualHeight * 4, 384, 384, PixelFormats.Pbgra32);
        var visual = new DrawingVisual();
        using (var drawing = visual.RenderOpen())
            drawing.DrawRectangle(new VisualBrush(control) { Stretch = Stretch.Fill }, null, new Rect(0, 0, control.ActualWidth, control.ActualHeight));
        bitmap.Render(visual);
        return bitmap;
    }
}
