param([ValidateSet('x64','Win32')][string]$platform='x64')
$ErrorActionPreference='Stop'
$repo='\\Mac\Home\Desktop\4Story macOS\.local\codex'
$main='\\Mac\Home\Desktop\4Story macOS'
$work="C:\4StoryAudit\C3Lod\$platform"
$out="$repo\.local\c3-lod-windows\$platform"
$sdl="$main\.local\c3-win\sdl\SDL3-3.4.18"
New-Item -ItemType Directory -Force $work,$out | Out-Null
foreach($f in 'lod_probe.cpp','probe.cpp','features.h','formats.h'){Copy-Item "$repo\native\client\c3\$f" $work -Force}
$vc=if($platform -eq 'x64'){'arm64_x64'}else{'arm64_x86'}
$arch=if($platform -eq 'x64'){'x64'}else{'x86'}
cmd /c "`"C:\4StoryAudit\BuildTools\VC\Auxiliary\Build\vcvarsall.bat`" $vc >nul && cd /d $work && cl /nologo /std:c++17 /EHsc /O2 /MD lod_probe.cpp /I`"$sdl\include`" /Fec3-lod.exe /link d3d9.lib `"$sdl\lib\$arch\SDL3.lib`" > `"$out\build.log`" 2>&1"
if($LASTEXITCODE -ne 0){Get-Content "$out\build.log" | Select-Object -Last 15;exit $LASTEXITCODE}
Copy-Item "$sdl\lib\$arch\SDL3.dll" $work -Force
& "$work\c3-lod.exe" "$work\lod" "$repo\native\client\c3\lod-shaders" > "$out\run.log" 2>&1
$code=$LASTEXITCODE
Get-Content "$out\run.log" | Select-Object -Last 5
if(Test-Path "$work\lod.ppm"){Copy-Item "$work\lod.ppm" $out -Force}
@{exit=$code;binarySHA256=(Get-FileHash "$work\c3-lod.exe").Hash;sourceSHA256=(Get-FileHash "$work\lod_probe.cpp").Hash} | ConvertTo-Json | Set-Content -Encoding UTF8 "$out\manifest.json"
exit $code
