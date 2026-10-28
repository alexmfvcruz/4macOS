# Separate visual evidence run; hold is outside measured frames.
param([ValidateSet('x64','Win32')][string]$platform='x64')
$ErrorActionPreference='Stop'
$repo='\\Mac\Home\Desktop\4Story macOS\.local\codex'
$main='\\Mac\Home\Desktop\4Story macOS'
$work="C:\4StoryAudit\C3Codex\$platform"
$out="$repo\.local\c3-windows-assets\$platform"
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class C3Shot {
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
 [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
 [DllImport("kernel32.dll")] public static extern bool GetExitCodeProcess(IntPtr h,out uint code);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h,out RECT r);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h,out uint processId);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string c,string t);
}
"@
[C3Shot]::SetProcessDPIAware() | Out-Null
$env:C3_CAPTURE_HOLD_MS='10000'
foreach($scene in 'static','character','character-ffp'){
 $prefix="$work\shot-$scene-$([Guid]::NewGuid().ToString('N'))"
 $args=@($prefix,$scene,"`"$main\source\4Story 5.0 Client`"","`"$repo\native\client\reference\shaders`"",'120','30')
 $p=Start-Process "$work\bin\$platform\c3-assets.exe" -ArgumentList $args -WorkingDirectory $work -PassThru -RedirectStandardOutput "$out\shot-$scene.log" -RedirectStandardError "$out\shot-$scene.err"
 $processHandle=$p.Handle
 $deadline=(Get-Date).AddSeconds(120)
 while(!(Test-Path "$prefix.ppm") -and !$p.HasExited -and (Get-Date) -lt $deadline){Start-Sleep -Milliseconds 100}
 if(!(Test-Path "$prefix.ppm")){throw "No completed readback $scene"}
 $p.Refresh()
 $h=$p.MainWindowHandle
 if($h -eq [IntPtr]::Zero){throw "No scene window $scene"}
 [uint32]$ownerId=0
 [C3Shot]::GetWindowThreadProcessId($h,[ref]$ownerId) | Out-Null
 if($ownerId -ne $p.Id){throw "Wrong scene window process $scene"}
 [C3Shot]::SetForegroundWindow($h) | Out-Null
 Start-Sleep -Milliseconds 300
 $r=New-Object C3Shot+RECT
 if(![C3Shot]::GetWindowRect($h,[ref]$r)){throw 'GetWindowRect failed'}
 $bmp=New-Object System.Drawing.Bitmap ($r.R-$r.L),($r.B-$r.T)
 $graphics=[System.Drawing.Graphics]::FromImage($bmp)
 try{$graphics.CopyFromScreen($r.L,$r.T,0,0,$bmp.Size);$bmp.Save("$out\window-$scene.png",[System.Drawing.Imaging.ImageFormat]::Png)}finally{$graphics.Dispose();$bmp.Dispose()}
 $null=$p.Handle
 if(!$p.WaitForExit(20000)){$p.Kill();throw "Capture run did not finish $scene"}
 $p.WaitForExit()
 [uint32]$exitCode=259
 if(![C3Shot]::GetExitCodeProcess($processHandle,[ref]$exitCode) -or $exitCode -ne 0){throw "Capture run failed $scene exit=$exitCode"}
 "$platform $scene screenshot $($r.R-$r.L)x$($r.B-$r.T)"
}
Remove-Item Env:C3_CAPTURE_HOLD_MS
