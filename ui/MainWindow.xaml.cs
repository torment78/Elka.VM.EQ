using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Threading;
using Elka.VM.EQ.Controls;

namespace Elka.VM.EQ;
public partial class MainWindow : Window
{
    private readonly EngineHandle _engine;
    private readonly DispatcherTimer _timer;
    private readonly bool _demo;
    private Frame _frame;
    private int _band, _channel = -1;
    private bool _refreshing, _solo, _closed;
    private float[]? _clipboard;
    private int _clipboardChannels;
    private readonly List<Button> _filterButtons = [], _channelButtons = [], _bandButtons = [];
    internal FrameworkElement DesignRoot => Root;
    internal Frame CurrentFrame => _frame;
    internal ResponseGraph Response => Graph;
    internal MainWindow(bool demo)
    {
        _demo = demo; _engine = Native.Open(demo);
        try { InitializeComponent(); }
        catch { _engine.Dispose(); throw; }
        Icon = new System.Windows.Media.Imaging.BitmapImage(new Uri("pack://application:,,,/Assets/ElkaVmEq.png"));
        _refreshing = true; KindBox.SelectedIndex = 0; RangeBox.ItemsSource = DbScale.Presets.Select(s => s.Label).ToArray(); RangeBox.SelectedIndex = 0; _refreshing = false;
        string[] names = ["Bell / Peak", "Notch", "Band pass", "Low pass", "High pass", "Low shelf", "High shelf"];
        for (int type = 0; type < 7; type++)
        {
            int selected = type;
            var b = new Button { Height = 29, Padding = new Thickness(4, 1, 4, 1), Margin = new Thickness(6, 2, 6, 2), ToolTip = names[type] };
            // A vector filter icon with a readable tooltip, not a bitmap.
            var geometry = new StreamGeometry();
            using (var p = geometry.Open()) for (int i = 0; i <= 60; i++)
            {
                double x = i / 60.0, h = Math.Exp(-Math.Pow((x - .5) * 9, 2)), s = 1 / (1 + Math.Exp(-(x - .5) * 20));
                double y = type switch { 0 => 13 - 11 * h, 1 => 2 + 11 * h, 2 => 14 - 12 * h, 3 => 2 + 12 * s, 4 => 14 - 12 * s, 5 => 4 + 9 * s, _ => 13 - 9 * s };
                if (i == 0) p.BeginFigure(new Point(i, y), false, false); else p.LineTo(new Point(i, y), true, false);
            }
            geometry.Freeze(); b.Content = new System.Windows.Shapes.Path { Data = geometry, Stroke = Skin.Ink, StrokeThickness = 1.5, Width = 64, Height = 17, Stretch = Stretch.Uniform };
            b.Click += (_, _) => Edit(_band, Native.Type, selected); FiltersPanel.Children.Add(b); _filterButtons.Add(b);
        }
        for (int band = 0; band < 6; band++)
        {
            int selected = band; var b = new Button { FontSize = 16, Padding = new Thickness(12, 4, 12, 4) };
            b.Click += (_, _) => SetBand(selected); BandsPanel.Children.Add(b); _bandButtons.Add(b);
        }
        FrequencyKnob.Changed += value => Edit(_band, Native.Frequency, value);
        GainKnob.Changed += value => Edit(_band, Native.Gain, value);
        QKnob.Changed += value => Edit(_band, Native.Q, value);
        foreach (var knob in new[] { FrequencyKnob, GainKnob, QKnob }) knob.Dragging += SetDragging;
        Graph.Dragging += SetDragging; Graph.BandSelected += SetBand; Graph.EditRequested += Edit; Graph.ResetRequested += Reset;
        _timer = new DispatcherTimer(DispatcherPriority.Background) { Interval = TimeSpan.FromMilliseconds(16) };
        _timer.Tick += (_, _) => Poll();
        StateChanged += (_, _) => { if (WindowState == WindowState.Minimized) { _timer.Stop(); EndDrags(); } else { Poll(true); _timer.Start(); } };
        Deactivated += (_, _) => EndDrags();
        SourceInitialized += (_, _) => HwndSource.FromHwnd(new WindowInteropHelper(this).Handle)?.AddHook(WindowHook);
        Closed += (_, _) => { _closed = true; _timer.Stop(); EndDrags(); Graph.DisposeGraph(); _engine.Dispose(); };
        RenderText.Text = RenderCapability.Tier >> 16 > 0 ? "GPU available · WPF" : "Software · WPF";
        Poll(true); _timer.Start();
    }
    private static void Active(Button button, bool active) => button.Tag = active ? "Active" : null;
    private void Poll(bool force = false)
    {
        if (_closed) return; var next = Native.Snapshot(_engine);
        if (force || !next.Same(_frame)) { _frame = next; Refresh(); }
        PollSpectrum();
        PollLiveLevel();
    }
    private void Refresh()
    {
        _refreshing = true;
        try
        {
            bool ready = _frame.Ready != 0;
            if (_channel >= _frame.Channels) _channel = -1;
            int c = _channel < 0 ? 0 : _channel;
            KindBox.SelectedIndex = _frame.Kind;
            var targets = Enumerable.Range(0, Native.TargetCount(_frame.Edition, _frame.Kind)).Select(i => Native.TargetName(_frame.Edition, _frame.Kind, i)).ToArray();
            if (!TargetBox.Items.Cast<string>().SequenceEqual(targets)) TargetBox.ItemsSource = targets;
            TargetBox.SelectedIndex = _frame.Index; TargetBox.IsEnabled = targets.Length > 0; KindBox.IsEnabled = _frame.Edition != 0;
            EditionText.Text = _frame.Edition switch { 3 => "Potato", 2 => "Banana", 1 => "Standard", _ => "Disconnected" };
            EqPower.Content = _frame.Enabled != 0 ? "EQ ON" : "EQ OFF"; Active(EqPower, ready && _frame.Enabled != 0); EqPower.IsEnabled = ready;
            Active(BankA, _frame.Bank == 0); Active(BankB, _frame.Bank == 1);
            if (_channelButtons.Count != _frame.Channels + 1)
            {
                ChannelsPanel.Children.Clear(); _channelButtons.Clear();
                for (int i = -1; i < _frame.Channels; i++)
                {
                    int selected = i; var b = new Button { Content = i < 0 ? "All" : (i + 1).ToString(), Width = i < 0 ? 42 : 33, Padding = new Thickness(2), Margin = new Thickness(2, 3, 2, 3) };
                    b.Click += (_, _) => { EndDrags(); _channel = selected; Refresh(); }; ChannelsPanel.Children.Add(b); _channelButtons.Add(b);
                }
            }
            for (int i = 0; i < _channelButtons.Count; i++) Active(_channelButtons[i], i - 1 == _channel);
            int type = (int)_frame.Get(c, _band, Native.Type);
            for (int i = 0; i < 7; i++) { Active(_filterButtons[i], type == i && ready); _filterButtons[i].IsEnabled = ready; }
            for (int i = 0; i < 6; i++)
            {
                Active(_bandButtons[i], _band == i);
                double hz = _frame.Get(c, i, Native.Frequency);
                _bandButtons[i].Content = $"{i + 1}   ·   {(hz >= 1000 ? $"{hz / 1000:0.##} kHz" : $"{hz:0} Hz")}";
                _bandButtons[i].ToolTip = _frame.Get(c, i, Native.On) > .5 ? "Band enabled" : "Band disabled";
            }
            FrequencyKnob.Configure("FREQUENCY", "Hz", 20, 20000, 1000, true);
            GainKnob.Configure("GAIN", "dB", Native.Limit(_frame.Edition, _frame.Kind, Native.Gain, -999), Native.Limit(_frame.Edition, _frame.Kind, Native.Gain, 999), 0, false);
            QKnob.Configure("Q", "bandwidth", Native.Limit(_frame.Edition, _frame.Kind, Native.Q, -999), 100, 3, true);
            FrequencyKnob.Live(_frame.Get(c, _band, Native.Frequency), ready, false);
            GainKnob.Live(_frame.Get(c, _band, Native.Gain), ready, type is >= 1 and <= 4);
            QKnob.Live(_frame.Get(c, _band, Native.Q), ready, false);
            Active(BandOn, ready && _frame.Get(c, _band, Native.On) > .5); Active(Solo, _solo);
            foreach (var b in new[] { BankA, BankB, FlatAll, CopyAll, CopyChannel, BandOn, FlatBand, ResetBand }) b.IsEnabled = ready;
            Paste.IsEnabled = ready && _clipboard != null;
            GraphCaption.Text = $"{(_solo ? "BAND RESPONSE" : "FREQUENCY RESPONSE")}     ·     BAND {_band + 1}     ·     {(ready ? names(type) : "OFFLINE")}";
            Graph.SetFrame(_frame, _channel, _band, _solo, DbScale.At(RangeBox.SelectedIndex));
            ConnectionLed.Fill = ready ? Skin.Cyan : Skin.Muted;
            StatusText.Text = (_demo ? "DEMO · No audio connection" : _frame.Status.Replace(" | Calculated response preview", "")) + "   ·   C engine / WPF   ·   Calculated EQ response";
        }
        finally { _refreshing = false; }
        UpdateSpectrumSources();
        PollLiveLevel();
        static string names(int type) => type switch { 0 => "BELL", 1 => "NOTCH", 2 => "BAND PASS", 3 => "LOW PASS", 4 => "HIGH PASS", 5 => "LOW SHELF", _ => "HIGH SHELF" };
    }
    private void SetDragging(bool dragging) { if (!_engine.IsClosed) Native.Dragging(_engine, dragging ? 1 : 0); }
    private void EndDrags() { FrequencyKnob.EndDrag(); GainKnob.EndDrag(); QKnob.EndDrag(); Graph.EndDrag(); SetDragging(false); }
    private void SetBand(int band) { EndDrags(); _band = band; Refresh(); }
    internal void Edit(int band, int field, double value) { if (Native.Edit(_engine, _channel, band, field, value) != 0) Poll(true); }
    internal void SelectDemoBus() { if (_demo) { Native.Select(_engine, 1, 0); Poll(true); } }
    internal Task RenderReady() => Graph.Ready;
    internal void SetDisplayScale(int index) => RangeBox.SelectedIndex = Math.Clamp(index, 0, DbScale.Presets.Length - 1);
    internal void TogglePowerForTest() { Native.Enabled(_engine, _frame.Enabled == 0 ? 1 : 0); Poll(true); }
    internal void SetBankForTest(int bank) { Native.Bank(_engine, bank); Poll(true); }
    internal void Reset(int band)
    {
        EndDrags(); Native.Defaults(out var defaults, Native.FrameBytes);
        Native.Apply(_engine, _channel, defaults.Bands[..30], 1, 30, band); Poll(true);
    }
    private void TargetChanged(object sender, SelectionChangedEventArgs e)
    {
        if (_refreshing || !IsInitialized || KindBox.SelectedIndex < 0) return;
        EndDrags(); int index = ReferenceEquals(sender, KindBox) ? 0 : TargetBox.SelectedIndex;
        if (index < 0) return; _channel = -1; Native.Select(_engine, KindBox.SelectedIndex, index); Poll(true);
    }
    private void RangeChanged(object sender, SelectionChangedEventArgs e) { if (!_refreshing && IsInitialized && _frame.Bands != null) Refresh(); }
    private void PowerClick(object sender, RoutedEventArgs e) { EndDrags(); Native.Enabled(_engine, _frame.Enabled == 0 ? 1 : 0); Poll(true); }
    private void FlatAllClick(object sender, RoutedEventArgs e) => Reset(-1);
    private void BankAClick(object sender, RoutedEventArgs e) => SwitchBank(0);
    private void BankBClick(object sender, RoutedEventArgs e) => SwitchBank(1);
    private void SwitchBank(int bank) { EndDrags(); if (_frame.Bank != bank) { Native.Bank(_engine, bank); Poll(true); } }
    private void CopyChannelClick(object sender, RoutedEventArgs e) { _clipboardChannels = 1; int start = (_channel < 0 ? 0 : _channel) * 30; _clipboard = _frame.Bands[start..(start + 30)]; Refresh(); }
    private void CopyAllClick(object sender, RoutedEventArgs e) { _clipboardChannels = _frame.Channels; _clipboard = _frame.Bands[..(_frame.Channels * 30)]; Refresh(); }
    private void PasteClick(object sender, RoutedEventArgs e)
    {
        if (_clipboard == null) return; EndDrags();
        if (_clipboardChannels != 1 && _clipboardChannels != _frame.Channels) { MessageBox.Show(this, "Copy All requires matching channel counts. Use Ch.Copy to transfer one channel between a strip and a bus.", "Different channel counts"); return; }
        Native.Apply(_engine, _channel, _clipboard, _clipboardChannels, _clipboard.Length, -1); Poll(true);
    }
    private void BandOnClick(object sender, RoutedEventArgs e) => Edit(_band, Native.On, _frame.Get(_channel < 0 ? 0 : _channel, _band, Native.On) > .5 ? 0 : 1);
    private void SoloClick(object sender, RoutedEventArgs e) { _solo = !_solo; Refresh(); }
    private void FlatBandClick(object sender, RoutedEventArgs e) { Edit(_band, Native.Type, 0); Edit(_band, Native.Gain, 0); }
    private void ResetBandClick(object sender, RoutedEventArgs e) => Reset(_band);
    private void MinimizeClick(object sender, RoutedEventArgs e) => WindowState = WindowState.Minimized;
    private void CloseClick(object sender, RoutedEventArgs e) => Close();
    private void DragTitle(object sender, MouseButtonEventArgs e)
    {
        // Only the empty/title region drags; interactive header controls handle their own input.
        if (e.GetPosition((IInputElement)sender).X < 435 && e.LeftButton == MouseButtonState.Pressed) { EndDrags(); DragMove(); e.Handled = true; }
    }
    [StructLayout(LayoutKind.Sequential)] private struct NativeRect { public int Left, Top, Right, Bottom; }
    private IntPtr WindowHook(IntPtr hwnd, int message, IntPtr wParam, IntPtr lParam, ref bool handled)
    {
        if (message == 0x0214) // WM_SIZING: one aspect-locked rectangle, no recursive layout/size writes.
        {
            var r = Marshal.PtrToStructure<NativeRect>(lParam); int edge = wParam.ToInt32();
            if (edge is 3 or 6) r.Right = r.Left + (int)Math.Round((r.Bottom - r.Top) * 1140.0 / 900);
            else { int height = (int)Math.Round((r.Right - r.Left) * 900.0 / 1140); if (edge is 4 or 5) r.Top = r.Bottom - height; else r.Bottom = r.Top + height; }
            Marshal.StructureToPtr(r, lParam, false); handled = true; return new IntPtr(1);
        }
        return IntPtr.Zero;
    }
}
