param([ValidateSet('x64','Win32')][string]$platform='x64',[string]$outputTag='c3-windows-assets',[switch]$diagnostics,[string]$forceLodBias='')
$ErrorActionPreference='Stop'
$priorDiagnostics=$env:C3_DIAGNOSTICS
$priorBias=$env:C3_FORCE_LOD_BIAS
try {
Remove-Item Env:C3_DIAGNOSTICS,Env:C3_FORCE_LOD_BIAS -ErrorAction SilentlyContinue
$repo='\\Mac\Home\Desktop\4Story macOS\.local\codex'
$main='\\Mac\Home\Desktop\4Story macOS'
$tree=if($platform -eq 'x64'){'C:\4StoryAudit\Client64'}else{'C:\4StoryAudit\Client'}
$work="C:\4StoryAudit\C3Codex\$platform"
$out="$repo\.local\$outputTag\$platform"
$sdl="$main\.local\c3-win\sdl\SDL3-3.4.18"
if($diagnostics){
 foreach($scene in 'static','character','character-ffp'){
  if(Test-Path "$out\$scene-diagnostics"){throw "Diagnostics require a fresh outputTag: $out\$scene-diagnostics already exists"}
 }
}
New-Item -ItemType Directory -Force $work,$out | Out-Null
foreach($name in 'asset_probe.cpp','asset_scenes.h','asset_shaders.h','asset_diagnostics.h','checked_device.h','checked_device.inc','probe.cpp','features.h','formats.h','windows-assets.vcxproj'){Copy-Item "$repo\native\client\c3\$name" $work -Force}
$env:CL='/D_HAS_AUTO_PTR_ETC=1 /D_HAS_STD_BYTE=0 /FIfunctional /FIalgorithm'
& 'C:\4StoryAudit\BuildTools\MSBuild\Current\Bin\MSBuild.exe' "$work\windows-assets.vcxproj" /m /nologo /v:m /t:Rebuild /p:Configuration=Release "/p:Platform=$platform" /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0.26100.0 /p:PreferredToolArchitecture=arm64 "/p:SrcRoot=$tree\src" "/p:DumpSource=$work\asset_probe.cpp" "/p:C3Source=$work" "/p:SDLRoot=$sdl" > "$out\build.log" 2>&1
$code=$LASTEXITCODE
Get-Content "$out\build.log" | Select-String ': (fatal )?error |warning LNK|error LNK' | Select-Object -First 20 | ForEach-Object {$_.Line}
if($code -ne 0){exit $code}
$exe="$work\bin\$platform\c3-assets.exe"
$arch=if($platform -eq 'x64'){'x64'}else{'x86'}
Copy-Item "$sdl\lib\$arch\SDL3.dll" (Split-Path $exe) -Force
$d3dx=if($platform -eq 'x64'){'C:\Windows\System32\D3DX9_43.dll'}else{"$main\source\4Story 5.0 Client\D3DX9_43.dll"}
Copy-Item $d3dx (Split-Path $exe) -Force
$assets="$main\source\4Story 5.0 Client"
$shaders="$repo\native\client\reference\shaders"
if($diagnostics){$env:C3_DIAGNOSTICS='1'}
if($forceLodBias -ne ''){$env:C3_FORCE_LOD_BIAS=$forceLodBias}
$artifactHashes=@{}
foreach($scene in 'static','character','character-ffp'){
 $prefix="$work\run-$([Guid]::NewGuid().ToString('N'))-$scene"
 $args=@($prefix,$scene,"`"$assets`"","`"$shaders`"",'120','30')
 $p=Start-Process $exe -ArgumentList $args -WorkingDirectory $work -PassThru -RedirectStandardOutput "$out\$scene.log" -RedirectStandardError "$out\$scene.err"
 $null=$p.Handle
 if(!$p.WaitForExit(120000)){$p.Kill();throw "Timed out $scene"}
 $p.WaitForExit()
 "$platform $scene exit=$($p.ExitCode)"
 Get-Content "$out\$scene.log" | Select-Object -Last 5
 if($p.ExitCode -ne 0){Get-Content "$out\$scene.err";exit $p.ExitCode}
 if($diagnostics){Copy-Item "$prefix-diagnostics" "$out\$scene-diagnostics" -Recurse -Force -ErrorAction Stop}
 foreach($suffix in '.ppm','-first.ppm','-startup.ppm','-timing.tsv'){
  $destination="$out\$scene$suffix"
  Copy-Item "$prefix$suffix" $destination -Force -ErrorAction Stop
  $expected=(Get-FileHash "$prefix$suffix").Hash
  if((Get-FileHash $destination).Hash -ne $expected){throw "Artifact copy mismatch $destination"}
  $artifactHashes["$scene$suffix"]=$expected
 }
}
@{diagnostics=[bool]$diagnostics;forceLodBias=$forceLodBias;artifacts=$artifactHashes;platform=$platform;exeSHA256=(Get-FileHash $exe).Hash;sourceSHA256=(Get-FileHash "$work\asset_probe.cpp").Hash;engineSHA256=(Get-FileHash "$tree\src\Lib\Own\Engine Lib\Lib\$arch\Engine Lib.lib").Hash;d3dxSHA256=(Get-FileHash $d3dx).Hash;finished=(Get-Date -Format o)} | ConvertTo-Json | Set-Content -Encoding UTF8 "$out\manifest.json"
'PASS Windows original scenes'

} finally {
if($null -eq $priorDiagnostics){Remove-Item Env:C3_DIAGNOSTICS -ErrorAction SilentlyContinue}else{$env:C3_DIAGNOSTICS=$priorDiagnostics}
if($null -eq $priorBias){Remove-Item Env:C3_FORCE_LOD_BIAS -ErrorAction SilentlyContinue}else{$env:C3_FORCE_LOD_BIAS=$priorBias}
}
