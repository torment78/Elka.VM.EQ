using System.Globalization;
using System.Windows;
using System.Windows.Media;

namespace Elka.VM.EQ.Controls;
internal static class Skin
{
    internal static readonly Brush Cyan = Brush("#29DDED"), Ink = Brush("#DAEAF1"), Muted = Brush("#8CAEBE"), Well = Brush("#091923"), Grid = Brush("#243E4D");
    internal static SolidColorBrush Brush(string hex) { var b = new SolidColorBrush((Color)ColorConverter.ConvertFromString(hex)); b.Freeze(); return b; }
    internal static Pen Pen(Brush brush, double width) { var p = new Pen(brush, width) { StartLineCap = PenLineCap.Round, EndLineCap = PenLineCap.Round, LineJoin = PenLineJoin.Round }; p.Freeze(); return p; }
    internal static void Text(DrawingContext dc, string value, double size, Brush brush, double x, double y, bool center = true)
    {
        var t = new FormattedText(value, CultureInfo.InvariantCulture, FlowDirection.LeftToRight, new Typeface("Segoe UI"), size, brush, 1);
        dc.DrawText(t, new Point(x - (center ? t.Width / 2 : 0), y));
    }
    internal static Point Polar(Point center, double radius, double degrees) => center + new Vector(radius * Math.Cos(degrees * Math.PI / 180), radius * Math.Sin(degrees * Math.PI / 180));
    internal static StreamGeometry Arc(Point center, double radius, double sweep)
    {
        var g = new StreamGeometry();
        if (sweep > .001) using (var p = g.Open()) { p.BeginFigure(Polar(center, radius, 135), false, false); p.ArcTo(Polar(center, radius, 135 + sweep), new Size(radius, radius), 0, sweep > 180, SweepDirection.Clockwise, true, false); }
        g.Freeze(); return g;
    }
    internal static LinearGradientBrush Gradient(string top, string bottom)
    { var b = new LinearGradientBrush((Color)ColorConverter.ConvertFromString(top), (Color)ColorConverter.ConvertFromString(bottom), 90); b.Freeze(); return b; }
}
