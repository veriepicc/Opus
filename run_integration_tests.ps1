# integration test runner for x64-smart-instructions spec
# runs all 20 test files and measures compile time + binary size

$tests = @(
    "test_5plus_args",
    "test_break_continue",
    "test_chained_fields",
    "test_compound_fields",
    "test_div_mod",
    "test_dll_arrays",
    "test_dll_strings",
    "test_else_if",
    "test_enum_values",
    "test_globals_simple",
    "test_globals",
    "test_import_basic",
    "test_index_expr_array",
    "test_method_args",
    "test_multi_return",
    "test_nested_exprs",
    "test_print_int_decimal",
    "test_parallel_capture_shadow",
    "test_reg_preserve",
    "test_short_circuit",
    "test_str_arg_helper",
    "test_str_return_branch",
    "test_struct_ptr_params",
    "test_unary_complex"
)

$negativeTests = @(
    @{
        Name = "test_import_lib"
        File = "tests\\test_import_lib.op"
        ExpectedErrorContains = "no 'main' function found"
    },
    "invalid_unterminated_block_comment",
    "invalid_unknown_string_escape",
    "invalid_hex_string_odd_nibbles",
    "invalid_unterminated_string_escape",
    "invalid_unterminated_char_escape",
    "invalid_english_block",
    "invalid_struct_literal_eof",
    @{
        Name = "invalid_spawn_void"
        File = "tests\\invalid_spawn_void.op"
        ExpectedErrorContains = "must return an integer-compatible value"
    },
    @{
        Name = "invalid_spawn_arg_count"
        File = "tests\\invalid_spawn_arg_count.op"
        ExpectedErrorContains = "requires 2 arguments, got 1"
    },
    @{
        Name = "invalid_parallel_capture_mutation"
        File = "tests\\invalid_parallel_capture_mutation.op"
        ExpectedErrorContains = "parallel for cannot mutate captured variable 'shared'"
    },
    @{
        Name = "invalid_parallel_global_mutation"
        File = "tests\\invalid_parallel_global_mutation.op"
        ExpectedErrorContains = "parallel for cannot mutate global 'global_counter' directly"
    }
)

$negativeRunTests = @(
    @{
        Name = "invalid_jit_parallel_for"
        File = "tests\\invalid_jit_parallel_for.op"
        ExpectedErrorContains = "parallel for is not supported in JIT mode yet"
    }
)

$positiveRunTests = @(
    @{
        Name = "jit_cast_to_int_run"
        File = "tests\\jit_cast_to_int_run.op"
        ExpectedExit = 0
        ExpectedOutputContains = "Program returned: 3"
    },
    @{
        Name = "jit_narrow_i32_run"
        File = "tests\\jit_narrow_i32_run.op"
        ExpectedExit = 0
        ExpectedOutputContains = "2`r`n2`r`nProgram returned: 0"
    },
    @{
        Name = "jit_let_mut_typed_run"
        File = "tests\\jit_let_mut_typed_run.op"
        ExpectedExit = 0
        ExpectedOutputContains = "Program returned: 3"
    },
    @{
        Name = "jit_index_expr_run"
        File = "tests\\jit_index_expr_run.op"
        ExpectedExit = 0
        ExpectedOutputContains = "10`r`n20`r`n30`r`nProgram returned: 0"
    },
    @{
        Name = "jit_array_literal_run"
        File = "tests\\jit_array_literal_run.op"
        ExpectedExit = 0
        ExpectedOutputContains = "2`r`nProgram returned: 0"
    },
    @{
        Name = "jit_await_null_run"
        File = "tests\\jit_await_null_run.op"
        ExpectedExit = 0
        ExpectedOutputContains = "0`r`nProgram returned: 0"
    },
    @{
        Name = "jit_atomic_cas_run"
        File = "tests\\jit_atomic_cas_run.op"
        ExpectedExit = 0
        ExpectedOutputContains = "1`r`n7`r`n0`r`n7`r`nProgram returned: 0"
    }
)

$projectTests = @(
    @{
        Name = "project_test3"
        ProjectFile = "tests\project_tests\test3\opus.project"
        ExeFile = "tests\project_tests\test3\Test3.exe"
        ExpectedExit = 55
        RuntimeTimeoutSeconds = 5
    },
    @{
        Name = "healing_auto_heal"
        ProjectFile = "tests\healing_tests\auto_heal\opus.project"
        ExeFile = "tests\healing_tests\auto_heal\AutoHeal.exe"
        ExpectedExit = 7
        RuntimeTimeoutSeconds = 5
    }
)

