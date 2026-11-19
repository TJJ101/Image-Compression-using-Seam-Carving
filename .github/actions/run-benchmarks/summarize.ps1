<#
.SYNOPSIS
  Turns a Google Benchmark JSON report from seamcarve_bench into Markdown tables that put the seam-carving
  implementations side by side, and appends them to the GitHub run summary.

.DESCRIPTION
  Benchmark names follow bench/bench_seams.cpp:
    FindSeam/<impl>/<image>/<width>, Carve/<impl>/<image>/<width>, ComputeEnergy/<image>/<width>
  and an implementation that is not available is a single FindSeam/<impl> or Carve/<impl> entry that was skipped.

  Tables written:
    - At a glance: one row per implementation (how often it is fastest, median slowdown, median extra seam energy)
    - FindSeam speed: one row per image and width, one column per implementation, relative to the fastest
    - FindSeam seam energy: the same layout, relative to DP (which finds the optimal seam)
    - Carve speed, fitted complexity, and (collapsed) ComputeEnergy and every raw result
  Times are wall-clock means over the repetitions: CPU time on Windows only advances in ~15.6 ms ticks.

  Throws after writing the summary if any benchmark failed with an error.

  Kept ASCII-only (special characters are made with [char]) because Windows PowerShell 5.1 reads a .ps1 without a
  BOM in the ANSI code page.

.EXAMPLE
  build\seamcarve_bench.exe --benchmark_out=bench.json --benchmark_out_format=json --benchmark_repetitions=3
  .github\actions\run-benchmarks\summarize.ps1 -Json bench.json -SummaryPath bench.md
#>
param(
    [Parameter(Mandatory = $true)][string]$Json,
    [int]$Repetitions = 1,
    # Where the Markdown goes; defaults to the GitHub run summary. Empty: only print it.
    [string]$SummaryPath = $env:GITHUB_STEP_SUMMARY,
    # Implementation the seam energy is compared against (it finds the optimal seam)
    [string]$Baseline = 'dp'
)

$ErrorActionPreference = 'Stop'
$Mu = [char]0x00B5     # micro sign
$Times = [char]0x00D7  # multiplication sign
# Two seam energies are the same seam cost if they differ by less than this fraction (floating-point sums)
$EnergyTolerance = 1e-9

# ---- Formatting -----------------------------------------------------------------------------------------------
function Format-Number([double]$Value) {
    $Abs = [math]::Abs($Value)
    if ($Abs -ge 100) { return '{0:N0}' -f $Value }
    if ($Abs -ge 10) { return '{0:N1}' -f $Value }
    return '{0:N2}' -f $Value
}

function Format-Time([double]$Ns) {
    if ($Ns -lt 1e3) { return "$(Format-Number $Ns) ns" }
    if ($Ns -lt 1e6) { return "$(Format-Number ($Ns / 1e3)) $($Mu)s" }
    if ($Ns -lt 1e9) { return "$(Format-Number ($Ns / 1e6)) ms" }
    return "$(Format-Number ($Ns / 1e9)) s"
}

function Format-Ratio([double]$Ratio) {
    if ($Ratio -lt 10) { return ('{0:N1}' -f $Ratio) + $Times }
    return ('{0:N0}' -f $Ratio) + $Times
}

function Format-Percent([double]$Fraction) {
    $Pct = $Fraction * 100
    $Sign = if ($Pct -gt 0) { '+' } else { '' }
    if ([math]::Abs($Pct) -ge 10) { return $Sign + ('{0:N0}' -f $Pct) + '%' }
    return $Sign + ('{0:N1}' -f $Pct) + '%'
}

# A seam energy difference from the baseline, or "optimal" when there is none
function Format-Overhead([double]$Fraction) {
    if ([math]::Abs($Fraction) -le $EnergyTolerance) { return 'optimal' }
    return Format-Percent $Fraction
}

function Get-Median([double[]]$Values) {
    if ($Values.Count -eq 0) { return $null }
    $Sorted = @($Values | Sort-Object)
    $Mid = [math]::Floor($Sorted.Count / 2)
    if ($Sorted.Count % 2 -eq 1) { return $Sorted[$Mid] }
    return ($Sorted[$Mid - 1] + $Sorted[$Mid]) / 2
}

