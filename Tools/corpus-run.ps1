param(
    [string]$Manifest = "Tools/corpus/manifest.json",
    [string]$OutRoot = "Runs/Corpus",
    [switch]$NoBuild,
    [switch]$NoEncode
)

$ErrorActionPreference = "Stop"

function Resolve-RepoPath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }

    return Join-Path $script:RepoRoot $Path
}

function ConvertTo-SafeName {
    param([string]$Name)

    return ($Name -replace '[^A-Za-z0-9_.-]+', '_').Trim('_')
}

function ConvertTo-CommandLine {
    param([string[]]$CommandArgs)

    $quoted = foreach ($arg in $CommandArgs) {
        if ($arg -match '[\s"]') {
            '"' + ($arg -replace '"', '\"') + '"'
        }
        else {
            $arg
        }
    }

    return ($quoted -join " ")
}

function Read-RecordingMeta {
    param([string]$Path)

    $meta = @{}
    if (!(Test-Path $Path)) {
        return $meta
    }

    foreach ($line in Get-Content -Path $Path) {
        if ($line -match '^([^=]+)=(.*)$') {
            $meta[$matches[1]] = $matches[2]
        }
    }

    return $meta
}

function Get-ScenarioName {
    param($Scenario)

    if ($Scenario -is [string]) {
        return $Scenario
    }

    if ($Scenario.name) {
        return [string]$Scenario.name
    }

    throw "Scenario is missing a name"
}

function New-GeneratedInputScript {
    param(
        $Scenario,
        [string]$RecordDir
    )

    if ($Scenario -is [string]) {
        return $null
    }

    if (!$Scenario.input -or $Scenario.input.Count -eq 0) {
        return $null
    }

    $scenarioName = Get-ScenarioName $Scenario
    $path = Join-Path $RecordDir "$scenarioName.generated.mvm-input"
    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add("# Generated from corpus manifest scenario: $scenarioName")

    foreach ($step in $Scenario.input) {
        if ($null -ne $step.waitMs) {
            $lines.Add("WAIT $([uint32]$step.waitMs)")
        }
        elseif ($step.press) {
            $durationMs = if ($null -ne $step.durationMs) { [uint32]$step.durationMs } else { 100 }
            $lines.Add("PRESS $($step.press) $durationMs")
        }
        elseif ($step.down) {
            $lines.Add("DOWN $($step.down)")
        }
        elseif ($step.up) {
            $lines.Add("UP $($step.up)")
        }
        else {
            throw "Unsupported input step in scenario '$scenarioName': $($step | ConvertTo-Json -Compress)"
        }
    }

    Set-Content -Path $path -Value $lines -Encoding ASCII
    return $path
}

$script:RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $script:RepoRoot

$manifestPath = Resolve-RepoPath $Manifest
$manifestData = Get-Content -Raw -Path $manifestPath | ConvertFrom-Json

if (!$NoBuild) {
    & (Resolve-RepoPath "build.bat")
    if ($LASTEXITCODE -ne 0) {
        throw "build.bat failed with exit code $LASTEXITCODE"
    }
}

$appName = Split-Path -Leaf $script:RepoRoot
$exePath = Resolve-RepoPath "$appName.exe"
if (!(Test-Path $exePath)) {
    throw "Executable not found: $exePath"
}

$sdlBin = Resolve-RepoPath "SDL2-2.32.6/x86_64-w64-mingw32/bin"
$batchId = Get-Date -Format "yyyyMMdd_HHmmss"
$batchRoot = Resolve-RepoPath (Join-Path $OutRoot $batchId)
$logsRoot = Join-Path $batchRoot "logs"
$recordsRoot = Join-Path $batchRoot "recordings"
$videosRoot = Join-Path $batchRoot "videos"
New-Item -ItemType Directory -Force -Path $logsRoot, $recordsRoot, $videosRoot | Out-Null
Copy-Item -Path $manifestPath -Destination (Join-Path $batchRoot "manifest.resolved.json") -Force

$summary = New-Object System.Collections.Generic.List[object]