$negativeProjectTests = @(
    @{
        Name = "project_test4_duplicate_function_conflict"
        ProjectFile = "tests\project_tests\test4\opus.project"
        ExpectedErrorContains = "conflicts with function"
    },
    @{
        Name = "project_test5_import_cycle"
        ProjectFile = "tests\project_tests\test5\opus.project"
        ExpectedErrorContains = "import cycle detected"
    },
    @{
        Name = "project_test6_duplicate_project_decl"
        ProjectFile = "tests\project_tests\test6\opus.project"
        ExpectedErrorContains = "multiple project declarations found"
    }
)

$results = @()
$totalTests = $tests.Count + $negativeTests.Count + $negativeRunTests.Count + $positiveRunTests.Count + $projectTests.Count + $negativeProjectTests.Count
$passedTests = 0
$failedTests = 0

Write-Host "=== x64 Smart Instructions Integration Test Suite ===" -ForegroundColor Cyan
Write-Host "Running $totalTests tests..." -ForegroundColor Cyan
Write-Host ""

function Invoke-BoundedProcess {
    param(
        [string]$FilePath,
        [int]$TimeoutSeconds = 5
    )

    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $FilePath
    $startInfo.WorkingDirectory = (Get-Location).Path
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.CreateNoWindow = $true

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    $null = $process.Start()

    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        try {
            $process.Kill()
        } catch {
        }

        return @{
            TimedOut = $true
            ExitCode = $null
            StdOut = ""
            StdErr = ""
        }
    }

    $process.WaitForExit()

    return @{
        TimedOut = $false
        ExitCode = $process.ExitCode
        StdOut = $process.StandardOutput.ReadToEnd()
        StdErr = $process.StandardError.ReadToEnd()
    }
}

foreach ($test in $tests) {
    $testFile = "tests\$test.op"
    $exeFile = "tests\$test.exe"
    
    Write-Host "Testing: $test" -ForegroundColor Yellow
    if (Test-Path $exeFile) {
        Remove-Item $exeFile -Force
    }
    
    # compile and measure time
    $compileTime = (Measure-Command {
        $output = & .\bin\Release\Opus.exe $testFile 2>&1
        $script:compileOutput = $output
    }).TotalMilliseconds
    
    # check if compilation succeeded
    if ($LASTEXITCODE -eq 0 -and (Test-Path $exeFile)) {
        # get binary size
        $binarySize = (Get-Item $exeFile).Length / 1KB
        
        $result = [PSCustomObject]@{
            Test = $test
            Status = "PASS"
            CompileTime = [math]::Round($compileTime, 2)
            BinarySize = [math]::Round($binarySize, 2)
        }
        
        Write-Host "  PASS - ${compileTime}ms, ${binarySize}KB" -ForegroundColor Green
        $passedTests++
    } else {
        $result = [PSCustomObject]@{
            Test = $test
            Status = "FAIL"
            CompileTime = [math]::Round($compileTime, 2)
            BinarySize = 0
            Error = $script:compileOutput
        }
        
        Write-Host "  FAIL - compilation error" -ForegroundColor Red
        Write-Host "  Error: $($script:compileOutput)" -ForegroundColor Red
        $failedTests++
    }
    
    $results += $result
}

foreach ($project in $projectTests) {
    $projectFile = $project.ProjectFile
    $exeFile = $project.ExeFile
    $runtimeTimeoutSeconds = if ($project.ContainsKey("RuntimeTimeoutSeconds")) { $project.RuntimeTimeoutSeconds } else { 5 }

    Write-Host "Testing: $($project.Name)" -ForegroundColor Yellow
    if (Test-Path $exeFile) {
        Remove-Item $exeFile -Force
    }

    $compileTime = (Measure-Command {
        $output = & .\bin\Release\Opus.exe build $projectFile 2>&1
        $script:compileOutput = $output
    }).TotalMilliseconds

    if ($LASTEXITCODE -eq 0 -and (Test-Path $exeFile)) {
        $runtime = Invoke-BoundedProcess -FilePath (Join-Path (Get-Location) $exeFile) -TimeoutSeconds $runtimeTimeoutSeconds
        $timedOut = $runtime.TimedOut
        if (-not $timedOut) {
            $runtimeExit = $runtime.ExitCode
            $runtimeOutput = (($runtime.StdOut + $runtime.StdErr).Trim())
            if ($runtimeOutput.Length -gt 0) {
                Write-Host $runtimeOutput
            }
        }

        if ($timedOut) {
            $result = [PSCustomObject]@{
                Test = $project.Name
                Status = "FAIL"
                CompileTime = [math]::Round($compileTime, 2)
                BinarySize = [math]::Round(((Get-Item $exeFile).Length / 1KB), 2)
                Error = "runtime timed out after $runtimeTimeoutSeconds seconds"
            }

            Write-Host "  FAIL - runtime timeout" -ForegroundColor Red
            Write-Host "  Error: runtime timed out after $runtimeTimeoutSeconds seconds" -ForegroundColor Red
            $failedTests++
        } elseif ($runtimeExit -eq $project.ExpectedExit) {
            $binarySize = (Get-Item $exeFile).Length / 1KB

            $result = [PSCustomObject]@{
                Test = $project.Name
                Status = "PASS"
                CompileTime = [math]::Round($compileTime, 2)
                BinarySize = [math]::Round($binarySize, 2)
            }

            Write-Host "  PASS - ${compileTime}ms, ${binarySize}KB, exit $runtimeExit" -ForegroundColor Green
            $passedTests++
        } else {
            $result = [PSCustomObject]@{
                Test = $project.Name
                Status = "FAIL"
                CompileTime = [math]::Round($compileTime, 2)
                BinarySize = [math]::Round(((Get-Item $exeFile).Length / 1KB), 2)
                Error = "expected exit $($project.ExpectedExit), got $runtimeExit"
            }

            Write-Host "  FAIL - runtime exit mismatch" -ForegroundColor Red
            Write-Host "  Error: expected exit $($project.ExpectedExit), got $runtimeExit" -ForegroundColor Red
            $failedTests++
        }
    } else {
        $result = [PSCustomObject]@{
            Test = $project.Name
            Status = "FAIL"
            CompileTime = [math]::Round($compileTime, 2)
            BinarySize = 0
            Error = $script:compileOutput
        }

        Write-Host "  FAIL - project compilation error" -ForegroundColor Red
        Write-Host "  Error: $($script:compileOutput)" -ForegroundColor Red
        $failedTests++
    }

    $results += $result
}

