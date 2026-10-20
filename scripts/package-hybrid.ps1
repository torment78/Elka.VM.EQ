param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$hybridRoot=Split-Path -Parent $PSScriptRoot
if(-not $SkipBuild){& "$PSScriptRoot\build.ps1" -Configuration Release}
$hybridOutput=Join-Path $hybridRoot 'artifacts\Elka VM EQ Hybrid'
dotnet publish "$hybridRoot\ui\Elka.VM.EQ.csproj" -c Release --self-contained false -p:SkipNativeBuild=true -o $hybridOutput
if($LASTEXITCODE){throw 'Hybrid publish failed.'}
Copy-Item -LiteralPath "$hybridRoot\README-HYBRID.txt" -Destination (Join-Path $hybridOutput 'README.txt')
$engineSource=Get-FileHash -LiteralPath "$hybridRoot\build\Release\ElkaEqEngine.dll"
$enginePublished=Get-FileHash -LiteralPath (Join-Path $hybridOutput 'ElkaEqEngine.dll')
if($engineSource.Hash -ne $enginePublished.Hash){throw 'Native engine package hash mismatch.'}
Write-Output "Hybrid app: $hybridOutput\ElkaVmEq.exe"
