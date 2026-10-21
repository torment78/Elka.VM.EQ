using System.ComponentModel;
using System.Diagnostics;
using System.Windows;

namespace Elka.VM.EQ;
public partial class MainWindow
{
    internal const string DonationUrl = "https://ko-fi.com/msffixit";
    // Injectable only for offline checks, so tests never launch a browser.
    internal Action<ProcessStartInfo> DonationLauncher { get; set; } = info => Process.Start(info);
    internal FrameworkElement DonationVisual => DonateButton;

    private void DonateClick(object sender, RoutedEventArgs e)
    {
        try { DonationLauncher(new ProcessStartInfo(DonationUrl) { UseShellExecute = true }); }
        catch (Exception ex) when (ex is Win32Exception or InvalidOperationException)
        {
            MessageBox.Show(this, $"Could not open your browser. You can support ElkaSoft at:\n\n{DonationUrl}",
                "Donate", MessageBoxButton.OK, MessageBoxImage.Information);
        }
    }
}