foreach ($project in $negativeProjectTests) {
    $projectFile = $project.ProjectFile

    Write-Host "Testing: $($project.Name)" -ForegroundColor Yellow

    $compileTime = (Measure-Command {
        $output = & .\bin\Release\Opus.exe build $projectFile 2>&1
        $script:compileOutput = $output
    }).TotalMilliseconds

    $compileText = ($script:compileOutput | Out-String)

    if ($LASTEXITCODE -ne 0 -and $compileText.Contains($project.ExpectedErrorContains)) {
        $result = [PSCustomObject]@{
            Test = $project.Name
            Status = "PASS"
            CompileTime = [math]::Round($compileTime, 2)
            BinarySize = 0
        }

        Write-Host "  PASS - expected project compile failure in ${compileTime}ms" -ForegroundColor Green
        $passedTests++
    } else {
        $result = [PSCustomObject]@{
            Test = $project.Name
            Status = "FAIL"
            CompileTime = [math]::Round($compileTime, 2)
            BinarySize = 0
            Error = $script:compileOutput
        }

        Write-Host "  FAIL - expected project compilation failure" -ForegroundColor Red
        Write-Host "  Error: $($script:compileOutput)" -ForegroundColor Red
        $failedTests++
    }

    $results += $result
}

foreach ($test in $negativeTests) {
    $testName = if ($test -is [string]) { $test } else { $test.Name }
    $testFile = if ($test -is [string]) { "tests\$test.op" } else { $test.File }
    $exeFile = "tests\$testName.exe"
    
    Write-Host "Testing: $testName" -ForegroundColor Yellow
    if (Test-Path $exeFile) {
        Remove-Item $exeFile -Force
    }
    
    $compileTime = (Measure-Command {
        $output = & .\bin\Release\Opus.exe $testFile 2>&1
        $script:compileOutput = $output
    }).TotalMilliseconds
    
    $compileText = ($script:compileOutput | Out-String)
    $expectedMessage = if ($test -is [string]) { $null } else { $test.ExpectedErrorContains }

    if ($LASTEXITCODE -ne 0 -and ($null -eq $expectedMessage -or $compileText.Contains($expectedMessage))) {
        $result = [PSCustomObject]@{
            Test = $testName
            Status = "PASS"
            CompileTime = [math]::Round($compileTime, 2)
            BinarySize = 0
        }
        
        Write-Host "  PASS - expected compile failure in ${compileTime}ms" -ForegroundColor Green
        $passedTests++
    } else {
        $result = [PSCustomObject]@{
            Test = $testName
            Status = "FAIL"
            CompileTime = [math]::Round($compileTime, 2)
            BinarySize = if (Test-Path $exeFile) { [math]::Round(((Get-Item $exeFile).Length / 1KB), 2) } else { 0 }
            Error = if ($null -eq $expectedMessage) {
                "expected compilation to fail"
            } else {
                "expected compilation to fail with message containing '$expectedMessage'"
            }
        }
        
        Write-Host "  FAIL - malformed input compiled successfully" -ForegroundColor Red
        $failedTests++
    }
    
    $results += $result
}

