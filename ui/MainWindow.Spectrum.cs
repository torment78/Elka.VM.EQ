using System.Windows;
using System.Windows.Controls;

namespace Elka.VM.EQ;
public partial class MainWindow
{
    private bool _spectrumWanted, _changingSpectrum;
    private int _spectrumEdition = -1, _lastSpectrumState = -1;
    private uint _lastSpectrumSequence = uint.MaxValue;
    private (int Kind, int Index, int Channel)? _spectrumRoute;

    private void UpdateSpectrumSources()
    {
        if (_spectrumEdition != _frame.Edition)
        {
            _changingSpectrum = true;
            var choices = new List<string> { "Follow EQ · strip: insert tap" };
            int buses = _frame.Edition switch { 3 => 8, 2 => 5, 1 => 2, _ => 0 };
            for (int i = 0; i < buses; i++) choices.Add($"{Native.TargetName(_frame.Edition, 1, i)} · post-master mix");
            SpectrumSource.ItemsSource = choices; SpectrumSource.SelectedIndex = 0;
            _spectrumEdition = _frame.Edition; _changingSpectrum = false;
        }
        SpectrumPower.IsEnabled = !_demo && (_frame.Edition > 0 || _spectrumWanted);
        ApplySpectrumRoute();
    }
    private void ApplySpectrumRoute()
    {
        int source = SpectrumSource.SelectedIndex;
        (int Kind, int Index, int Channel) route = source > 0 ? (1, source - 1, _channel) : (_frame.Kind, _frame.Index, _channel);
        if (_spectrumRoute == route) return;
        _spectrumRoute = route; Graph.SetSpectrum(null); _lastSpectrumSequence = uint.MaxValue;
        if (_spectrumWanted && Native.SpectrumRequest(_engine, 1, route.Item1, route.Item2, route.Item3) == 0)
            StopSpectrum();
    }
    private void StopSpectrum()
    {
        Native.SpectrumRequest(_engine, 0, 0, 0, -1); _spectrumWanted = false;
        Graph.SetSpectrum(null); Active(SpectrumPower, false); SpectrumPower.Content = "RTA OFF";
    }
    private void SpectrumClick(object sender, RoutedEventArgs e)
    {
        if (_spectrumWanted) { StopSpectrum(); return; }
        if (MessageBox.Show(this, "Enable the optional MAIN audio callback?\n\nClose ElkaFX and other callback applications first. Audio will pass through unchanged. Turn RTA off before starting them again.\n\nFor a post-EQ spectrum, select a post-master bus mix. The strip tap is only documented after the input insert.", "Enable live spectrum", MessageBoxButton.OKCancel, MessageBoxImage.Information) != MessageBoxResult.OK) return;
        _spectrumWanted = true; _spectrumRoute = null; ApplySpectrumRoute(); PollSpectrum();
    }
    private void SpectrumSourceChanged(object sender, SelectionChangedEventArgs e)
    { if (!_changingSpectrum && IsInitialized && _frame.Bands != null) ApplySpectrumRoute(); }
    private void PollSpectrum()
    {
        if (_demo) return;
        if (Native.SpectrumRead(_engine, out var s, Native.SpectrumBytes) == 0) return;
        if (s.Requested == 0 && _spectrumWanted) _spectrumWanted = false;
        bool changed = s.Sequence != _lastSpectrumSequence || s.State != _lastSpectrumState;
        _lastSpectrumSequence = s.Sequence; _lastSpectrumState = s.State;
        if (changed) Graph.SetSpectrum(_spectrumWanted && s.State == 2 ? s.Db : null);
        SpectrumPower.Content = s.State switch { 2 => "RTA ON", 3 => "RTA BUSY", 4 => "RTA ERROR", 1 or 5 => "RTA WAIT", _ => "RTA OFF" };
        Active(SpectrumPower, _spectrumWanted && s.State == 2);
        SpectrumPower.ToolTip = s.Message;
        if (_spectrumWanted || s.State is 3 or 4)
            StatusText.Text = $"RTA · {s.Message}   ·   EQ curve is a calculated preview";
    }
    internal void ShowDemoSpectrum()
    {
        if (!_demo) return;
        var bins = Enumerable.Range(0, 384).Select(i =>
            (float)(-48 + 31 * Math.Exp(-Math.Pow((i - 178) / 11.0, 2)) + 12 * Math.Exp(-Math.Pow((i - 260) / 29.0, 2)) + 2 * Math.Sin(i * .47))).ToArray();
        Graph.SetSpectrum(bins); SpectrumPower.Content = "DEMO RTA";
        StatusText.Text = "OFFLINE DEMO · Synthetic spectrum for visual testing only · No audio callback";
    }
}
