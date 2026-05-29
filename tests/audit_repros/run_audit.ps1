$ErrorActionPreference = 'Stop'
$opus = Resolve-Path "..\..\bin\Release\opus.exe"
$dir = Resolve-Path "."
$tests = @(
    @{ name = "unsigned_compare"; expect_ok = 0 }
    @{ name = "unsigned_shr";     expect_ok = 0 }
    @{ name = "u32_signext";      expect_ok = 0 }
    @{ name = "unsigned_div";     expect_ok = 0 }
    @{ name = "shift_overflow";   expect_ok = $null }
    @{ name = "intmin_div";       expect_ok = $null }
    @{ name = "float_lt_compute"; expect_ok = 0 }
    @{ name = "float_neg_lt_pos"; expect_ok = $null }
)
foreach ($t in $tests) {
    $src = Join-Path $dir ($t.name + ".op")
    $exe = Join-Path $dir ($t.name + ".exe")
    if (Test-Path $exe) { Remove-Item $exe -Force }
    & $opus --quiet --fast $src 2>&1 | Out-Null
    if (-not (Test-Path $exe)) {
        Write-Host ("{0,-22} COMPILE FAIL" -f $t.name) -ForegroundColor Red
        continue
    }
    $p = Start-Process -FilePath $exe -PassThru -Wait
    $ec = $p.ExitCode
    if ($null -ne $t.expect_ok) {
        $tag = if ($ec -eq $t.expect_ok) { "[OK]" } else { "[BUG]" }
        $color = if ($ec -eq $t.expect_ok) { "Green" } else { "Red" }
        Write-Host ("{0,-22} ec={1,-12} {2}" -f $t.name, $ec, $tag) -ForegroundColor $color
    } else {
        Write-Host ("{0,-22} ec={1}" -f $t.name, $ec) -ForegroundColor Yellow
    }
}