foreach ($runTest in $positiveRunTests) {
    Write-Host "Testing: $($runTest.Name)" -ForegroundColor Yellow

    $output = & .\bin\Release\Opus.exe --run $runTest.File 2>&1
    $combined = ($output | Out-String)
    $expectedExit = $runTest.ExpectedExit
    $expectedOutput = $runTest.ExpectedOutputContains
    $exitOk = $LASTEXITCODE -eq $expectedExit
    $outputOk = $combined.Contains($expectedOutput)

    if ($exitOk -and $outputOk) {
        $result = [PSCustomObject]@{
            Test = $runTest.Name
            Status = "PASS"
            CompileTime = 0
            BinarySize = 0
        }

        Write-Host "  PASS - run output matched" -ForegroundColor Green
        $passedTests++
    } else {
        $result = [PSCustomObject]@{
            Test = $runTest.Name
            Status = "FAIL"
            CompileTime = 0
            BinarySize = 0
            Error = $combined
        }

        Write-Host "  FAIL - unexpected run result" -ForegroundColor Red
        Write-Host "  Output: $combined" -ForegroundColor Red
        $failedTests++
    }

    $results += $result
}

foreach ($test in $negativeRunTests) {
    $testName = $test.Name
    $testFile = $test.File

    Write-Host "Testing: $testName" -ForegroundColor Yellow

    $compileTime = (Measure-Command {
        $output = & .\bin\Release\Opus.exe --run $testFile 2>&1
        $script:compileOutput = $output
    }).TotalMilliseconds

    $compileText = ($script:compileOutput | Out-String)

    if ($LASTEXITCODE -ne 0 -and $compileText.Contains($test.ExpectedErrorContains)) {
        $result = [PSCustomObject]@{
            Test = $testName
            Status = "PASS"
            CompileTime = [math]::Round($compileTime, 2)
            BinarySize = 0
        }

        Write-Host "  PASS - expected run failure in ${compileTime}ms" -ForegroundColor Green
        $passedTests++
    } else {
        $result = [PSCustomObject]@{
            Test = $testName
            Status = "FAIL"
            CompileTime = [math]::Round($compileTime, 2)
            BinarySize = 0
            Error = "expected run failure with message containing '$($test.ExpectedErrorContains)'"
        }

        Write-Host "  FAIL - unexpected run result" -ForegroundColor Red
        Write-Host "  Error: $($script:compileOutput)" -ForegroundColor Red
        $failedTests++
    }

    $results += $result
}

Write-Host ""
Write-Host "=== Test Summary ===" -ForegroundColor Cyan
Write-Host "Total: $totalTests" -ForegroundColor White
Write-Host "Passed: $passedTests" -ForegroundColor Green
Write-Host "Failed: $failedTests" -ForegroundColor Red
Write-Host ""

if ($passedTests -gt 0) {
    $avgCompileTime = ($results | Where-Object { $_.Status -eq "PASS" } | Measure-Object -Property CompileTime -Average).Average
    $maxCompileTime = ($results | Where-Object { $_.Status -eq "PASS" } | Measure-Object -Property CompileTime -Maximum).Maximum
    $avgBinarySize = ($results | Where-Object { $_.Status -eq "PASS" } | Measure-Object -Property BinarySize -Average).Average
    $maxBinarySize = ($results | Where-Object { $_.Status -eq "PASS" } | Measure-Object -Property BinarySize -Maximum).Maximum
    
    Write-Host "=== Performance Metrics ===" -ForegroundColor Cyan
    Write-Host "Compile Time:" -ForegroundColor White
    Write-Host "  Average: $([math]::Round($avgCompileTime, 2))ms" -ForegroundColor White
    Write-Host "  Maximum: $([math]::Round($maxCompileTime, 2))ms" -ForegroundColor White
    $compileColor = if ($maxCompileTime -lt 100) { "Green" } else { "Red" }
    Write-Host "  Target: under 100ms" -ForegroundColor $compileColor
    Write-Host ""
    Write-Host "Binary Size:" -ForegroundColor White
    Write-Host "  Average: $([math]::Round($avgBinarySize, 2))KB" -ForegroundColor White
    Write-Host "  Maximum: $([math]::Round($maxBinarySize, 2))KB" -ForegroundColor White
    $sizeColor = if ($maxBinarySize -lt 14) { "Green" } else { "Red" }
    Write-Host "  Target: under 14KB" -ForegroundColor $sizeColor
}

Write-Host ""
Write-Host "=== Detailed Results ===" -ForegroundColor Cyan
$results | Format-Table -AutoSize

# export to csv for analysis
$results | Export-Csv -Path "integration_test_results.csv" -NoTypeInformation
Write-Host "Results exported to integration_test_results.csv" -ForegroundColor Gray

if ($failedTests -gt 0) {
    exit 1
}
exit 0