$UnitToNs = @{ ns = 1.0; us = 1e3; ms = 1e6; s = 1e9 }

# ---- Read the report ------------------------------------------------------------------------------------------
$Report = Get-Content $Json -Raw | ConvertFrom-Json

$Results = [ordered]@{}     # run_name -> result; the mean over the repetitions, else the single run
$Unavailable = @()          # implementations (or images) that were skipped as a whole
$BigO = @()
$ImplOrder = New-Object System.Collections.Generic.List[string]
$ImageOrder = New-Object System.Collections.Generic.List[string]

foreach ($B in $Report.benchmarks) {
    $Parts = $B.run_name -split '/'
    $Kind = $Parts[0]
    if ($Kind -eq 'ComputeEnergy') {
        $Impl = ''; $Image = $Parts[1]; $Width = $Parts[2]
    } else {
        $Impl = $Parts[1]; $Image = $Parts[2]; $Width = $Parts[3]
    }
    if ($Impl -and ($Kind -eq 'FindSeam' -or $Kind -eq 'Carve') -and -not $ImplOrder.Contains($Impl)) { $ImplOrder.Add($Impl) }
    if ($Image -and -not $ImageOrder.Contains($Image)) { $ImageOrder.Add($Image) }

    if ($B.aggregate_name -eq 'BigO') {
        $BigO += [pscustomobject]@{ Kind = $Kind; Impl = $Impl; Image = $Image; Complexity = 'O(' + $B.big_o.Trim('()') + ')' }
        continue
    }
    if ($B.run_type -eq 'aggregate' -and $B.aggregate_name -ne 'mean') { continue }
    $Key = $B.run_name
    $Existing = $Results[$Key]
    # A mean replaces the single repetitions; otherwise keep the first entry
    if ($Existing -and -not ($B.run_type -eq 'aggregate' -and $Existing.RunType -ne 'aggregate')) { continue }

    $Status = if ($B.error_occurred) { 'error' } elseif ($B.skipped) { 'skipped' } else { 'ok' }
    $Message = if ($B.error_occurred) { $B.error_message } elseif ($B.skipped) { $B.skip_message } else { $B.label }
    $Energy = if ($B.PSObject.Properties['seam_energy']) { [double]$B.seam_energy } else { $null }
    $Ns = if ($Status -eq 'ok') { [double]$B.real_time * $UnitToNs[$B.time_unit] } else { $null }
    $Result = [pscustomobject]@{
        Name = $Key; RunType = $B.run_type; Kind = $Kind; Impl = $Impl; Image = $Image
        Width = if ($Width) { [int]$Width } else { $null }
        Status = $Status; Message = $Message; Ns = $Ns; Cpu = $B.cpu_time; Unit = $B.time_unit; SeamEnergy = $Energy
    }
    $Results[$Key] = $Result
    if (-not $Image -and $Status -ne 'ok') { $Unavailable += $Result }
}

$All = @($Results.Values)
$Errors = @($All | Where-Object { $_.Status -eq 'error' })

# Benchmarks that measured one implementation on one image at one width, grouped as $Cells[kind][image/width][impl]
function Get-Cases([string]$Kind) {
    $Measured = @($All | Where-Object { $_.Kind -eq $Kind -and $_.Image -and $_.Width })
    $Cases = [ordered]@{}
    foreach ($Image in $ImageOrder) {
        $Widths = @($Measured | Where-Object { $_.Image -eq $Image } | ForEach-Object { $_.Width } | Sort-Object -Unique)
        foreach ($Width in $Widths) {
            $Cells = @{}
            foreach ($R in @($Measured | Where-Object { $_.Image -eq $Image -and $_.Width -eq $Width })) { $Cells[$R.Impl] = $R }
            $Cases["$Image/$Width"] = [pscustomobject]@{ Image = $Image; Width = $Width; Cells = $Cells }
        }
    }
    return $Cases
}

# Implementations with at least one entry for this kind (measured or skipped as a whole), in registration order
function Get-Impls([string]$Kind) {
    return @($ImplOrder | Where-Object { $Impl = $_; @($All | Where-Object { $_.Kind -eq $Kind -and $_.Impl -eq $Impl }).Count -gt 0 })
}

