namespace Elka.VM.EQ.Controls;

// Display-only scales. They never change the native EQ's parameter limits.
internal sealed record DbScale(double Top, double Bottom)
{
    internal static readonly DbScale[] Presets = [new(24, -54), new(36, -72), new(18, -18), new(18, -36)];
    private static readonly int[] ReferenceTicks = [36, 24, 18, 12, 6, 3, 0, -3, -6, -12, -18, -24, -36, -48, -60, -72];
    internal double Span => Top - Bottom;
    internal string Label => $"+{Top:0} / −{-Bottom:0} dB";
    internal IEnumerable<int> Ticks => ReferenceTicks.Where(t => t <= Top && t >= Bottom)
        .Append((int)Top).Append((int)Bottom).Distinct().OrderDescending();
    internal double Normalize(double db) => (Top - Math.Clamp(db, Bottom, Top)) / Span;
    internal double FromNormalized(double position) => Top - Math.Clamp(position, 0, 1) * Span;
    internal double DragDelta(double upwardPixels, double height) => upwardPixels / height * Span;
    internal static DbScale At(int index) => Presets[Math.Clamp(index, 0, Presets.Length - 1)];
}