foreach ($run in $manifestData.runs) {
    $gamePath = Resolve-RepoPath $run.game
    $durationMs = if ($run.durationMs) { [uint32]$run.durationMs } else { [uint32]$manifestData.defaultDurationMs }
    $maxSteps = if ($run.maxSteps) { [uint32]$run.maxSteps } else { [uint32]$manifestData.defaultMaxSteps }
    $maxLoggedCalls = if ($run.maxLoggedCalls) { [uint32]$run.maxLoggedCalls } else { [uint32]$manifestData.defaultMaxLoggedCalls }

    foreach ($profile in $run.profiles) {
        foreach ($scenario in $run.scenarios) {
            $scenarioName = Get-ScenarioName $scenario
            $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
            $gameName = ConvertTo-SafeName ([System.IO.Path]::GetFileNameWithoutExtension($gamePath))
            $runId = ConvertTo-SafeName "$stamp`__$profile`__$scenarioName`__$gameName"
            $logPath = Join-Path $logsRoot "$runId.log"
            $recordDir = Join-Path $recordsRoot $runId
            $videoPath = Join-Path $videosRoot "$runId.mp4"
            New-Item -ItemType Directory -Force -Path $recordDir | Out-Null

            $scenarioPath = New-GeneratedInputScript -Scenario $scenario -RecordDir $recordDir

            $args = @(
                $gamePath,
                [string]$profile,
                [string]$maxSteps,
                [string]$maxLoggedCalls,
                "--duration-ms",
                [string]$durationMs,
                "--record-dir",
                $recordDir
            )
            if ($scenarioPath) {
                $args += @("--input-script", $scenarioPath)
            }

            $startTime = Get-Date
            $timedOut = $false
            $exitCode = $null
            $header = @(
                "corpus.run_id=$runId",
                "corpus.game=$gamePath",
                "corpus.profile=$profile",
                "corpus.scenario=$scenarioName",
                "corpus.duration_ms=$durationMs",
                "corpus.started=$($startTime.ToString('o'))",
                ""
            )
            Set-Content -Path $logPath -Value $header -Encoding UTF8

            try {
                $psi = [System.Diagnostics.ProcessStartInfo]::new()
                $psi.FileName = $exePath
                $psi.Arguments = ConvertTo-CommandLine -CommandArgs $args
                $psi.WorkingDirectory = $script:RepoRoot
                $psi.UseShellExecute = $false
                $psi.RedirectStandardOutput = $true
                $psi.RedirectStandardError = $true
                $psi.EnvironmentVariables["PATH"] = "$sdlBin;$($psi.EnvironmentVariables["PATH"])"

                $process = [System.Diagnostics.Process]::new()
                $process.StartInfo = $psi
                [void]$process.Start()
                $stdoutTask = $process.StandardOutput.ReadToEndAsync()
                $stderrTask = $process.StandardError.ReadToEndAsync()

                $timeoutMs = [int]($durationMs + 15000)
                if (!$process.WaitForExit($timeoutMs)) {
                    $timedOut = $true
                    $process.Kill()
                    $process.WaitForExit()
                }
                else {
                    $process.WaitForExit()
                }
                $exitCode = $process.ExitCode
                Add-Content -Path $logPath -Value $stdoutTask.Result -Encoding UTF8
                Add-Content -Path $logPath -Value $stderrTask.Result -Encoding UTF8
            }
            finally {
                $endTime = Get-Date
                Add-Content -Path $logPath -Value @(
                    "",
                    "corpus.ended=$($endTime.ToString('o'))",
                    "corpus.exit_code=$exitCode",
                    "corpus.timed_out=$timedOut"
                ) -Encoding UTF8
            }

            $recordMetaPath = Join-Path $recordDir "recording.txt"
            $recordMeta = Read-RecordingMeta $recordMetaPath
            $encoded = $false
            if (!$NoEncode -and (Get-Command ffmpeg -ErrorAction SilentlyContinue) -and $recordMeta.ContainsKey("frames")) {
                $framesPath = Join-Path $recordDir "frames.rgba"
                $audioPath = Join-Path $recordDir "audio.wav"
                if ((Test-Path $framesPath) -and [int]$recordMeta["frames"] -gt 0) {
                    if ((Test-Path $audioPath) -and $recordMeta.ContainsKey("audio_bytes") -and [int]$recordMeta["audio_bytes"] -gt 0) {
                        & ffmpeg -y -hide_banner -loglevel error `
                            -f rawvideo -pixel_format rgba `
                            -video_size "$($recordMeta["width"])x$($recordMeta["height"])" `
                            -framerate $recordMeta["fps"] `
                            -i $framesPath `
                            -i $audioPath `
                            -vf "pad=ceil(iw/2)*2:ceil(ih/2)*2" `
                            -pix_fmt yuv420p $videoPath
                    }
                    else {
                        & ffmpeg -y -hide_banner -loglevel error `
                            -f rawvideo -pixel_format rgba `
                            -video_size "$($recordMeta["width"])x$($recordMeta["height"])" `
                            -framerate $recordMeta["fps"] `
                            -i $framesPath `
                            -vf "pad=ceil(iw/2)*2:ceil(ih/2)*2" `
                            -pix_fmt yuv420p $videoPath
                    }
                    $encoded = ($LASTEXITCODE -eq 0 -and (Test-Path $videoPath))
                }
            }

            $summary.Add([pscustomobject]@{
                run_id = $runId
                game = $gamePath
                profile = $profile
                scenario = $scenarioName
                duration_ms = $durationMs
                exit_code = $exitCode
                timed_out = $timedOut
                log = $logPath
                recording_dir = $recordDir
                video = if ($encoded) { $videoPath } else { "" }
            })
        }
    }
}

$summaryPath = Join-Path $batchRoot "summary.csv"
$summary | Export-Csv -Path $summaryPath -NoTypeInformation -Encoding UTF8
Write-Host "Corpus batch complete: $batchRoot"
Write-Host "Summary: $summaryPath"