function Get-UnavailableNote([string]$Kind) {
    $Items = @($Unavailable | Where-Object { $_.Kind -eq $Kind -and $_.Impl } | ForEach-Object { "**$($_.Impl)** ($($_.Status): $($_.Message))" })
    if ($Items.Count -eq 0) { return @() }
    return @('', "Not run: $($Items -join ', ').")
}

# Speed table: fastest implementation in bold, the others as a multiple of it
function Get-SpeedTable([string]$Kind, $Cases, [string[]]$Impls) {
    $Lines = @("| Image | Width | $($Impls -join ' | ') |", "|---|---:|$(($Impls | ForEach-Object { '---:' }) -join '|')|")
    foreach ($Case in $Cases.Values) {
        $Ok = @($Case.Cells.Values | Where-Object { $_.Status -eq 'ok' })
        $Fastest = if ($Ok.Count -gt 0) { ($Ok | Measure-Object -Property Ns -Minimum).Minimum } else { $null }
        $Row = foreach ($Impl in $Impls) {
            $R = $Case.Cells[$Impl]
            if (-not $R) { '-' }
            elseif ($R.Status -ne 'ok') { $R.Status }
            elseif ($Ok.Count -gt 1 -and $R.Ns -eq $Fastest) { "**$(Format-Time $R.Ns)**" }
            elseif ($Ok.Count -gt 1) { "$(Format-Time $R.Ns) ($(Format-Ratio ($R.Ns / $Fastest)))" }
            else { Format-Time $R.Ns }
        }
        $Lines += "| $($Case.Image) | $($Case.Width) px | $($Row -join ' | ') |"
    }
    return $Lines
}

# ---- Build the summary ----------------------------------------------------------------------------------------
$Summary = @('### SeamCarve benchmarks', '')
$Summary += "Wall-time mean over $Repetitions repetition(s) on a $($Report.context.num_cpus)-CPU machine. Compare implementations within one run; shared runners are too noisy to compare runs."
if (@($All | Where-Object { $_.Message -eq 'grayscale energy' }).Count -gt 0) {
    $Summary += '', 'FindSeam used the grayscale intensity as the energy map (ComputeEnergy is not implemented yet).'
}

if ($Errors.Count -gt 0) {
    $Summary += '', "#### $([char]0x274C) $($Errors.Count) benchmark(s) failed", '', '| Benchmark | Error |', '|---|---|'
    $Summary += @($Errors | ForEach-Object { "| $($_.Name) | $($_.Message) |" })
}

$FindCases = Get-Cases 'FindSeam'
$FindImpls = Get-Impls 'FindSeam'
$HasBaseline = @($All | Where-Object { $_.Kind -eq 'FindSeam' -and $_.Impl -eq $Baseline -and $_.Status -eq 'ok' }).Count -gt 0

