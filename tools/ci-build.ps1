$ErrorActionPreference = 'Stop'
$msbuild = (Get-Command msbuild).Source
$directory = Split-Path $msbuild
while ($directory -and -not (Test-Path "$directory/VC/Auxiliary/Build")) {
    $directory = Split-Path $directory
}
if (-not $directory) { throw "Cannot locate VC tools for $msbuild" }
$build = "$directory/VC/Auxiliary/Build"
$version = (Get-Content "$build/Microsoft.VCToolsVersion.default.txt" -Raw).Trim()
$toolsets = @(Get-ChildItem "$build/Microsoft.VCToolsVersion.v*.default.txt" | Where-Object {
    (Get-Content $_.FullName -Raw).Trim() -eq $version
})
if ($toolsets.Count -ne 1) { throw "Cannot uniquely identify installed toolset for MSVC $version" }
$toolset = [regex]::Match($toolsets[0].Name, '\.(v\d+)\.').Groups[1].Value
Write-Host "MSBuild: $msbuild; MSVC: $version; toolset: $toolset"
& $msbuild Opus.sln /p:Configuration=Release /p:Platform=x64 "/p:PlatformToolset=$toolset" /m
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
