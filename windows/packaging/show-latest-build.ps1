#requires -Version 5.1
$ErrorActionPreference = 'Stop'
$helperRoot = Split-Path -Parent $PSScriptRoot
$reportPath = Join-Path $helperRoot 'Tux-sstv-Windows-error.txt'
$report = [Collections.Generic.List[string]]::new()

function Find-TuxBuild {
    param([string]$Folder, [int]$Depth = 0)
    if (-not (Test-Path -LiteralPath $Folder -PathType Container)) { return }
    $logDirectory = Join-Path $Folder 'build-windows-logs'
    if (Test-Path -LiteralPath $logDirectory -PathType Container) {
        $logs = @(Get-ChildItem -LiteralPath $logDirectory -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -in @('build.log','tests.log','automatic-build.log','bootstrap.log') })
        if ($logs.Count) {
            $newest = $logs | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
            [pscustomobject]@{ Source = $Folder; Updated = $newest.LastWriteTimeUtc }
        }
        return
    }
    if ($Depth -ge 3) { return }
    foreach ($child in @(Get-ChildItem -LiteralPath $Folder -Directory -ErrorAction SilentlyContinue)) {
        if ($child.Name -like 'Tux*' -and -not ($child.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            Find-TuxBuild -Folder $child.FullName -Depth ($Depth + 1)
        }
    }
}

function Add-TuxLog {
    param([string]$Path, [switch]$TailOnly)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return }
    $file = Get-Item -LiteralPath $Path
    $report.Add('')
    $report.Add('LOG: ' + $file.FullName)
    $report.Add('Updated: ' + $file.LastWriteTime.ToString('yyyy-MM-dd HH:mm:ss'))
    if (-not $TailOnly) {
        $pattern = 'FAIL!|QFATAL|FATAL|CMake Error|error:|ninja:.*stopped|BUILD STOPPED|Cannot load|Could not load|Unresolved runtime|Exception|Subprocess aborted'
        $failureMatches = @(Select-String -LiteralPath $Path -Pattern $pattern -Encoding UTF8 -Context 2,7 |
            Select-Object -Last 10)
        foreach ($match in $failureMatches) {
            $report.Add('Failure context near line ' + $match.LineNumber + ':')
            foreach ($line in $match.Context.PreContext) { $report.Add([string]$line) }
            $report.Add($match.Line)
            foreach ($line in $match.Context.PostContext) { $report.Add([string]$line) }
        }
    }
    $report.Add('Last 45 lines:')
    foreach ($line in @(Get-Content -LiteralPath $Path -Encoding UTF8 -Tail 45)) { $report.Add([string]$line) }
}

try {
    $report.Add('Tux-sstv Windows - newest build report (collector revision 2)')
    $report.Add('Report generated: ' + (Get-Date).ToString('yyyy-MM-dd HH:mm:ss'))
    $roots = @($helperRoot, [Environment]::GetFolderPath('Desktop'),
        (Join-Path $env:USERPROFILE 'Desktop'), (Join-Path $env:USERPROFILE 'Downloads'))
    $parent = Split-Path -Parent $helperRoot
    if ($parent) { $roots += $parent }
    $found = @(foreach ($root in @($roots | Select-Object -Unique)) { Find-TuxBuild -Folder $root })
    $builds = @($found | Group-Object Source | ForEach-Object { $_.Group[0] } |
        Sort-Object Updated -Descending)
    if (-not $builds.Count) {
        $report.Add('No build logs found under the usual Tux-sstv folders.')
        $report.Add('Copy this entire helper folder inside your extracted Tux-sstv-Windows source folder and run it again.')
    } else {
        $latest = $builds[0]
        $report.Add('SELECTED NEWEST BUILD: ' + $latest.Source)
        $report.Add('Newest log update: ' + $latest.Updated.ToLocalTime().ToString('yyyy-MM-dd HH:mm:ss'))
        $revision = Join-Path $latest.Source 'WINDOWS-KIT-REVISION.txt'
        if (Test-Path -LiteralPath $revision -PathType Leaf) {
            foreach ($line in @(Get-Content -LiteralPath $revision -Encoding UTF8 -TotalCount 2)) { $report.Add([string]$line) }
        }
        $font = Join-Path $latest.Source 'resources\fonts\NotoColorEmoji.ttf'
        if (Test-Path -LiteralPath $font -PathType Leaf) {
            $report.Add('Bundled emoji font: PRESENT')
            $report.Add('Font SHA-256: ' + (Get-FileHash -LiteralPath $font -Algorithm SHA256).Hash)
        } else { $report.Add('Bundled emoji font: ABSENT (this source folder predates the font repair)') }
        $report.Add('Other detected builds (their failures are not merged into this report):')
        foreach ($build in $builds) { $report.Add($build.Updated.ToLocalTime().ToString('yyyy-MM-dd HH:mm:ss') + '  ' + $build.Source) }
        Add-TuxLog -Path (Join-Path $latest.Source 'build-windows-logs\build.log')
        Add-TuxLog -Path (Join-Path $latest.Source 'build-windows-logs\tests.log')
        Add-TuxLog -Path (Join-Path $latest.Source 'build-windows\Testing\Temporary\LastTest.log')
        Add-TuxLog -Path (Join-Path $latest.Source 'build-windows-logs\automatic-build.log') -TailOnly
        Add-TuxLog -Path (Join-Path $latest.Source 'build-windows-logs\bootstrap.log') -TailOnly
    }
} catch {
    $report.Add('Collector error: ' + $_.Exception.Message)
}

$report | Set-Content -LiteralPath $reportPath -Encoding UTF8
Write-Host ($report -join [Environment]::NewLine)
Write-Host ''
Write-Host ('Saved report: ' + $reportPath)
Write-Host 'Copy this newly generated text file back to your USB drive.'
Start-Process -FilePath (Join-Path $env:WINDIR 'System32\notepad.exe') -ArgumentList ('"' + $reportPath + '"')
