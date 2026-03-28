param(
    [string]$StdlibDir = (Join-Path $PSScriptRoot "..\stdlib"),
    [string]$OutputPath = (Join-Path $PSScriptRoot "..\src\stdlib.ixx")
)

$ErrorActionPreference = "Stop"

function Get-ModuleName([string]$Root, [string]$FullPath) {
    $rootPath = [System.IO.Path]::GetFullPath($Root)
    $filePath = [System.IO.Path]::GetFullPath($FullPath)

    $rootUri = New-Object System.Uri(($rootPath.TrimEnd('\') + '\'))
    $fileUri = New-Object System.Uri($filePath)
    $relative = [System.Uri]::UnescapeDataString($rootUri.MakeRelativeUri($fileUri).ToString())
    $relative = $relative.Replace('\', '/')
    if ($relative.EndsWith(".op")) {
        $relative = $relative.Substring(0, $relative.Length - 3)
    }
    return $relative.Replace('/', '.')
}

$stdlibRoot = [System.IO.Path]::GetFullPath($StdlibDir)
$outputFile = [System.IO.Path]::GetFullPath($OutputPath)
$outputDir = Split-Path -Parent $outputFile

if (-not (Test-Path $stdlibRoot)) {
    throw "stdlib directory not found: $stdlibRoot"
}

if (-not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir | Out-Null
}

$files = Get-ChildItem -Path $stdlibRoot -Filter *.op -Recurse | Sort-Object FullName

$builder = New-Object System.Text.StringBuilder
[void]$builder.AppendLine("// generated file - do not edit by hand")
[void]$builder.AppendLine()
[void]$builder.AppendLine("export module opus.stdlib;")
[void]$builder.AppendLine()
[void]$builder.AppendLine("import std;")
[void]$builder.AppendLine()
[void]$builder.AppendLine("export namespace opus {")
[void]$builder.AppendLine("export inline const std::unordered_map<std::string, std::string>& embedded_stdlib_sources() {")
[void]$builder.AppendLine("    static const std::unordered_map<std::string, std::string> sources = {")

foreach ($file in $files) {
    $moduleName = Get-ModuleName $stdlibRoot $file.FullName
    $safeName = ($moduleName -replace '[^A-Za-z0-9]', '').ToUpper()
    if ($safeName.Length -gt 8) {
        $safeName = $safeName.Substring(0, 8)
    }
    $delimiter = "OP" + $safeName
    $content = Get-Content -Path $file.FullName -Raw
    [void]$builder.AppendLine("        {""$moduleName"", R""$delimiter(")
    [void]$builder.Append($content)
    if (-not $content.EndsWith("`n")) {
        [void]$builder.AppendLine()
    }
    [void]$builder.AppendLine(")$delimiter""},")
}

[void]$builder.AppendLine("    };")
[void]$builder.AppendLine("    return sources;")
[void]$builder.AppendLine("}")
[void]$builder.AppendLine("}")

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($outputFile, $builder.ToString(), $utf8NoBom)
Write-Host "generated embedded stdlib module: $outputFile"
