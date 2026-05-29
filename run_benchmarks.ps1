param(
    [int]$Iterations = 25,
    [int]$WarmupIterations = 3,
    [double]$TrimFraction = 0.1
)

$ErrorActionPreference = 'Stop'

$benchmarks = @(
    @{ Name = 'benchmark_math';   Opus = 'tests\benchmark_math.op';   Msvc = 'benchmarks\msvc\benchmark_math.vcxproj';   MsvcExe = 'benchmarks\msvc\x64\Release\BenchmarkMathMSVC.exe';   OpusExe = 'tests\benchmark_math.exe' },
    @{ Name = 'benchmark_state';  Opus = 'tests\benchmark_state.op';  Msvc = 'benchmarks\msvc\benchmark_state.vcxproj';  MsvcExe = 'benchmarks\msvc\x64\Release\BenchmarkStateMSVC.exe';  OpusExe = 'tests\benchmark_state.exe' },
    @{ Name = 'benchmark_branch'; Opus = 'tests\benchmark_branch.op'; Msvc = 'benchmarks\msvc\benchmark_branch.vcxproj'; MsvcExe = 'benchmarks\msvc\x64\Release\BenchmarkBranchMSVC.exe'; OpusExe = 'tests\benchmark_branch.exe' },
    @{ Name = 'benchmark_general'; Opus = 'tests\benchmark_general.op'; Msvc = 'benchmarks\msvc\benchmark_general.vcxproj'; MsvcExe = 'x64\Release\BenchmarkGeneralMSVC.exe'; OpusExe = 'tests\benchmark_general.exe' },
    @{ Name = 'benchmark_text'; Opus = 'tests\benchmark_text.op'; Msvc = 'benchmarks\msvc\benchmark_text.vcxproj'; MsvcExe = 'x64\Release\BenchmarkTextMSVC.exe'; OpusExe = 'tests\benchmark_text.exe' },
    @{ Name = 'benchmark_hft'; Opus = 'tests\benchmark_hft.op'; Msvc = 'benchmarks\msvc\benchmark_hft.vcxproj'; MsvcExe = 'benchmarks\msvc\x64\Release\BenchmarkHftMSVC.exe'; OpusExe = 'tests\benchmark_hft.exe' }
)

function Get-MsBuildPath {
    $candidates = @(
        'C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\amd64\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\amd64\MSBuild.exe'
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) { return $candidate }
    }

    $found = Get-Command MSBuild.exe -ErrorAction SilentlyContinue
    if ($found) { return $found.Source }

    throw 'MSBuild.exe not found'
}

function Get-Percentile([double[]]$Sorted, [double]$Percentile) {
    if ($Sorted.Count -eq 0) {
        return 0.0
    }

    $index = [Math]::Ceiling(($Sorted.Count - 1) * $Percentile)
    $index = [Math]::Max(0, [Math]::Min($Sorted.Count - 1, [int]$index))
    return [math]::Round($Sorted[$index], 3)
}

function Get-TrimmedAverage([double[]]$Sorted, [double]$Fraction) {
    if ($Sorted.Count -eq 0) {
        return 0.0
    }

    $trimCount = [Math]::Ceiling($Sorted.Count * $Fraction)
    if (($trimCount * 2) -ge $Sorted.Count) {
        $trimCount = 0
    }

    if ($trimCount -gt 0) {
        $slice = @($Sorted[$trimCount..($Sorted.Count - $trimCount - 1)])
    } else {
        $slice = @($Sorted)
    }

    return [math]::Round((($slice | Measure-Object -Average).Average), 3)
}

function Get-StdDev([double[]]$Values) {
    if ($Values.Count -le 1) {
        return 0.0
    }

    $mean = (($Values | Measure-Object -Average).Average)
    $sumSq = 0.0
    foreach ($value in $Values) {
        $delta = $value - $mean
        $sumSq += $delta * $delta
    }

    return [math]::Round([Math]::Sqrt($sumSq / ($Values.Count - 1)), 3)
}

