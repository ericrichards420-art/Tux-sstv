#requires -Version 5.1
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$file = Join-Path $root 'packaging\bootstrap-windows.ps1'
$tokens = $null
$parseErrors = $null
$null = [Management.Automation.Language.Parser]::ParseFile($file,[ref]$tokens,[ref]$parseErrors)
if ($parseErrors.Count) { throw ($parseErrors | Out-String) }
. $file

function Assert-Equal($Actual,$Expected,$Name) {
    if ($Actual -ne $Expected) { throw ($Name + ': expected [' + $Expected + '], got [' + $Actual + ']') }
    Write-Host ('PASS: ' + $Name)
}
Assert-Equal (ConvertTo-MsysPath 'C:\Users\Eric Richards\Tux-sstv\packaging\auto-build-msys2.sh') '/c/Users/Eric Richards/Tux-sstv/packaging/auto-build-msys2.sh' 'Paths with spaces'
Assert-Equal (ConvertTo-MsysPath "D:\Tux & Radio's\build.sh") "/d/Tux & Radio's/build.sh" 'Path metacharacters remain data'
$rejected = $false
try { $null = ConvertTo-MsysPath '\\server\share\source' } catch { $rejected = $true }
Assert-Equal $rejected $true 'Network share rejected with a local extraction instruction'
Assert-Equal (Get-DefaultToolsDirectory 'C:\Users\Eric\AppData\Local' 'C:') 'C:\Users\Eric\AppData\Local\TuxSSTVBuild' 'Private user tool directory'
Assert-Equal (Get-DefaultToolsDirectory 'C:\Users\Eric Richards\AppData\Local' 'C:') 'C:\TuxSSTVBuild' 'Space-free tool-directory fallback'

foreach ($case in @(
    @{ Codes=@(0,0); ExpectSuccess=$true; Calls=2 },
    @{ Codes=@(1,0,0); ExpectSuccess=$true; Calls=3 },
    @{ Codes=@(0,1,0,0); ExpectSuccess=$true; Calls=4 },
    @{ Codes=@(1,1,1,1); ExpectSuccess=$false; Calls=4 }
)) {
    $queue = [Collections.Generic.Queue[int]]::new()
    foreach ($code in $case.Codes) { $queue.Enqueue($code) }
    $passed = $true
    try { Invoke-MsysUpdates { $queue.Dequeue() } } catch { $passed = $false }
    Assert-Equal $passed $case.ExpectSuccess ('Fresh-shell update sequence: ' + ($case.Codes -join ','))
    Assert-Equal ($case.Codes.Count-$queue.Count) $case.Calls 'Bounded update retry count'
}

$temporary = Join-Path ([IO.Path]::GetTempPath()) ('tux-bootstrap-test-' + [Guid]::NewGuid().ToString('N'))
try {
    [IO.File]::WriteAllBytes($temporary,[byte[]]@(1,2,3,4))
    $expected = (Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash
    Assert-Equal (Test-DownloadHash $temporary $expected) $true 'Matching download checksum'
    Assert-Equal (Test-DownloadHash $temporary ('0'*64)) $false 'Corrupt download rejected'
    Assert-Equal (Test-DownloadHash ($temporary+'.absent') $expected) $false 'Missing cache entry rejected'
} finally { if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary } }
Write-Host 'PowerShell 5.1 parser and bootstrap controller checks passed. No downloads or installers were run.'
