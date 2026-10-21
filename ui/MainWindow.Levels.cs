using System.Globalization;

namespace Elka.VM.EQ;
internal static class LevelDisplay
{
    internal static string Format(LevelFrame levels, Frame selected, int channel)
    {
        if (levels.AgeMs > 500 || levels.Edition == 0 || levels.Edition != selected.Edition || levels.Kind != selected.Kind || levels.Index != selected.Index ||
            levels.Channels != selected.Channels || levels.Values is not { Length: 8 } || channel < -1 || channel >= levels.Channels) return "—";
        double peak = -1;
        for (int i = 0; i < levels.Channels; i++)
        {
            if (channel >= 0 && channel != i) continue;
            if ((levels.ValidMask & (1 << i)) == 0 || !float.IsFinite(levels.Values[i]) || levels.Values[i] < 0) return "—";
            peak = Math.Max(peak, levels.Values[i]);
        }
        if (peak < 0) return "—";
        if (peak == 0) return "−∞";
        double db = 20 * Math.Log10(peak);
        if (Math.Abs(db) < .05) db = 0;
        return db.ToString("+0.0;−0.0;0.0", CultureInfo.InvariantCulture);
    }
}
public partial class MainWindow
{
    private void PollLiveLevel()
    {
        if (_closed || _engine.IsClosed) return;
        LiveLevelValue.Text = !_demo && Native.ReadLevels(_engine, out var levels, Native.LevelsBytes) != 0
            ? LevelDisplay.Format(levels, _frame, _channel) : "—";
        string source = _frame.Kind == 0 ? "Input strip · pre-fader (not a documented pre-EQ tap)" : "Bus · output level";
        string channels = _channel < 0 ? "Maximum across the selected strip/bus channels" : $"Channel {_channel + 1}";
        LiveLevelWell.ToolTip = _demo ? "Offline demo: no live audio readings" : $"{source}\n{channels}\nCurrent API level in dBFS; no peak hold. Works with RTA off. — means unavailable; −∞ means digital silence.";
    }
}