# At a glance: one row per implementation over all the FindSeam cases it ran
if ($FindCases.Count -gt 0) {
    $Summary += '', '#### At a glance (FindSeam)', ''
    $Header = '| Algorithm | Cases | Widths | Fastest in | Median time vs fastest |'
    $Align = '|---|---:|---|---:|---:|'
    if ($HasBaseline) {
        $Header += " Optimal seams | Median seam energy vs $Baseline | Worst |"
        $Align += '---:|---:|---:|'
    }
    $Summary += $Header, $Align
    foreach ($Impl in $FindImpls) {
        $Mine = @($FindCases.Values | Where-Object { $_.Cells[$Impl] -and $_.Cells[$Impl].Status -eq 'ok' })
        if ($Mine.Count -eq 0) {
            $Status = @($All | Where-Object { $_.Kind -eq 'FindSeam' -and $_.Impl -eq $Impl } | Select-Object -First 1).Status
            $Row = "| $Impl | 0 | - | - | $Status |"
            if ($HasBaseline) { $Row += ' - | - | - |' }
            $Summary += $Row
            continue
        }
        $Wins = 0
        $Slowdowns = @()
        $Overheads = @()
        $Optimal = 0
        foreach ($Case in $Mine) {
            $Ok = @($Case.Cells.Values | Where-Object { $_.Status -eq 'ok' })
            $Fastest = ($Ok | Measure-Object -Property Ns -Minimum).Minimum
            if ($Case.Cells[$Impl].Ns -eq $Fastest) { $Wins++ }
            $Slowdowns += $Case.Cells[$Impl].Ns / $Fastest
            $Base = $Case.Cells[$Baseline]
            if ($HasBaseline -and $Base -and $Base.Status -eq 'ok' -and $null -ne $Base.SeamEnergy -and $Base.SeamEnergy -ne 0) {
                $Overhead = ($Case.Cells[$Impl].SeamEnergy - $Base.SeamEnergy) / $Base.SeamEnergy
                $Overheads += $Overhead
                if ([math]::Abs($Overhead) -le $EnergyTolerance) { $Optimal++ }
            }
        }
        $Widths = @($Mine | ForEach-Object { $_.Width } | Sort-Object -Unique)
        $WidthText = if ($Widths.Count -gt 1) { "$($Widths[0])-$($Widths[-1]) px" } else { "$($Widths[0]) px" }
        $Row = "| $Impl | $($Mine.Count) | $WidthText | $Wins | $(Format-Ratio (Get-Median $Slowdowns)) |"
        if ($HasBaseline) {
            if ($Overheads.Count -eq 0) { $Row += ' - | - | - |' }
            elseif ($Impl -eq $Baseline) { $Row += " $Optimal / $($Overheads.Count) | (baseline) | - |" }
            else {
                $Worst = ($Overheads | Measure-Object -Maximum).Maximum
                $Row += " $Optimal / $($Overheads.Count) | $(Format-Overhead (Get-Median $Overheads)) | $(Format-Overhead $Worst) |"
            }
        }
        $Summary += $Row
    }
    $Summary += '', "_Median time vs fastest_ only counts the cases the algorithm ran (graph cut stops at smaller widths). " +
        "_Seam energy_ is the total energy of the pixels on the seam found; $Baseline finds the minimum, so +X% means a worse seam."
}

$Summary += '', '#### FindSeam: time for one seam', ''
if ($FindCases.Count -gt 0) {
    $Summary += @(Get-SpeedTable 'FindSeam' $FindCases $FindImpls)
    $Summary += '', "Fastest in bold; the others as a multiple of it. - = not run at that width."
} else {
    $Summary += 'No FindSeam results.'
}
$Summary += @(Get-UnavailableNote 'FindSeam')

if ($FindCases.Count -gt 0) {
    $Summary += '', '#### FindSeam: seam energy (lower is better)', ''
    $Summary += "| Image | Width | $($FindImpls -join ' | ') |", "|---|---:|$(($FindImpls | ForEach-Object { '---:' }) -join '|')|"
    foreach ($Case in $FindCases.Values) {
        $Base = $Case.Cells[$Baseline]
        $BaseEnergy = if ($Base -and $Base.Status -eq 'ok') { $Base.SeamEnergy } else { $null }
        $Row = foreach ($Impl in $FindImpls) {
            $R = $Case.Cells[$Impl]
            if (-not $R) { '-' }
            elseif ($R.Status -ne 'ok') { $R.Status }
            elseif ($null -eq $R.SeamEnergy) { '?' }
            elseif ($Impl -eq $Baseline -or $null -eq $BaseEnergy -or $BaseEnergy -eq 0) { Format-Number $R.SeamEnergy }
            else {
                $Overhead = ($R.SeamEnergy - $BaseEnergy) / $BaseEnergy
                if ([math]::Abs($Overhead) -le $EnergyTolerance) { 'optimal' } else { "$(Format-Number $R.SeamEnergy) ($(Format-Overhead $Overhead))" }
            }
        }
        $Summary += "| $($Case.Image) | $($Case.Width) px | $($Row -join ' | ') |"
    }
    if ($HasBaseline) {
        $Summary += '', "$Baseline shows the seam's total energy; the others say *optimal* when they match it, otherwise how much higher they are. A negative value would mean $Baseline missed the minimum."
    }
}

