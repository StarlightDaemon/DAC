# Dot-source this file; environment changes affect this PowerShell process only.
$dacRepository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$dacToolRoot = Join-Path $dacRepository '.tools'
$dacVcRoot = Join-Path $dacToolRoot 'msvc\Contents\VC\Tools\MSVC\14.44.35207'
$dacSdkRoot = Join-Path $dacToolRoot 'sdk'
$dacSdkVersion = '10.0.26100.0'
$dacCmakeRoot = Join-Path $dacToolRoot 'msvc\Contents\Common7\IDE\CommonExtensions\Microsoft\CMake'
$env:PATH = "$dacVcRoot\bin\Hostx64\x64;$dacSdkRoot\bin\$dacSdkVersion\x64;$dacCmakeRoot\CMake\bin;$dacCmakeRoot\Ninja;$env:PATH"
$env:INCLUDE = "$dacVcRoot\include;$dacSdkRoot\Include\$dacSdkVersion\ucrt;$dacSdkRoot\Include\$dacSdkVersion\shared;$dacSdkRoot\Include\$dacSdkVersion\um;$dacSdkRoot\Include\$dacSdkVersion\winrt"
$env:LIB = "$dacVcRoot\lib\x64;$dacSdkRoot\Lib\$dacSdkVersion\ucrt\x64;$dacSdkRoot\Lib\$dacSdkVersion\um\x64"
$env:LIBPATH = "$dacVcRoot\lib\x64"
$env:VCToolsInstallDir = "$dacVcRoot\"
$env:WindowsSdkDir = "$dacSdkRoot\"
$env:WindowsSDKVersion = "$dacSdkVersion\"
$env:VSCMD_ARG_TGT_ARCH = 'x64'
$env:VSCMD_ARG_HOST_ARCH = 'x64'
$env:CC = 'cl.exe'
$env:CXX = 'cl.exe'
# Compiler temporary files stay in this checkout as well.
$dacTemporary = Join-Path $dacToolRoot 'tmp'
New-Item -ItemType Directory -Path $dacTemporary -Force | Out-Null
$env:TEMP = $dacTemporary
$env:TMP = $dacTemporary
