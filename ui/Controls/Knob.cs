using System.Globalization;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;

namespace Elka.VM.EQ.Controls;
public sealed class Knob : FrameworkElement
{
    private DrawingGroup? _face;
    private string _title = "", _unit = "";
    private double _min = 20, _max = 20000, _reset = 1000, _value = 1000;
    private bool _log = true, _ready, _readOnly, _dragging;
    private double _startY, _start;
    internal event Action<double>? Changed;
    internal event Action<bool>? Dragging;
    internal double Value => _value;
    public Knob()
    {
        Focusable = true; Cursor = Cursors.SizeNS;
        ToolTip = "Drag or scroll · Shift for fine control · Double-click to reset · Right-click for numeric entry";
        SizeChanged += (_, _) => { _face = null; InvalidateVisual(); };
        IsEnabledChanged += (_, _) => { if (!IsEnabled) EndDrag(); InvalidateVisual(); };
    }
    internal void Configure(string title, string unit, double min, double max, double reset, bool logarithmic)
    {
        if (_title == title && _min == min && _max == max) return;
        _title = title; _unit = unit; _min = min; _max = max; _reset = reset; _log = logarithmic; _face = null; InvalidateVisual();
    }
    internal void Live(double value, bool ready, bool readOnly)
    {
        if (!ready) EndDrag();
        if (_value == value && _ready == ready && _readOnly == readOnly) return;
        if (!_dragging) _value = value;
        _ready = ready; _readOnly = readOnly; Cursor = !ready || readOnly ? Cursors.Arrow : Cursors.SizeNS; InvalidateVisual();
    }
    internal double Normalize(double v) => Math.Clamp(_log ? Math.Log(Math.Max(_min, v) / _min) / Math.Log(_max / _min) : (v - _min) / (_max - _min), 0, 1);
    internal double Denormalize(double v) { v = Math.Clamp(v, 0, 1); return _log ? _min * Math.Pow(_max / _min, v) : _min + (_max - _min) * v; }
    private void Request(double value)
    {
        if (!_ready || _readOnly || !double.IsFinite(value)) return;
        value = Math.Clamp(value, _min, _max); _value = value; Changed?.Invoke(value); InvalidateVisual();
    }
    internal void EndDrag()
    { if (!_dragging) return; _dragging = false; if (IsMouseCaptured) ReleaseMouseCapture(); Dragging?.Invoke(false); }
    protected override void OnMouseLeftButtonDown(MouseButtonEventArgs e)
    {
        e.Handled = true; Focus(); if (!_ready || _readOnly) return;
        if (e.ClickCount == 2) { EndDrag(); Request(_reset); return; }
        _startY = e.GetPosition(this).Y; _start = Normalize(_value); _dragging = CaptureMouse(); if (_dragging) Dragging?.Invoke(true);
    }
    protected override void OnMouseMove(MouseEventArgs e)
    { if (_dragging) { Request(Denormalize(_start + (_startY - e.GetPosition(this).Y) / 220 * (Keyboard.Modifiers.HasFlag(ModifierKeys.Shift) ? .1 : 1))); e.Handled = true; } }
    protected override void OnMouseLeftButtonUp(MouseButtonEventArgs e) { EndDrag(); e.Handled = true; }
    protected override void OnLostMouseCapture(MouseEventArgs e) { EndDrag(); base.OnLostMouseCapture(e); }
    protected override void OnMouseWheel(MouseWheelEventArgs e)
    {
        var fine = Keyboard.Modifiers.HasFlag(ModifierKeys.Shift);
        Request(_log ? Denormalize(Normalize(_value) + Math.Sign(e.Delta) * (fine ? .001 : .01)) : _value + Math.Sign(e.Delta) * (fine ? .1 : .5)); e.Handled = true;
    }
    protected override void OnKeyDown(KeyEventArgs e)
    {
        if (e.Key == Key.Home) { Request(_reset); e.Handled = true; }
        else if (e.Key is Key.Up or Key.Right or Key.Down or Key.Left) { Request(Denormalize(Normalize(_value) + (e.Key is Key.Up or Key.Right ? .005 : -.005))); e.Handled = true; }
        else if (e.Key == Key.Enter) { NumericEntry(); e.Handled = true; }
        else if (e.Key == Key.Escape) { EndDrag(); e.Handled = true; }
    }
    protected override void OnMouseRightButtonDown(MouseButtonEventArgs e) { e.Handled = true; NumericEntry(); }
    private void NumericEntry()
    {
        EndDrag(); if (!_ready || _readOnly) return;
        var input = new TextBox { Text = _value.ToString("0.###", CultureInfo.InvariantCulture), FontSize = 20, Margin = new Thickness(16, 8, 16, 8) };
        var panel = new StackPanel();
        panel.Children.Add(new TextBlock { Text = $"{_title}  ·  {_min} to {_max} {_unit}", Margin = new Thickness(16, 15, 16, 0), Foreground = Skin.Muted });
        panel.Children.Add(input);
        var apply = new Button { Content = "Apply", Height = 36, Margin = new Thickness(16, 0, 16, 16) }; panel.Children.Add(apply);
        var dialog = new Window { Title = _title, Width = 340, SizeToContent = SizeToContent.Height, ResizeMode = ResizeMode.NoResize, Owner = Window.GetWindow(this), WindowStartupLocation = WindowStartupLocation.CenterOwner, Content = panel, Background = Skin.Well };
        void Save() { if (double.TryParse(input.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out var v) && double.IsFinite(v) && v >= _min && v <= _max) { Request(v); dialog.Close(); } else input.BorderBrush = Brushes.OrangeRed; }
        apply.Click += (_, _) => Save(); input.KeyDown += (_, e) => { if (e.Key == Key.Enter) Save(); if (e.Key == Key.Escape) dialog.Close(); };
        dialog.Loaded += (_, _) => { input.Focus(); input.SelectAll(); }; dialog.ShowDialog();
    }
    private void BuildFace()
    {
        var group = new DrawingGroup(); double cx = ActualWidth / 2; var c = new Point(cx, 178); double r = _title == "GAIN" ? 66 : 60;
        using (var dc = group.Open())
        {
            Skin.Text(dc, _title, 13, Skin.Muted, cx, 6);
            dc.DrawRoundedRectangle(Skin.Brush("#060F16"), null, new Rect(cx - 66, 35, 132, 36), 6, 6);
            dc.DrawRoundedRectangle(Skin.Gradient("#06121B", "#0F2734"), Skin.Pen(Skin.Brush("#30566A"), 1), new Rect(cx - 65, 34, 130, 35), 5, 5);
            for (int i = 0; i <= 30; i++) dc.DrawLine(Skin.Pen(i % 5 == 0 ? Skin.Muted : Skin.Brush("#45616F"), i % 5 == 0 ? 1.5 : 1), Skin.Polar(c, r + 18, 135 + i * 9), Skin.Polar(c, r + (i % 5 == 0 ? 25 : 22), 135 + i * 9));
            dc.DrawGeometry(null, Skin.Pen(Skin.Brush("#041018"), 8), Skin.Arc(c, r + 10, 270));
            dc.DrawGeometry(null, Skin.Pen(Skin.Brush("#294B60"), 3), Skin.Arc(c, r + 10, 270));
            dc.DrawEllipse(Skin.Brush("#55000000"), null, new Point(cx, c.Y + 5), r + 4, r + 4);
            dc.DrawEllipse(Skin.Gradient("#A0B8C2", "#172E3C"), Skin.Pen(Skin.Brush("#020C12"), 2), c, r, r);
            dc.DrawEllipse(Skin.Gradient("#142B39", "#65828F"), null, c, r - 3, r - 3);
            dc.DrawEllipse(Skin.Brush("#081620"), null, c, r - 7, r - 7);
            var face = new RadialGradientBrush((Color)ColorConverter.ConvertFromString("#38505B"), (Color)ColorConverter.ConvertFromString("#101E28")) { GradientOrigin = new Point(.32, .17), Center = new Point(.42, .35), RadiusX = .8, RadiusY = .8 }; face.Freeze();
            dc.DrawEllipse(face, Skin.Pen(Skin.Brush("#527181"), .7), c, r - 9, r - 9);
            Skin.Text(dc, _min.ToString("0.###", CultureInfo.InvariantCulture), 11, Skin.Muted, cx - 76, 250);
            Skin.Text(dc, _max >= 1000 ? $"{_max / 1000:0.#}k" : _max.ToString("0.###", CultureInfo.InvariantCulture), 11, Skin.Muted, cx + 76, 250);
        }
        group.Freeze(); _face = group;
    }
    protected override void OnRender(DrawingContext dc)
    {
        dc.DrawRectangle(Brushes.Transparent, null, new Rect(RenderSize));
        if (_face == null) BuildFace(); dc.DrawDrawing(_face);
        double cx = ActualWidth / 2, r = _title == "GAIN" ? 66 : 60; var c = new Point(cx, 178);
        var display = !_ready ? "—" : (_readOnly ? 0 : _value).ToString(_title == "Q" ? "0.000" : _title == "FREQUENCY" ? "0.0" : "+0.0;-0.0;0.0", CultureInfo.InvariantCulture);
        Skin.Text(dc, display, 21, _readOnly ? Skin.Muted : Skin.Ink, cx, 36);
        Skin.Text(dc, _readOnly ? "not used by this filter" : _unit, 10, Skin.Muted, cx, 72);
        if (_ready)
        {
            var sweep = 270 * Normalize(_readOnly ? 0 : _value); var arc = Skin.Arc(c, r + 10, sweep);
            dc.DrawGeometry(null, Skin.Pen(Skin.Brush("#2430DFFF"), 9), arc);
            dc.DrawGeometry(null, Skin.Pen(_readOnly ? Skin.Muted : Skin.Cyan, 3.5), arc);
            double angle = 135 + sweep;
            dc.DrawLine(Skin.Pen(Skin.Brush("#050F16"), 5), Skin.Polar(c, 8, angle), Skin.Polar(c, r - 14, angle));
            dc.DrawLine(Skin.Pen(_readOnly ? Skin.Muted : Skin.Ink, 2.5), Skin.Polar(c, 8, angle), Skin.Polar(c, r - 14, angle));
            dc.DrawEllipse(Skin.Gradient("#506777", "#152735"), null, c, 5, 5);
        }
        if (IsKeyboardFocused) dc.DrawLine(Skin.Pen(Skin.Cyan, 1), new Point(cx - 48, 72), new Point(cx + 48, 72));
    }
}
