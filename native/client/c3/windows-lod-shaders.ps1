$ErrorActionPreference='Stop'
$repo='\\Mac\Home\Desktop\4Story macOS\.local\codex'
$inputDir="$repo\native\client\c3\lod-shaders"
$work="C:\4StoryAudit\C3LodShaders-$([Guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory $work | Out-Null
$files=Get-ChildItem $inputDir | Where-Object {$_.Extension -in '.vsh','.psh'} | ForEach-Object Name
Push-Location $work
try{& 'C:\4StoryAudit\c3shaders\assemble.exe' $inputDir @files;if($LASTEXITCODE -ne 0){throw 'Shader assembly failed'};Copy-Item "$work\*.bin" $inputDir -Force}finally{Pop-Location}
@{sources=@(Get-ChildItem $inputDir | Where-Object {$_.Extension -in '.vsh','.psh'} | ForEach-Object {@{name=$_.Name;SHA256=(Get-FileHash $_.FullName).Hash}});assemblerSHA256=(Get-FileHash 'C:\4StoryAudit\c3shaders\assemble.exe').Hash;d3dxSHA256=(Get-FileHash 'C:\4StoryAudit\c3shaders\D3DX9_43.dll').Hash;files=@(Get-ChildItem "$inputDir\*.bin" | ForEach-Object {@{name=$_.Name;SHA256=(Get-FileHash $_.FullName).Hash}})} | ConvertTo-Json -Depth 4 | Set-Content -Encoding UTF8 "$inputDir\manifest.json"