function Measure-Exe([string]$Path, [int]$WarmupCount, [int]$Count, [double]$TrimFraction) {
    $times = @()
    $exitCodes = @()

    for ($i = 0; $i -lt $WarmupCount; $i++) {
        & $Path | Out-Null
        $exitCodes += $LASTEXITCODE
    }

    for ($i = 0; $i -lt $Count; $i++) {
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        & $Path | Out-Null
        $sw.Stop()
        $times += $sw.Elapsed.TotalMilliseconds
        $exitCodes += $LASTEXITCODE
    }

    $sorted = @($times | Sort-Object)

    return [PSCustomObject]@{
        Samples = $Count
        WarmupSamples = $WarmupCount
        AvgMs = [math]::Round((($times | Measure-Object -Average).Average), 3)
        TrimmedAvgMs = Get-TrimmedAverage $sorted $TrimFraction
        MedianMs = [math]::Round($sorted[[int]($sorted.Count / 2)], 3)
        MinMs = [math]::Round((($times | Measure-Object -Minimum).Minimum), 3)
        MaxMs = [math]::Round((($times | Measure-Object -Maximum).Maximum), 3)
        P90Ms = Get-Percentile $sorted 0.90
        P95Ms = Get-Percentile $sorted 0.95
        StdDevMs = Get-StdDev $times
        ExitCodes = (($exitCodes | Select-Object -Unique) -join ',')
    }
}

$msbuild = Get-MsBuildPath
$results = @()

Write-Host '=== Opus Benchmark Suite ===' -ForegroundColor Cyan
Write-Host "Iterations per runtime sample: $Iterations" -ForegroundColor Cyan
Write-Host "Warmup samples per executable: $WarmupIterations" -ForegroundColor Cyan
Write-Host ''

foreach ($bench in $benchmarks) {
    Write-Host "Benchmark: $($bench.Name)" -ForegroundColor Yellow

    $opusCompile = (Measure-Command { & '.\bin\Release\opus.exe' '--quiet' '--fast' $bench.Opus | Out-Null }).TotalMilliseconds
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $bench.OpusExe)) {
        throw "Opus compile failed for $($bench.Name)"
    }
    $opusRun = Measure-Exe $bench.OpusExe $WarmupIterations $Iterations $TrimFraction
    $opusSize = (Get-Item $bench.OpusExe).Length
    Write-Host ("  Opus  compile={0}ms median={1}ms trim={2}ms stddev={3}ms size={4}" -f ([math]::Round($opusCompile, 3)), $opusRun.MedianMs, $opusRun.TrimmedAvgMs, $opusRun.StdDevMs, $opusSize)
    $results += [PSCustomObject]@{
        Benchmark = $bench.Name
        Toolchain = 'Opus'
        CompileMs = [math]::Round($opusCompile, 3)
        Samples = $opusRun.Samples
        WarmupSamples = $opusRun.WarmupSamples
        AvgMs = $opusRun.AvgMs
        TrimmedAvgMs = $opusRun.TrimmedAvgMs
        MedianMs = $opusRun.MedianMs
        MinMs = $opusRun.MinMs
        MaxMs = $opusRun.MaxMs
        P90Ms = $opusRun.P90Ms
        P95Ms = $opusRun.P95Ms
        StdDevMs = $opusRun.StdDevMs
        SizeBytes = $opusSize
        ExitCodes = $opusRun.ExitCodes
    }

    $msvcCompile = (Measure-Command { & $msbuild $bench.Msvc /t:Rebuild /p:Configuration=Release /p:Platform=x64 /m | Out-Null }).TotalMilliseconds
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $bench.MsvcExe)) {
        throw "MSVC build failed for $($bench.Name)"
    }
    $msvcRun = Measure-Exe $bench.MsvcExe $WarmupIterations $Iterations $TrimFraction
    $msvcSize = (Get-Item $bench.MsvcExe).Length
    Write-Host ("  MSVC  compile={0}ms median={1}ms trim={2}ms stddev={3}ms size={4}" -f ([math]::Round($msvcCompile, 3)), $msvcRun.MedianMs, $msvcRun.TrimmedAvgMs, $msvcRun.StdDevMs, $msvcSize)
    Write-Host ''
    $results += [PSCustomObject]@{
        Benchmark = $bench.Name
        Toolchain = 'MSVC'
        CompileMs = [math]::Round($msvcCompile, 3)
        Samples = $msvcRun.Samples
        WarmupSamples = $msvcRun.WarmupSamples
        AvgMs = $msvcRun.AvgMs
        TrimmedAvgMs = $msvcRun.TrimmedAvgMs
        MedianMs = $msvcRun.MedianMs
        MinMs = $msvcRun.MinMs
        MaxMs = $msvcRun.MaxMs
        P90Ms = $msvcRun.P90Ms
        P95Ms = $msvcRun.P95Ms
        StdDevMs = $msvcRun.StdDevMs
        SizeBytes = $msvcSize
        ExitCodes = $msvcRun.ExitCodes
    }
}

Write-Host '=== Benchmark Results ===' -ForegroundColor Cyan
Write-Host ''
$results | Sort-Object Benchmark, Toolchain | Format-Table -AutoSize
$results | Export-Csv -Path 'benchmark_results.csv' -NoTypeInformation
Write-Host ''
Write-Host 'Results exported to benchmark_results.csv' -ForegroundColor Gray
