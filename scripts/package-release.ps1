param([switch]$SkipBuild, [string]$InnoCompiler = 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe')
$ErrorActionPreference = 'Stop'
$releaseRoot = Split-Path -Parent $PSScriptRoot
if (-not $SkipBuild) { & "$PSScriptRoot\build.ps1" -Configuration Release }
[xml]$project = Get-Content -LiteralPath "$releaseRoot\ui\Elka.VM.EQ.csproj"
$version = [string]$project.Project.PropertyGroup.Version
$versionNumbers = ($version -split '-')[0]
$releaseDir = Join-Path $releaseRoot "artifacts\releases\v$version"
$payload = Join-Path $releaseDir 'Elka VM EQ'
if (Test-Path -LiteralPath $releaseDir) { throw "Release staging already exists: $releaseDir. Preserve it or choose a new version before rebuilding." }
New-Item -ItemType Directory -Path $payload -Force | Out-Null
dotnet publish "$releaseRoot\ui\Elka.VM.EQ.csproj" -c Release -p:PublishProfile=Portable -p:SkipNativeBuild=true -o $payload
if ($LASTEXITCODE) { throw 'Single-file publish failed.' }
$published = @(Get-ChildItem -LiteralPath $payload -File -Recurse)
if ($published.Count -ne 1 -or $published[0].Name -ne 'ElkaVmEq.exe') { throw 'Expected exactly one executable; loose publish files were found.' }
Copy-Item -LiteralPath "$releaseRoot\README.txt" -Destination "$payload\README.txt"

# Include the notices belonging to the actual restored runtime versions, not
# whichever other SDK/runtime happens to be installed on the packaging machine.
$assets = Get-Content -LiteralPath "$releaseRoot\ui\obj\project.assets.json" -Raw | ConvertFrom-Json
$packages = @($assets.packageFolders.PSObject.Properties.Name)
$notices = Join-Path $payload 'Notices'
New-Item -ItemType Directory -Path $notices | Out-Null
foreach ($runtime in @('Microsoft.NETCore.App.Runtime.win-x64', 'Microsoft.WindowsDesktop.App.Runtime.win-x64')) {
    $dependency = @($assets.project.frameworks.PSObject.Properties.Value.downloadDependencies | Where-Object name -EQ $runtime)
    if ($dependency.Count -ne 1 -or $dependency[0].version -notmatch '^\[([^,]+),\s*([^\]]+)\]$' -or $Matches[1] -ne $Matches[2]) { throw "Cannot resolve exact runtime notices for $runtime" }
    $packagePath = $runtime.ToLowerInvariant() + '/' + $Matches[1]
    $package = $packages | ForEach-Object { Join-Path $_ $packagePath } | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if (-not $package) { throw "Runtime package not found: $packagePath" }
    $license = @('LICENSE.TXT', 'LICENSE') | ForEach-Object { Join-Path $package $_ } | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if (-not $license) { throw "Missing runtime license: $package" }
    Copy-Item -LiteralPath $license -Destination (Join-Path $notices "$runtime-LICENSE.txt")
    $thirdParty = Join-Path $package 'THIRD-PARTY-NOTICES.TXT'
    if (Test-Path -LiteralPath $thirdParty) { Copy-Item -LiteralPath $thirdParty -Destination (Join-Path $notices "$runtime-THIRD-PARTY-NOTICES.txt") }
}
if (-not (Test-Path -LiteralPath $InnoCompiler)) { throw 'Install Inno Setup 6 or supply -InnoCompiler.' }
$installerAssets = Join-Path $releaseRoot 'packaging\Assets'
New-Item -ItemType Directory -Path $installerAssets -Force | Out-Null
$donateImage = Join-Path $installerAssets 'Donate.png'
$render = Start-Process -FilePath "$payload\ElkaVmEq.exe" -ArgumentList @('--render-donate', ('"' + $donateImage + '"')) -WindowStyle Hidden -PassThru -Wait
if ($render.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $donateImage)) { throw 'Installer donation button render failed.' }
& $InnoCompiler "/DAppVersion=$version" "/DVersionNumbers=$versionNumbers" "/DPayloadDir=$payload" "/DReleaseDir=$releaseDir" "$releaseRoot\packaging\ElkaVmEq.iss"
if ($LASTEXITCODE) { throw 'Inno Setup compilation failed.' }
$zip = Join-Path $releaseDir "Elka-VM-EQ-$version-Portable-win-x64.zip"
Compress-Archive -LiteralPath $payload -DestinationPath $zip -CompressionLevel Optimal
$artwork = Join-Path $releaseDir "Elka-VM-EQ-$version-Artwork.zip"
Copy-Item -LiteralPath "$releaseRoot\artifacts\Elka-VM-EQ-artwork.zip" -Destination $artwork
$files = @($zip, (Join-Path $releaseDir "Elka-VM-EQ-$version-Setup-x64.exe"), $artwork)
$hashes = $files | ForEach-Object { $hash = Get-FileHash -LiteralPath $_ -Algorithm SHA256; '{0}  {1}' -f $hash.Hash.ToLowerInvariant(), (Split-Path $_ -Leaf) }
[IO.File]::WriteAllLines((Join-Path $releaseDir "SHA256SUMS-$version.txt"), $hashes)
Write-Output "Release prepared: $releaseDir"