$CarveCases = Get-Cases 'Carve'
$CarveImpls = Get-Impls 'Carve'
$Summary += '', '#### Carve: remove 10% of the columns', ''
if ($CarveCases.Count -gt 0) {
    $Summary += @(Get-SpeedTable 'Carve' $CarveCases $CarveImpls)
} else {
    $Summary += 'No Carve results.'
}
$Summary += @(Get-UnavailableNote 'Carve')

if ($BigO.Count -gt 0) {
    $Columns = @($FindImpls | Where-Object { $I = $_; @($BigO | Where-Object { $_.Kind -eq 'FindSeam' -and $_.Impl -eq $I }).Count -gt 0 })
    $HasEnergy = @($BigO | Where-Object Kind -eq 'ComputeEnergy').Count -gt 0
    $Headers = @($Columns | ForEach-Object { "FindSeam $_" })
    if ($HasEnergy) { $Headers += 'ComputeEnergy' }
    $Summary += '', '#### Fitted complexity (N = image width; the aspect ratio stays fixed)', ''
    $Summary += "| Image | $($Headers -join ' | ') |", "|---|$(($Headers | ForEach-Object { '---' }) -join '|')|"
    foreach ($Image in $ImageOrder) {
        $Row = @(foreach ($Impl in $Columns) {
            $Fit = @($BigO | Where-Object { $_.Kind -eq 'FindSeam' -and $_.Impl -eq $Impl -and $_.Image -eq $Image } | Select-Object -First 1)
            if ($Fit.Count -gt 0) { $Fit[0].Complexity } else { '-' }
        })
        if ($HasEnergy) {
            $Fit = @($BigO | Where-Object { $_.Kind -eq 'ComputeEnergy' -and $_.Image -eq $Image } | Select-Object -First 1)
            $Row += if ($Fit.Count -gt 0) { $Fit[0].Complexity } else { '-' }
        }
        if (@($Row | Where-Object { $_ -ne '-' }).Count -gt 0) { $Summary += "| $Image | $($Row -join ' | ') |" }
    }
    $Summary += '', 'Fits over few widths are rough: graph cut only runs up to small images.'
}

# ComputeEnergy and the raw list are collapsed; they are not comparisons between algorithms
$Energy = @($All | Where-Object { $_.Kind -eq 'ComputeEnergy' })
if ($Energy.Count -gt 0) {
    $Summary += '', '<details><summary>ComputeEnergy (shared by all algorithms)</summary>', '', '| Image | Width | Time |', '|---|---:|---:|'
    $Summary += @($Energy | ForEach-Object {
        $Time = if ($_.Status -eq 'ok') { Format-Time $_.Ns } else { "$($_.Status): $($_.Message)" }
        "| $(if ($_.Image) { $_.Image } else { '-' }) | $(if ($_.Width) { "$($_.Width) px" } else { '-' }) | $Time |"
    })
    $Summary += '', '</details>'
}

$Summary += '', '<details><summary>All results</summary>', '', '| Benchmark | Wall time | CPU time | Seam energy | Status |', '|---|---:|---:|---:|---|'
$Summary += @($All | ForEach-Object {
    $Energy = if ($null -ne $_.SeamEnergy) { Format-Number $_.SeamEnergy } else { '' }
    if ($_.Status -eq 'ok') {
        $Cpu = [double]$_.Cpu * $UnitToNs[$_.Unit]
        $State = if ($_.Message) { "ok ($($_.Message))" } else { 'ok' }
        "| $($_.Name) | $(Format-Time $_.Ns) | $(Format-Time $Cpu) | $Energy | $State |"
    } else {
        "| $($_.Name) | - | - | | $($_.Status): $($_.Message) |"
    }
})
$Summary += '', '</details>'

# ---- Output ---------------------------------------------------------------------------------------------------
$Text = ($Summary -join "`n") + "`n"
Write-Host $Text
# AppendAllText writes UTF-8 without a BOM, which Out-File / >> in Windows PowerShell 5.1 would not
if ($SummaryPath) { [System.IO.File]::AppendAllText($SummaryPath, $Text) }
if ($Errors.Count -gt 0) { throw "$($Errors.Count) benchmark(s) failed with an error, see the summary above" }
