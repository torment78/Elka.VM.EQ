param([ValidateSet('Debug','Release')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$nativeRoot=Split-Path -Parent $PSScriptRoot
$nativeCmake=Get-Command cmake.exe -ErrorAction SilentlyContinue
if($nativeCmake){$nativeCmake=$nativeCmake.Source}else{
 $nativeCmake='C:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
}
if(-not(Test-Path -LiteralPath $nativeCmake)){throw 'Install Visual Studio C++ desktop tools with CMake or add cmake.exe to PATH.'}
& $nativeCmake --fresh -S $nativeRoot -B "$nativeRoot\build" -G 'Visual Studio 18 2026' -A x64
if($LASTEXITCODE){throw 'CMake configuration failed.'}
& $nativeCmake --build "$nativeRoot\build" --config $Configuration
if($LASTEXITCODE){throw 'Build failed.'}
$nativeCTest=Join-Path (Split-Path $nativeCmake) 'ctest.exe'
& $nativeCTest --test-dir "$nativeRoot\build" -C $Configuration --output-on-failure
if($LASTEXITCODE){throw 'Tests failed.'}
dotnet build "$nativeRoot\ui\Elka.VM.EQ.csproj" -c $Configuration -p:SkipNativeBuild=true
if($LASTEXITCODE){throw 'WPF build failed.'}
$hybridTest=Start-Process -FilePath "$nativeRoot\ui\bin\$Configuration\net8.0-windows\ElkaVmEq.exe" -ArgumentList '--self-test' -WindowStyle Hidden -Wait -PassThru
Get-Content -LiteralPath "$nativeRoot\ui\bin\$Configuration\net8.0-windows\self-test.txt"
if($hybridTest.ExitCode){throw 'Hybrid integration tests failed.'}
