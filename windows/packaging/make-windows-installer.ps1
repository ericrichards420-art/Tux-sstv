#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$BundleDirectory = '',
    [string]$OutputDirectory = '',
    [string]$CompilerPath = '',
    [switch]$NoOpenFolder
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function ConvertTo-InstallerArgument {
    param([string]$Value)
    if ($Value.Contains('"') -or $Value.Contains("`n") -or $Value.Contains("`r")) {
        throw 'An installer argument contains an unsupported quote or newline.'
    }
    # Win32 command-line parsing treats backslashes immediately before a
    # closing quote specially. Double only those trailing backslashes.
    return '"' + [regex]::Replace($Value, '(\\+)$', '$1$1') + '"'
}

function Invoke-InstallerProcess {
    param([string]$Program, [string[]]$Arguments, [string]$LogPrefix = '')
    $quoted = @($Arguments | ForEach-Object { ConvertTo-InstallerArgument $_ })
    $parameters = @{ FilePath=$Program; ArgumentList=$quoted; NoNewWindow=$true; PassThru=$true }
    if ($LogPrefix) {
        $parameters.RedirectStandardOutput = $LogPrefix + '-stdout.log'
        $parameters.RedirectStandardError = $LogPrefix + '-stderr.log'
    }
    $process = Start-Process @parameters
    try {
        $null = $process.Handle
        $process.WaitForExit()
        $process.Refresh()
        if ($null -eq $process.ExitCode) { throw ('No exit status from ' + $Program) }
        if ($LogPrefix) {
            foreach ($log in @(($LogPrefix + '-stdout.log'), ($LogPrefix + '-stderr.log'))) {
                if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log -Tail 50 | ForEach-Object { Write-Host $_ } }
            }
        }
        return [int]$process.ExitCode
    } finally { $process.Dispose() }
}

function Assert-InstallerBundle {
    param([string]$Path)
    foreach ($relative in @('tux-sstv.exe','tux-sstv-audio.exe','rigctl.exe','qt.conf',
            'Qt6Core.dll','Qt6Gui.dll','Qt6Widgets.dll',
            'platforms\qwindows.dll','platforms\qoffscreen.dll')) {
        $file = Join-Path $Path $relative
        if (-not (Test-Path -LiteralPath $file -PathType Leaf) -or (Get-Item -LiteralPath $file).Length -eq 0) {
            throw ('Incomplete portable program: missing ' + $relative + '. Keep the whole working program folder together.')
        }
    }
    if (-not (Test-Path -LiteralPath (Join-Path $Path 'licenses') -PathType Container) -or
            @(Get-ChildItem -LiteralPath (Join-Path $Path 'licenses') -Recurse -File).Count -eq 0) {
        throw 'The portable program is missing its license notices.'
    }
    if (-not (Test-Path -LiteralPath (Join-Path $Path 'multimedia') -PathType Container) -or
            @(Get-ChildItem -LiteralPath (Join-Path $Path 'multimedia') -Filter '*.dll' -File).Count -eq 0) {
        throw 'The portable program is missing its multimedia plugins.'
    }
}

