#requires -Version 5.1
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$file = Join-Path $root 'packaging\make-windows-installer.ps1'
$tokens = $null
$parseErrors = $null
$null = [Management.Automation.Language.Parser]::ParseFile($file,[ref]$tokens,[ref]$parseErrors)
if ($parseErrors.Count) { throw ($parseErrors | Out-String) }

function Assert-Equal($Actual,$Expected,$Name) {
    if ($Actual -ne $Expected) { throw ($Name + ': expected [' + $Expected + '], got [' + $Actual + ']') }
    Write-Host ('PASS: ' + $Name)
}
function Assert-Rejected([scriptblock]$Action,[string]$Name) {
    $rejected = $false
    try { & $Action | Out-Null } catch { $rejected = $true }
    Assert-Equal $rejected $true $Name
}

$temporary = Join-Path ([IO.Path]::GetTempPath()) ('Tux installer tests ' + [Guid]::NewGuid().ToString('N'))
try {
    $null = New-Item -ItemType Directory -Path (Join-Path $temporary 'packaging')
    Copy-Item -LiteralPath $file -Destination (Join-Path $temporary 'packaging\make-windows-installer.ps1')
    # Import a copy so all generated output stays in this test's own folder.
    . (Join-Path $temporary 'packaging\make-windows-installer.ps1')
    Assert-Equal (ConvertTo-InstallerArgument "C:\Eric Richards\Tux & Radio's") '"C:\Eric Richards\Tux & Radio''s"' 'Paths with spaces and metacharacters remain one argument'
    Assert-Equal (ConvertTo-InstallerArgument 'C:\') '"C:\\"' 'Trailing path backslash is escaped'
    Assert-Rejected { ConvertTo-InstallerArgument 'bad"argument' } 'Embedded quotes rejected'

    $bundle = Join-Path $temporary 'dist\windows\Tux-sstv'
    foreach ($relative in @('tux-sstv.exe','tux-sstv-audio.exe','rigctl.exe','qt.conf','Qt6Core.dll',
            'Qt6Gui.dll','Qt6Widgets.dll','platforms\qwindows.dll','platforms\qoffscreen.dll',
            'licenses\COPYING.txt','multimedia\fixture.dll')) {
        $path = Join-Path $bundle $relative
        $null = New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force
        [IO.File]::WriteAllText($path,'fixture - never execute')
    }
    Assert-InstallerBundle $bundle
    Write-Host 'PASS: Complete portable fixture accepted without executing it'
    $plugin = Join-Path $bundle 'platforms\qoffscreen.dll'
    Remove-Item -LiteralPath $plugin
    Assert-Rejected { Assert-InstallerBundle $bundle } 'Incomplete Qt deployment rejected'
    [IO.File]::WriteAllText($plugin,'fixture')

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $badZip = Join-Path $temporary 'bad.zip'
    $zip = [IO.Compression.ZipFile]::Open($badZip,[IO.Compression.ZipArchiveMode]::Create)
    try { $null = $zip.CreateEntry('Tux-sstv/../../outside.txt') } finally { $zip.Dispose() }
    Assert-Rejected { Expand-InstallerPayload $badZip (Join-Path $temporary 'unpack') } 'ZIP path traversal rejected before extraction'
    Assert-Equal (Test-Path -LiteralPath (Join-Path $temporary 'outside.txt')) $false 'No file created outside the payload folder'

    $payload = Join-Path $temporary 'payload'
    $null = New-Item -ItemType Directory -Path $payload
    $zipPath = Join-Path $payload 'program.zip'
    [IO.File]::WriteAllText($zipPath,'checksum fixture')
    $manifest = @{ version='1.15.0'; archive='program.zip'; sha256=(Get-FileHash -LiteralPath $zipPath).Hash }
    $manifestFile = Join-Path $temporary 'installer-payload.json'
    $manifest | ConvertTo-Json | Set-Content -LiteralPath $manifestFile -Encoding Ascii
    Assert-Equal (Get-InstallerPayload $temporary).Version '1.15.0' 'Checksum-verified included program accepted'
    [IO.File]::AppendAllText($zipPath,'corrupt')
    Assert-Rejected { Get-InstallerPayload $temporary } 'Corrupted included program rejected'
    Remove-Item -LiteralPath $manifestFile

    function Get-AuthenticodeSignature {
        param([string]$LiteralPath)
        return [pscustomobject]@{ Status=$script:signatureStatus; SignerCertificate=[pscustomobject]@{ Subject=$script:signatureSubject } }
    }
    $script:signatureStatus = 'Valid'
    $script:signatureSubject = 'CN=Pyrsys B.V., O=Pyrsys B.V., C=NL'
    Assert-Equal (Test-InnoDownload $zipPath '') $true 'Official signed compiler download accepted'
    $script:signatureSubject = 'CN=Unrelated Publisher, C=US'
    Assert-Equal (Test-InnoDownload $zipPath '') $false 'Other publisher rejected'
    $script:signatureSubject = 'CN=Pyrsys B.V., O=Pyrsys B.V., C=NL'
    $script:signatureStatus = 'HashMismatch'
    Assert-Equal (Test-InnoDownload $zipPath '') $false 'Invalid compiler signature rejected'
    $hash = (Get-FileHash -LiteralPath $zipPath).Hash
    Assert-Equal (Test-InnoDownload $zipPath $hash) $true 'Published checksum accepted independently of old certificate stores'
    Assert-Equal (Test-InnoDownload $zipPath ('0'*64)) $false 'Published checksum mismatch rejected'

    [IO.File]::WriteAllText((Join-Path $temporary 'CMakeLists.txt'),'project(TuxSSTV VERSION 1.15.0 LANGUAGES CXX)')
    $null = New-Item -ItemType Directory -Path (Join-Path $temporary 'resources')
    [IO.File]::WriteAllText((Join-Path $temporary 'resources\tux-sstv.ico'),'icon fixture')
    $script:compilerFailure = $false
    function Get-InstallerCompiler { param([string]$RequestedPath) return 'fixture-compiler-never-executed.exe' }
    function Invoke-InstallerProcess {
        param([string]$Program,[string[]]$Arguments,[string]$LogPrefix)
        if ($script:compilerFailure) { return 2 }
        $out = @($Arguments | Where-Object { $_.StartsWith('/DInstallerOutput=') })[0].Substring('/DInstallerOutput='.Length)
        [IO.File]::WriteAllText((Join-Path $out 'Tux-sstv-1.15.0-windows-x64-setup.exe'),'installer fixture')
        return 0
    }
    $out = Join-Path $temporary "Output & Eric's files"
    Assert-Equal (Invoke-TuxInstallerBuild -RequestedBundle $bundle -RequestedOutput $out -KeepFolderClosed) 0 'Installer controller succeeds with compiler stand-in'
    $setup = Join-Path $out 'Tux-sstv-1.15.0-windows-x64-setup.exe'
    Assert-Equal (Test-Path -LiteralPath ($setup + '.sha256')) $true 'Installer checksum produced'
    $oldHash = (Get-FileHash -LiteralPath $setup).Hash
    $script:compilerFailure = $true
    Assert-Equal (Invoke-TuxInstallerBuild -RequestedBundle $bundle -RequestedOutput $out -KeepFolderClosed) 1 'Compiler failure reported even when an old installer exists'
    Assert-Equal (Get-FileHash -LiteralPath $setup).Hash $oldHash 'Failed compile preserves previous completed installer'
    Assert-Equal @(Get-ChildItem -LiteralPath $out -Directory -Filter '.installer-*' -Force).Count 0 'Temporary compilation folder cleaned'
} finally {
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Recurse -Force }
}
Write-Host 'PowerShell 5.1 installer controller checks passed. No tools downloaded, app started or real installer executed.'
