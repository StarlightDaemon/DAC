[CmdletBinding()]
param([ValidateSet('Debug','Release')][string]$Configuration='Release',[switch]$Test,[switch]$UseInstalledToolchain)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(!$UseInstalledToolchain){. (Join-Path $PSScriptRoot 'use-toolchain.ps1')}
$build=Join-Path $root ('build/'+$Configuration.ToLowerInvariant())
& cmake -S $root -B $build -G Ninja "-DCMAKE_BUILD_TYPE=$Configuration"
if($LASTEXITCODE -ne 0){throw 'CMake configure failed'}
& cmake --build $build --parallel 3
if($LASTEXITCODE -ne 0){throw 'Compilation failed'}
if($Test){
    & ctest --test-dir $build --output-on-failure --test-output-size-passed 1048576 --test-output-size-failed 1048576 --output-junit (Join-Path $build 'results.xml')
    if($LASTEXITCODE -ne 0){throw 'Automated verification failed or was unavailable'}
}