function Get-InstallerPayload {
    param([string]$Root)
    $manifestFile = Join-Path $Root 'installer-payload.json'
    $manifest = Get-Content -LiteralPath $manifestFile -Raw | ConvertFrom-Json
    if ($manifest.version -notmatch '^\d+\.\d+\.\d+$' -or
            $manifest.archive -notmatch '^[A-Za-z0-9._-]+\.zip$' -or
            $manifest.sha256 -notmatch '^[a-fA-F0-9]{64}$') {
        throw 'The installer payload manifest is invalid. Extract the complete installer-maker ZIP again.'
    }
    $archive = Join-Path (Join-Path $Root 'payload') $manifest.archive
    if (-not (Test-Path -LiteralPath $archive -PathType Leaf) -or
            (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $manifest.sha256) {
        throw 'The working Windows program ZIP failed its SHA-256 check. Copy the complete installer-maker ZIP again.'
    }
    return [pscustomobject]@{ Version=[string]$manifest.version; Archive=$archive }
}

function Expand-InstallerPayload {
    param([string]$Archive, [string]$Destination)
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead($Archive)
    try {
        foreach ($entry in $zip.Entries) {
            $name = $entry.FullName.Replace('\','/')
            if (-not $name.StartsWith('Tux-sstv/', [StringComparison]::Ordinal) -or
                    $name -match '(^|/)\.\.?(/|$)|:|^/' -or $name.Contains([char]0)) {
                throw ('Unexpected path in the program ZIP: ' + $entry.FullName)
            }
        }
    } finally { $zip.Dispose() }
    Expand-Archive -LiteralPath $Archive -DestinationPath $Destination
    return Join-Path $Destination 'Tux-sstv'
}

function Get-InnoPublishedHash {
    # GitHub publishes release-asset SHA-256 digests. If unavailable, the
    # download must instead have a valid signature from the official publisher.
    try {
        $release = Invoke-RestMethod -Uri 'https://api.github.com/repos/jrsoftware/issrc/releases/tags/is-6_7_4' `
            -Headers @{ Accept='application/vnd.github+json' } -UserAgent 'Tux-sstv-Installer-Maker' -TimeoutSec 30
        $asset = @($release.assets | Where-Object { $_.name -eq 'innosetup-6.7.4.exe' })
        if ($asset.Count -eq 1 -and $asset[0].PSObject.Properties['digest']) {
            $digest = [string]$asset[0].digest
            if ($digest -match '^sha256:([a-fA-F0-9]{64})$') { return $Matches[1] }
        }
    } catch { Write-Host 'Published checksum unavailable; checking the official publisher signature instead.' }
    return ''
}

function Test-InnoDownload {
    param([string]$Path, [string]$PublishedHash)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $false }
    if ($PublishedHash) { return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -eq $PublishedHash }
    $signature = Get-AuthenticodeSignature -LiteralPath $Path
    return $signature.Status -eq 'Valid' -and $null -ne $signature.SignerCertificate -and
        $signature.SignerCertificate.Subject -match '(^|,\s*)CN="?Pyrsys B\.V\."?(,|$)'
}

function Get-InstallerCompiler {
    param([string]$RequestedPath)
    if (-not $RequestedPath) { $RequestedPath = $env:TUX_ISCC }
    if ($RequestedPath) {
        $requested = [IO.Path]::GetFullPath($RequestedPath)
        if (-not (Test-Path -LiteralPath $requested -PathType Leaf)) { throw ('ISCC.exe was not found: ' + $requested) }
        return $requested
    }
    # Keep compiler tools private. No admin rights or global PATH changes.
    $cache = Join-Path $env:LOCALAPPDATA 'TuxSSTVInstallerTools'
    $compilerHome = Join-Path $cache 'InnoSetup-6.7.4'
    $compiler = Join-Path $compilerHome 'ISCC.exe'
    $ready = Join-Path $compilerHome '.tux-compiler-ready'
    if ((Test-Path -LiteralPath $ready) -and (Test-Path -LiteralPath $compiler)) { return $compiler }
    $null = New-Item -ItemType Directory -Path $cache -Force
    $download = Join-Path $cache 'innosetup-6.7.4.exe'
    $publishedHash = Get-InnoPublishedHash
    if (-not (Test-InnoDownload $download $publishedHash)) {
        $temporary = $download + '.download.exe'
        try {
            Write-Host 'Downloading Inno Setup 6.7.4 from its official release ...'
            Invoke-WebRequest -UseBasicParsing `
                -Uri 'https://github.com/jrsoftware/issrc/releases/download/is-6_7_4/innosetup-6.7.4.exe' `
                -OutFile $temporary -TimeoutSec 600
            if (-not (Test-InnoDownload $temporary $publishedHash)) {
                throw 'The Inno Setup download could not be verified against its published checksum or publisher signature.'
            }
            Move-Item -LiteralPath $temporary -Destination $download -Force
        } finally { if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force } }
    }
    Write-Host 'Preparing the installer compiler (no administrator rights needed) ...'
    $code = Invoke-InstallerProcess $download @('/PORTABLE=1','/CURRENTUSER','/VERYSILENT',
        '/SUPPRESSMSGBOXES','/SP-','/NORESTART','/NOICONS',('/DIR=' + $compilerHome))
    if ($code -ne 0 -or -not (Test-Path -LiteralPath $compiler)) { throw ('Installer compiler preparation failed, exit code ' + $code) }
    Set-Content -LiteralPath $ready -Value 'Tux-sstv private Inno Setup 6.7.4 compiler' -Encoding Ascii
    return $compiler
}

function Invoke-TuxInstallerBuild {
    param([string]$RequestedBundle, [string]$RequestedOutput, [string]$RequestedCompiler, [switch]$KeepFolderClosed)
    $root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
    $temporary = $null
    $mutex = $null
    $lockHeld = $false
    $transcriptStarted = $false
    $logDirectory = Join-Path $root 'installer-logs'
    try {
        if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT -or -not [Environment]::Is64BitOperatingSystem) {
            throw 'Run Make Windows Installer.cmd on 64-bit Windows 10 or Windows 11.'
        }
        $null = New-Item -ItemType Directory -Path $logDirectory -Force
        Start-Transcript -LiteralPath (Join-Path $logDirectory 'make-installer.log') -Append | Out-Null
        $transcriptStarted = $true
        $mutex = [Threading.Mutex]::new($false, 'Local\TuxSSTVInstallerMaker')
        try { $lockHeld = $mutex.WaitOne(0) } catch [Threading.AbandonedMutexException] { $lockHeld = $true }
        if (-not $lockHeld) { throw 'Another Tux-sstv installer build is running. Wait for it to finish.' }
        Write-Host 'Tux-sstv: make a Windows installer from the working program'
        [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
        $ProgressPreference = 'SilentlyContinue'
        if (Test-Path -LiteralPath (Join-Path $root 'installer-payload.json')) {
            $payload = Get-InstallerPayload $root
            $version = $payload.Version
            if ($RequestedBundle) { throw 'This installer-maker kit uses its included, checksum-verified Windows program.' }
            $temporary = Join-Path ([IO.Path]::GetTempPath()) ('Tux-sstv-installer-' + [Guid]::NewGuid().ToString('N'))
            $null = New-Item -ItemType Directory -Path $temporary
            Write-Host 'Unpacking the working Windows program. No program recompilation is needed.'
            $bundle = Expand-InstallerPayload $payload.Archive $temporary
            $defaultOutput = Join-Path $root 'output'
        } else {
            $cmake = Get-Content -LiteralPath (Join-Path $root 'CMakeLists.txt') -Raw
            if ($cmake -notmatch 'project\(TuxSSTV VERSION (\d+\.\d+\.\d+)') { throw 'Cannot read the Tux-sstv version.' }
            $version = $Matches[1]
            $bundle = $RequestedBundle
            if (-not $bundle) { $bundle = Join-Path $root 'dist\windows\Tux-sstv' }
            $defaultOutput = Join-Path $root 'dist\windows'
        }
        $bundle = [IO.Path]::GetFullPath($bundle)
        Assert-InstallerBundle $bundle
        $output = $RequestedOutput
        if (-not $output) { $output = $defaultOutput }
        $output = [IO.Path]::GetFullPath($output)
        $null = New-Item -ItemType Directory -Path $output -Force
        $icon = Join-Path $root 'resources\tux-sstv.ico'
        if (-not (Test-Path -LiteralPath $icon)) { throw 'Missing Tux camera icon. Extract the entire kit again.' }
        $compiler = Get-InstallerCompiler $RequestedCompiler
        $setup = Join-Path $output ('Tux-sstv-' + $version + '-windows-x64-setup.exe')
        # Compile to a new directory, so a failure cannot be mistaken for an
        # old setup.exe left over from a previous successful run.
        $compileDirectory = Join-Path $output ('.installer-' + [Guid]::NewGuid().ToString('N'))
        $null = New-Item -ItemType Directory -Path $compileDirectory
        try {
            Write-Host 'Creating Setup.exe, including the Start menu icon ...'
            $code = Invoke-InstallerProcess $compiler @('/Qp',('/DAppVersion=' + $version),
                ('/DBundleDir=' + $bundle),('/DAppIcon=' + $icon),('/DInstallerOutput=' + $compileDirectory),
                (Join-Path $root 'packaging\tux-sstv-windows.iss')) -LogPrefix (Join-Path $logDirectory 'compiler')
            $compiled = Join-Path $compileDirectory ([IO.Path]::GetFileName($setup))
            if ($code -ne 0 -or -not (Test-Path -LiteralPath $compiled -PathType Leaf)) {
                throw ('Installer compilation failed, exit code ' + $code + '. See installer-logs\make-installer.log.')
            }
            Move-Item -LiteralPath $compiled -Destination $setup -Force
        } finally { Remove-Item -LiteralPath $compileDirectory -Recurse -Force }
        $hash = (Get-FileHash -LiteralPath $setup -Algorithm SHA256).Hash.ToLowerInvariant()
        Set-Content -LiteralPath ($setup + '.sha256') -Encoding Ascii -Value ($hash + '  ' + [IO.Path]::GetFileName($setup))
        Write-Host ''
        Write-Host 'INSTALLER READY.' -ForegroundColor Green
        Write-Host $setup
        Write-Host 'Run this Setup.exe to install Tux-sstv and its Start menu icon. A desktop shortcut is optional.'
        if (-not $KeepFolderClosed) {
            try { Start-Process -FilePath 'explorer.exe' -ArgumentList (ConvertTo-InstallerArgument $output) | Out-Null }
            catch { Write-Host ('Open this folder: ' + $output) }
        }
        return 0
    } catch {
        Write-Host ''
        Write-Host ('INSTALLER STOPPED: ' + $_.Exception.Message) -ForegroundColor Red
        Write-Host ('Details: ' + (Join-Path $logDirectory 'make-installer.log'))
        return 1
    } finally {
        if ($temporary -and (Test-Path -LiteralPath $temporary)) { Remove-Item -LiteralPath $temporary -Recurse -Force }
        if ($lockHeld -and $null -ne $mutex) { $mutex.ReleaseMutex() }
        if ($null -ne $mutex) { $mutex.Dispose() }
        if ($transcriptStarted) { Stop-Transcript | Out-Null }
    }
}

# Importing for controller tests never downloads tools or runs programs.
if ($MyInvocation.InvocationName -ne '.') {
    exit (Invoke-TuxInstallerBuild -RequestedBundle $BundleDirectory -RequestedOutput $OutputDirectory `
        -RequestedCompiler $CompilerPath -KeepFolderClosed:$NoOpenFolder)
}
