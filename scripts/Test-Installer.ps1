param([Parameter(Mandatory)][string]$PayloadDir, [string]$InnoCompiler = 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe')
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$payload = (Resolve-Path -LiteralPath $PayloadDir).Path
$registry = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\ElkaVmEqInstallerSmokeTest_is1'
if (Test-Path -LiteralPath $registry) { throw 'A previous isolated installer test is still installed; inspect it before continuing.' }
$fixture = Join-Path $repo ('artifacts\installer-tests\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$installed = Join-Path $fixture 'Installed'
New-Item -ItemType Directory -Path $fixture | Out-Null
[xml]$project = Get-Content -LiteralPath "$repo\ui\Elka.VM.EQ.csproj"
$version = [string]$project.Project.PropertyGroup.Version
& $InnoCompiler '/DInstallerTest' "/DAppVersion=$version" "/DVersionNumbers=$(($version -split '-')[0])" "/DPayloadDir=$payload" "/DReleaseDir=$fixture" "$repo\packaging\ElkaVmEq.iss"
if ($LASTEXITCODE) { throw 'Test installer compilation failed.' }
function Invoke-TestProcess([string]$File, [string[]]$Arguments) {
    $process = Start-Process -FilePath $File -ArgumentList $Arguments -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit(60000)) { throw "Test process exceeded 60 seconds: $File. Inspect it before retrying." }
    if ($process.ExitCode -ne 0) { throw "Test process failed ($($process.ExitCode)): $File" }
}
function Assert-Payload {
    foreach ($file in Get-ChildItem -LiteralPath $payload -File -Recurse) {
        $relative = $file.FullName.Substring($payload.Length + 1)
        $target = Join-Path $installed $relative
        if (-not (Test-Path -LiteralPath $target)) { throw "Missing installed file: $relative" }
        if ((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath $target).Hash) { throw "Payload mismatch: $relative" }
    }
}
$arguments = @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-', ('/DIR="' + $installed + '"'), '/TASKS="!desktopicon"')
Invoke-TestProcess "$fixture\InstallerSmokeTest.exe" ($arguments + ('/LOG="' + $fixture + '\install.log"'))
Assert-Payload
$entry = Get-ItemProperty -LiteralPath $registry
if ($entry.InstallLocation.TrimEnd('\') -ne $installed) { throw 'Test uninstall registration points outside the fixture.' }
$shortcut = Join-Path ([Environment]::GetFolderPath('Programs')) 'ElkaVmEqInstallerSmokeTest\Elka VM EQ Test.lnk'
$shell = New-Object -ComObject WScript.Shell
if (-not (Test-Path -LiteralPath $shortcut) -or $shell.CreateShortcut($shortcut).TargetPath -ne "$installed\ElkaVmEq.exe") { throw 'Test shortcut mismatch.' }
$sentinel = Join-Path $installed 'user-preservation-check.txt'
[IO.File]::WriteAllText($sentinel, 'Installer test: preserve user-created files.')
$previousExtraction = $env:DOTNET_BUNDLE_EXTRACT_BASE_DIR
try {
    $env:DOTNET_BUNDLE_EXTRACT_BASE_DIR = Join-Path $fixture 'bundle-cache'
    Invoke-TestProcess "$installed\ElkaVmEq.exe" @('--self-test')
    Get-Content -LiteralPath "$installed\self-test.txt"
} finally { $env:DOTNET_BUNDLE_EXTRACT_BASE_DIR = $previousExtraction }
Invoke-TestProcess "$fixture\InstallerSmokeTest.exe" ($arguments + ('/LOG="' + $fixture + '\reinstall.log"'))
Assert-Payload
if ((Get-Content -LiteralPath $sentinel -Raw) -ne 'Installer test: preserve user-created files.') { throw 'Reinstall changed a user file.' }
Invoke-TestProcess "$installed\unins000.exe" @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', ('/LOG="' + $fixture + '\uninstall.log"'))
if ((Test-Path -LiteralPath "$installed\ElkaVmEq.exe") -or (Test-Path -LiteralPath $registry) -or (Test-Path -LiteralPath $shortcut)) { throw 'Test uninstall left a registered application behind.' }
if (-not (Test-Path -LiteralPath $sentinel)) { throw 'Uninstall removed the user-created sentinel.' }
$report = "PASS: installation, payload hashes, shortcut, bundled offline self-tests, reinstall, uninstall and user-file preservation. Version $version."
[IO.File]::WriteAllText((Join-Path $fixture 'result.txt'), $report)
Write-Output $report
Write-Output "Evidence retained: $fixture"
