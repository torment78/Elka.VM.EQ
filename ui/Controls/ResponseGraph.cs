using System.Diagnostics;
using System.Windows;
using System.Windows.Input;
using System.Windows.Media;

namespace Elka.VM.EQ.Controls;
public sealed class ResponseGraph : FrameworkElement
{
    private readonly VisualCollection _visuals;
    private readonly DrawingVisual _background = new(), _spectrum = new(), _grid = new(), _curves = new(), _nodes = new();
    private float[]? _spectrumDb;
    internal bool HasSpectrum => _spectrumDb != null;
    private static readonly Rect Plot = new(43, 12, 1045, 307);
    private Frame _frame;
    private int _channel, _band, _dragBand = -1;
    private bool _solo, _disposed;
    private DbScale _scale = DbScale.Presets[0];
    internal DbScale ActiveScale => _scale;
    private double _dragY, _dragGain;
    private double[][]? _responses;
    private double[][] _others = [];
    private Frame? _calculated;
    private int _calculatedChannel = -2, _revision;
    private (Frame Frame, int Channel, int Revision)? _pending;
    private Task _work = Task.CompletedTask;
    internal int CurveBuilds { get; private set; }
    internal double LastCurveMilliseconds { get; private set; }
    internal event Action<int>? BandSelected;
    internal event Action<int, int, double>? EditRequested;
    internal event Action<int>? ResetRequested;
    internal event Action<bool>? Dragging;
    internal Task Ready => _work;
    public ResponseGraph()
    {
        Focusable = true; Cursor = Cursors.Cross;
        _visuals = new VisualCollection(this) { _background, _spectrum, _grid, _curves, _nodes }; DrawGrid();
        ToolTip = "Drag a numbered point: frequency / gain. Scroll: Q. Double-click: reset band. Shift: fine gain adjustment.";
    }
    protected override int VisualChildrenCount => _visuals.Count;
    protected override Visual GetVisualChild(int index) => _visuals[index];
    private static double X(double frequency) => Plot.Left + Math.Log10(Math.Clamp(frequency, 20, 20000) / 20) / 3 * Plot.Width;
    private double Y(double db) => Plot.Top + _scale.Normalize(db) * Plot.Height;
    internal void SetFrame(Frame frame, int channel, int band, bool solo, DbScale scale)
    {
        if (_disposed) return;
        if (frame.Ready == 0) EndDrag();
        bool gridChanged = _scale != scale;
        if (gridChanged) EndDrag();
        _frame = frame; _channel = channel; _band = band; _solo = solo; _scale = scale;
        if (gridChanged) { DrawGrid(); DrawSpectrum(); }
        if (frame.Ready == 0) { _pending = null; _revision++; _calculated = null; _responses = null; _others = []; DrawCurves(); DrawNodes(); return; }
        if (_calculated is not { } previous || _calculatedChannel != channel || previous.SampleRate != frame.SampleRate || previous.Channels != frame.Channels || !previous.Bands.AsSpan().SequenceEqual(frame.Bands))
        {
            _calculated = frame; _calculatedChannel = channel;
            _pending = (frame, channel, ++_revision);
            if (_work.IsCompleted) _work = CalculateLoop();
        }
        DrawCurves(); DrawNodes();
    }
    private async Task CalculateLoop()
    {
        // At most one calculation in flight and one newest request pending.
        // Window resizing never enters this path.
        while (_pending is { } job && !_disposed)
        {
            _pending = null;
            var computed = await Task.Run(() =>
            {
                var watch = Stopwatch.StartNew(); int c = job.Channel < 0 ? 0 : job.Channel;
                var bands = Enumerable.Range(0, 6).Select(b => Native.Response(job.Frame, c, b)).ToArray();
                var others = new List<double[]>();
                if (job.Channel < 0) for (int n = 1; n < job.Frame.Channels; n++)
                {
                    if (!job.Frame.Bands.AsSpan(n * 30, 30).SequenceEqual(job.Frame.Bands.AsSpan(0, 30))) others.Add(Native.Response(job.Frame, n));
                }
                return (Bands: bands, Others: others.ToArray(), Milliseconds: watch.Elapsed.TotalMilliseconds);
            });
            if (!_disposed && job.Revision == _revision)
            {
                _responses = computed.Bands; _others = computed.Others; CurveBuilds++;
                LastCurveMilliseconds = computed.Milliseconds; DrawCurves(); DrawNodes();
            }
        }
    }
    internal void DisposeGraph() { _disposed = true; _pending = null; EndDrag(); }
    internal void SetSpectrum(float[]? bins) { _spectrumDb = bins; DrawSpectrum(); }
    internal double SpectrumY(double db) => Y(db);
    private void DrawSpectrum()
    {
        using var dc = _spectrum.RenderOpen();
        if (_spectrumDb is not { Length: 384 } bins) return;
        var geometry = new StreamGeometry();
        using (var p = geometry.Open())
        {
            p.BeginFigure(new Point(Plot.Left, Plot.Bottom), true, true);
            for (int i = 0; i < bins.Length; i++)
                p.LineTo(new Point(Plot.Left + (i + .5) / bins.Length * Plot.Width,
                    Y(Math.Clamp(float.IsFinite(bins[i]) ? bins[i] : -120, _scale.Bottom, _scale.Top))), true, false);
            p.LineTo(new Point(Plot.Right, Plot.Bottom), true, false);
        }
        geometry.Freeze();
        dc.PushClip(new RectangleGeometry(Plot));
        dc.DrawGeometry(Skin.Gradient("#80427791", "#18283C53"), Skin.Pen(Skin.Brush("#AA6291AB"), .8), geometry);
        dc.Pop();
    }
    private void DrawGrid()
    {
        using (var background = _background.RenderOpen())
            background.DrawRoundedRectangle(Skin.Gradient("#07131C", "#0C202A"), Skin.Pen(Skin.Brush("#375462"), 1), Plot, 5, 5);
        using var dc = _grid.RenderOpen();
        dc.DrawRectangle(Brushes.Transparent, null, new Rect(0, 0, 1110, 350));
        double tickSize = Math.Min(10, 3 * Plot.Height / _scale.Span * .95);
        foreach (int tick in _scale.Ticks)
        {
            double y = Y(tick);
            dc.DrawLine(Skin.Pen(tick == 0 ? Skin.Brush("#64818E") : Skin.Grid, tick == 0 ? 1 : .6), new Point(Plot.Left, y), new Point(Plot.Right, y));
            Skin.Text(dc, tick > 0 ? $"+{tick}" : tick.ToString(), tickSize, tick == 0 ? Skin.Ink : Skin.Muted, 22, y - tickSize * .7);
        }
        for (int decade = 10; decade <= 10000; decade *= 10) for (int step = 1; step <= 9; step++)
        {
            double f = decade * step; if (f < 20 || f > 20000) continue;
            dc.DrawLine(Skin.Pen(Skin.Grid, step == 1 ? 1 : .45), new Point(X(f), Plot.Top), new Point(X(f), Plot.Bottom));
        }
        int[] labels = [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000];
        foreach (int f in labels) Skin.Text(dc, f >= 1000 ? $"{f / 1000}k" : f.ToString(), 10, Skin.Muted, X(f), Plot.Bottom + 10);
    }
    private StreamGeometry MakeCurve(double[] values, bool fill)
    {
        var geometry = new StreamGeometry();
        using (var p = geometry.Open())
        {
            p.BeginFigure(new Point(Plot.Left, fill ? Y(0) : Y(values[0])), fill, fill);
            for (int i = 0; i < values.Length; i++) p.LineTo(new Point(Plot.Left + i * Plot.Width / (values.Length - 1), Y(values[i])), true, false);
            if (fill) p.LineTo(new Point(Plot.Right, Y(0)), true, false);
        }
        geometry.Freeze(); return geometry;
    }
    private void DrawCurves()
    {
        using var dc = _curves.RenderOpen();
        if (_responses == null || _frame.Ready == 0) return;
        dc.PushClip(new RectangleGeometry(Plot));
        if (!_solo) foreach (var other in _others) dc.DrawGeometry(null, Skin.Pen(Skin.Brush("#507F96AA"), 1), MakeCurve(other, false));
        var sum = new double[_responses[0].Length];
        for (int b = 0; b < 6; b++)
        {
            if (_solo && b != _band) continue;
            for (int i = 0; i < sum.Length; i++) sum[i] += _responses[b][i];
            if (b == _band) dc.DrawGeometry(Skin.Brush("#1538D4EA"), null, MakeCurve(_responses[b], true));
            if (!_solo) dc.DrawGeometry(null, Skin.Pen(Skin.Brush(b == _band ? "#6573B3D5" : "#264C819B"), b == _band ? 1.1 : .8), MakeCurve(_responses[b], false));
        }
        var main = MakeCurve(sum, false);
        dc.DrawGeometry(null, Skin.Pen(Skin.Brush(_frame.Enabled != 0 ? "#1839E1F2" : "#18779BAB"), 6), main);
        dc.DrawGeometry(null, Skin.Pen(_frame.Enabled != 0 ? Skin.Cyan : Skin.Muted, 1.8), main);
        dc.Pop();
    }
    private Point Handle(int b)
    {
        int c = _channel < 0 ? 0 : _channel; int type = (int)_frame.Get(c, b, Native.Type);
        return new Point(X(_frame.Get(c, b, Native.Frequency)), Y(type is 0 or 5 or 6 ? _frame.Get(c, b, Native.Gain) : 0));
    }
    private void DrawNodes()
    {
        using var dc = _nodes.RenderOpen();
        if (_frame.Ready == 0) { Skin.Text(dc, "Select an available EQ to begin", 17, Skin.Muted, Plot.Left + 23, Plot.Top + 25, false); return; }
        int c = _channel < 0 ? 0 : _channel;
        for (int b = 0; b < 6; b++)
        {
            var pt = Handle(b); bool selected = b == _band, on = _frame.Get(c, b, Native.On) > .5;
            if (selected) dc.DrawEllipse(Skin.Brush("#2536DCF3"), null, pt, 15, 15);
            dc.DrawEllipse(selected ? Skin.Cyan : Skin.Brush("#163A49"), Skin.Pen(on ? Skin.Cyan : Skin.Muted, selected ? 1.8 : 1), pt, 9, 9);
            Skin.Text(dc, (b + 1).ToString(), 10, selected ? Skin.Well : Skin.Ink, pt.X, pt.Y - 7);
        }
    }
    internal void EndDrag()
    { if (_dragBand < 0) return; _dragBand = -1; if (IsMouseCaptured) ReleaseMouseCapture(); Dragging?.Invoke(false); }
    protected override void OnMouseLeftButtonDown(MouseButtonEventArgs e)
    {
        e.Handled = true; Focus(); if (_frame.Ready == 0) return;
        var p = e.GetPosition(this); int found = -1; double closest = 22;
        for (int b = 0; b < 6; b++) { double d = (Handle(b) - p).Length; if (d < closest) { closest = d; found = b; } }
        if (found < 0) return; BandSelected?.Invoke(found);
        if (e.ClickCount == 2) { ResetRequested?.Invoke(found); return; }
        _dragY = p.Y; _dragGain = _frame.Get(_channel < 0 ? 0 : _channel, found, Native.Gain);
        if (CaptureMouse()) { _dragBand = found; Dragging?.Invoke(true); }
    }
    protected override void OnMouseMove(MouseEventArgs e)
    {
        if (_dragBand < 0) return; var p = e.GetPosition(this);
        EditRequested?.Invoke(_dragBand, Native.Frequency, 20 * Math.Pow(1000, Math.Clamp((p.X - Plot.Left) / Plot.Width, 0, 1)));
        int type = (int)_frame.Get(_channel < 0 ? 0 : _channel, _dragBand, Native.Type);
        if (type is 0 or 5 or 6) EditRequested?.Invoke(_dragBand, Native.Gain, _dragGain + _scale.DragDelta(_dragY - p.Y, Plot.Height) * (Keyboard.Modifiers.HasFlag(ModifierKeys.Shift) ? .1 : 1));
        e.Handled = true;
    }
    protected override void OnMouseLeftButtonUp(MouseButtonEventArgs e) { EndDrag(); e.Handled = true; }
    protected override void OnLostMouseCapture(MouseEventArgs e) { EndDrag(); base.OnLostMouseCapture(e); }
    protected override void OnKeyDown(KeyEventArgs e) { if (e.Key == Key.Escape) { EndDrag(); e.Handled = true; } }
    protected override void OnMouseWheel(MouseWheelEventArgs e)
    {
        if (_frame.Ready != 0) EditRequested?.Invoke(_band, Native.Q, _frame.Get(_channel < 0 ? 0 : _channel, _band, Native.Q) * Math.Pow(1.08, Math.Sign(e.Delta) * (Keyboard.Modifiers.HasFlag(ModifierKeys.Shift) ? .1 : 1)));
        e.Handled = true;
    }
}
