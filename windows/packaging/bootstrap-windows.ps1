#requires -Version 5.1
[CmdletBinding()]
param(
    [string]$ToolsDirectory = '',
    [switch]$NoOpenFolder
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function ConvertTo-MsysPath {
    param([Parameter(Mandatory=$true)][string]$WindowsPath)
    if ($WindowsPath -notmatch '^[A-Za-z]:[\\/]') {
        throw 'Extract the kit to a local drive, such as your Downloads folder, instead of a network share.'
    }
    $full = [IO.Path]::GetFullPath($WindowsPath)
    return '/' + $full.Substring(0,1).ToLowerInvariant() + '/' + $full.Substring(3).Replace('\','/')
}

function Get-DefaultToolsDirectory {
    param([string]$LocalAppData, [string]$SystemDrive)
    if ($LocalAppData -and $LocalAppData -notmatch '[^\x21-\x7e]') {
        return Join-Path $LocalAppData 'TuxSSTVBuild'
    }
    return Join-Path ($SystemDrive + '\') 'TuxSSTVBuild'
}

function Test-DownloadHash {
    param([string]$Path, [string]$Expected)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $false }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -eq $Expected
}

function Get-VerifiedDownload {
    param([string[]]$Urls, [string]$Destination, [string]$Sha256)
    if (Test-DownloadHash $Destination $Sha256) {
        Write-Host 'Using the verified cached MSYS2 download.'
        return
    }
    $temporary = $Destination + '.download'
    foreach ($url in $Urls) {
        for ($attempt=1; $attempt -le 2; $attempt++) {
            try {
                Write-Host ('Downloading MSYS2 from ' + ([Uri]$url).Host + ' ...')
                Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $temporary -TimeoutSec 600
                if (-not (Test-DownloadHash $temporary $Sha256)) {
                    throw 'The downloaded MSYS2 file did not match its published SHA-256 checksum.'
                }
                Move-Item -LiteralPath $temporary -Destination $Destination -Force
                return
            } catch {
                Write-Host ('Download attempt failed: ' + $_.Exception.Message)
                if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force }
                if ($attempt -lt 2) { Start-Sleep -Seconds 2 }
            }
        }
    }
    throw 'MSYS2 could not be downloaded and verified. Check the Internet connection and run the script again.'
}

function Invoke-NativeProcess {
    param([string]$Program, [string[]]$Arguments)
    # Arguments supplied here are fixed options or quoted paths, never shell code.
    $process = Start-Process -FilePath $Program -ArgumentList $Arguments -NoNewWindow -PassThru
    try {
        $null = $process.Handle
        $process.WaitForExit()
        $process.Refresh()
        if ($null -eq $process.ExitCode) { throw ('No exit status from ' + $Program) }
        return [int]$process.ExitCode
    } finally { $process.Dispose() }
}

function Invoke-MsysStage {
    param([string]$Bash, [string]$Script, [ValidateSet('initialize','update','build')][string]$Stage)
    $path = ConvertTo-MsysPath $Script
    # Windows filenames cannot contain double quotes; the path is a script
    # argument to bash, not an interpolated bash -c expression.
    return Invoke-NativeProcess $Bash @('--login', ('"' + $path + '"'), $Stage)
}

function Invoke-MsysUpdates {
    param([Parameter(Mandatory=$true)][scriptblock]$RunUpdate)
    $successes = 0
    for ($attempt=1; $attempt -le 4; $attempt++) {
        Write-Host ('Updating MSYS2 in a fresh session (' + $attempt + '/4) ...')
        $code = & $RunUpdate
        if ($code -eq 0) {
            $successes++
            if ($successes -ge 2) { return }
        } else {
            $successes = 0
            Write-Host 'The update session ended early. Retrying in a new shell; a core update can close its old shell.'
        }
    }
    throw 'MSYS2 updates did not finish successfully. See automatic-build.log and run this script again after correcting the reported error.'
}

function Invoke-TuxAutomaticBuild {
    param([string]$RequestedToolsDirectory, [switch]$KeepFolderClosed)
    $transcriptStarted = $false
    $mutex = $null
    $lockHeld = $false
    $source = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
    $logDirectory = Join-Path $source 'build-windows-logs'
    try {
        if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT -or -not [Environment]::Is64BitOperatingSystem) {
            throw 'This build script requires 64-bit Windows 10 (1809 or newer) or Windows 11.'
        }
        $build = [int](Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion').CurrentBuildNumber
        if ($build -lt 17763) { throw 'Windows 10 version 1809 or newer is required.' }
        foreach ($file in @('CMakeLists.txt','packaging\auto-build-msys2.sh','packaging\build-windows.sh','packaging\patch-hamlib-windows.sh','packaging\normalize-windows-source-times.sh','packaging\deploy-windows-platforms.sh','packaging\check-windows-stage.sh','packaging\make-windows-installer.ps1','packaging\tux-sstv-windows.iss','resources\tux-sstv.ico')) {
            if (-not (Test-Path -LiteralPath (Join-Path $source $file) -PathType Leaf)) {
                throw 'The source kit is incomplete. Extract the entire ZIP and run Install and Build Tux-sstv.cmd inside it.'
            }
        }
        $null = ConvertTo-MsysPath $source
        $null = New-Item -ItemType Directory -Path $logDirectory -Force
        Start-Transcript -LiteralPath (Join-Path $logDirectory 'bootstrap.log') -Append | Out-Null
        $transcriptStarted = $true
        Write-Host 'Tux-sstv: automatic Windows download and build'
        Write-Host 'This installs the build tools and compiles the development preview. Codex is not required.'
        Write-Host ('Source folder: ' + $source)
        Write-Host 'Downloads and compilation may take a while. Progress and errors are saved in build-windows-logs.'
        [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
        $ProgressPreference = 'SilentlyContinue'

        if ($env:TUX_MSYS2_ROOT) {
            $msysRoot = [IO.Path]::GetFullPath($env:TUX_MSYS2_ROOT)
            if (-not (Test-Path -LiteralPath (Join-Path $msysRoot 'usr\bin\bash.exe'))) {
                throw 'TUX_MSYS2_ROOT does not point to an existing MSYS2 installation. Clear it to let this script install its own tools.'
            }
            Write-Host ('Using the explicitly configured MSYS2 installation: ' + $msysRoot)
            $installBase = $null
        } else {
            $installBase = $RequestedToolsDirectory
            if (-not $installBase) { $installBase = $env:TUX_BUILD_TOOLS_DIRECTORY }
            if (-not $installBase) { $installBase = Get-DefaultToolsDirectory $env:LOCALAPPDATA $env:SystemDrive }
            $installBase = [IO.Path]::GetFullPath($installBase).TrimEnd([char[]]@('\','/'))
            if ($installBase -match '[^\x21-\x7e]' -or $installBase -match '^[A-Za-z]:$') {
                throw 'MSYS2 needs a tool directory without spaces or non-ASCII characters. Set TUX_BUILD_TOOLS_DIRECTORY to a writable folder such as C:\TuxSSTVBuild.'
            }
            $marker = Join-Path $installBase '.tux-sstv-build-tools'
            if ((Test-Path -LiteralPath $installBase) -and -not (Test-Path -LiteralPath $marker)) {
                if (@(Get-ChildItem -LiteralPath $installBase -Force).Count -gt 0) {
                    throw ('The tool directory belongs to another installation: ' + $installBase + '. Choose a new TUX_BUILD_TOOLS_DIRECTORY.')
                }
            }
            $null = New-Item -ItemType Directory -Path $installBase -Force
            Set-Content -LiteralPath $marker -Value 'Tux-sstv managed build tools' -Encoding Ascii
            $msysRoot = Join-Path $installBase 'msys64'
        }

        $hasher = [Security.Cryptography.SHA256]::Create()
        try { $lockId = ([BitConverter]::ToString($hasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($msysRoot.ToLowerInvariant())))).Replace('-','').Substring(0,24) }
        finally { $hasher.Dispose() }
        $mutex = [Threading.Mutex]::new($false, ('Local\TuxSSTVBuild-' + $lockId))
        try { $lockHeld = $mutex.WaitOne(0) } catch [Threading.AbandonedMutexException] { $lockHeld = $true }
        if (-not $lockHeld) { throw 'Another Tux-sstv build is using these tools. Wait for its window to finish.' }

        if ($installBase) {
            $ready = Join-Path $installBase '.msys2-base-extracted'
            $bash = Join-Path $msysRoot 'usr\bin\bash.exe'
            if (-not (Test-Path -LiteralPath $ready) -or -not (Test-Path -LiteralPath $bash)) {
                $archive = Join-Path $installBase 'msys2-base-x86_64-20261120.sfx.exe'
                $hash = '4f6dadfdb2c9be5fc1938e30b635cb72bb4344c35e8b6fd225bc3250fcab5897'
                Get-VerifiedDownload @(
                    'https://github.com/msys2/msys2-installer/releases/download/2026-11-20/msys2-base-x86_64-20261120.sfx.exe',
                    'https://repo.msys2.org/distrib/x86_64/msys2-base-x86_64-20261120.sfx.exe'
                ) $archive $hash
                Write-Host ('Installing private build tools in ' + $msysRoot)
                $code = Invoke-NativeProcess $archive @('-y', ('-o"' + $installBase + '"'))
                if ($code -ne 0 -or -not (Test-Path -LiteralPath $bash)) { throw ('MSYS2 extraction failed with exit code ' + $code) }
                Set-Content -LiteralPath $ready -Value 'MSYS2 2026-11-20 base extracted' -Encoding Ascii
            }
        }

        $bash = Join-Path $msysRoot 'usr\bin\bash.exe'
        $env:MSYSTEM = 'UCRT64'
        $env:CHERE_INVOKING = '1'
        $env:TUX_MSYS2_ROOT = $msysRoot
        $env:TUX_UNATTENDED = '1'
        if (-not $env:TUX_BUILD_JOBS) { $env:TUX_BUILD_JOBS = [string][Math]::Max(1,[Math]::Min(4,[Environment]::ProcessorCount-1)) }
        $script = Join-Path $source 'packaging\auto-build-msys2.sh'
        $code = Invoke-MsysStage $bash $script 'initialize'
        if ($code -ne 0) { throw ('MSYS2 initialization failed with exit code ' + $code) }
        Invoke-MsysUpdates { Invoke-MsysStage $bash $script 'update' }
        Write-Host 'Installing compiler, Qt and USB dependencies, then building Tux-sstv ...'
        $code = Invoke-MsysStage $bash $script 'build'
        if ($code -ne 0) { throw ('The Windows build stopped with exit code ' + $code + '. See automatic-build.log and build.log.') }

        $output = Join-Path $source 'dist\windows\Tux-sstv'
        if (-not (Test-Path -LiteralPath (Join-Path $output 'tux-sstv.exe'))) { throw 'The build exited without producing the expected Windows executable.' }
        $outputDirectory = Join-Path $source 'dist\windows'
        if (-not @(Get-ChildItem -LiteralPath $outputDirectory -Filter 'Tux-sstv-*-windows-x64-setup.exe' -File).Count) {
            throw 'The program built, but the Windows installer was not produced. Run Make Windows Installer.cmd to retry without recompiling.'
        }
        Write-Host ''
        Write-Host 'BUILD SUCCEEDED.' -ForegroundColor Green
        Write-Host ('Program: ' + (Join-Path $output 'tux-sstv.exe'))
        Write-Host ('Installer and portable ZIP: ' + $outputDirectory)
        Write-Host 'Run the setup EXE to install Tux-sstv and its Start menu icon.'
        if (-not $KeepFolderClosed) {
            try { Start-Process -FilePath 'explorer.exe' -ArgumentList ('"' + $outputDirectory + '"') | Out-Null }
            catch { Write-Host ('Open this folder to find the installer: ' + $outputDirectory) }
        }
        return 0
    } catch {
        Write-Host ''
        Write-Host ('BUILD STOPPED: ' + $_.Exception.Message) -ForegroundColor Red
        Write-Host ('Send the last error lines from this window or the logs in ' + $logDirectory + ' back to this chat.')
        Write-Host 'You can run the script again after fixing the reported error; verified downloads and completed tools are reused.'
        return 1
    } finally {
        if ($lockHeld -and $null -ne $mutex) { $mutex.ReleaseMutex() }
        if ($null -ne $mutex) { $mutex.Dispose() }
        if ($transcriptStarted) { Stop-Transcript | Out-Null }
    }
}

# Dot-sourcing imports the functions for the Windows-side controller tests.
if ($MyInvocation.InvocationName -ne '.') {
    exit (Invoke-TuxAutomaticBuild -RequestedToolsDirectory $ToolsDirectory -KeepFolderClosed:$NoOpenFolder)
}
