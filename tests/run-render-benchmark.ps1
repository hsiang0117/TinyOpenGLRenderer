param(
    [ValidateRange(64, 3840)][int]$Width = 1280,
    [ValidateRange(64, 2160)][int]$Height = 720,
    [ValidateRange(10, 10000)][int]$WarmupFrames = 60,
    [ValidateRange(30, 10000)][int]$SampleFrames = 120,
    [ValidateRange(1, 20)][int]$Rounds = 3,
    [switch]$CloudPresets,
    [switch]$CloudReference,
    [switch]$CloudHorizon,
    [switch]$CloudDistributionReference
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
function Get-BuildPath([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) { return $Path }
    return Join-Path $projectRoot $Path
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vsRoot = (& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
if (-not $vsRoot) { throw 'Visual Studio C++ tools were not found.' }
$msbuild = Join-Path $vsRoot 'MSBuild/Current/Bin/MSBuild.exe'
Push-Location $projectRoot
try {
    & $msbuild TinyOpenGLRenderer.sln /t:Build /p:Configuration=Release /p:Platform=x64 /nologo /verbosity:minimal
    if ($LASTEXITCODE -ne 0) { throw 'Release build failed.' }
    $buildPropertyJson = & $msbuild TinyOpenGLRenderer.vcxproj /p:Configuration=Release /p:Platform=x64 /getProperty:IntDir,OutDir,ExternalIncludeDir,ExternalLibraryDir /getItem:ClCompile /nologo
    if ($LASTEXITCODE -ne 0) { throw 'Renderer build paths could not be read.' }
    $buildInformation = $buildPropertyJson -join [Environment]::NewLine | ConvertFrom-Json
    $buildProperties = $buildInformation.Properties
    $objects = @($buildInformation.Items.ClCompile |
        Where-Object { $_.ExcludedFromBuild -ne 'true' -and [IO.Path]::GetFileName($_.Identity) -ne 'main.cpp' } |
        ForEach-Object {
            $objectPath = $_.ObjectFileName
            if ([IO.Path]::GetExtension($objectPath) -ne '.obj') {
                $objectPath = Join-Path $objectPath ([IO.Path]::GetFileNameWithoutExtension($_.Identity) + '.obj')
            }
            Get-Item -LiteralPath (Get-BuildPath $objectPath)
        })
    if ($objects.Count -eq 0) { throw 'Renderer object files were not found.' }
    $outputDir = Get-BuildPath $buildProperties.OutDir
    $buildDir = Join-Path $outputDir 'BenchmarkBuild'
    New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
    $benchmarkExe = Join-Path $outputDir 'renderBenchmark.exe'
    $responsePath = Join-Path $buildDir 'compile.rsp'
    $arguments = @(
        '/nologo', '/EHsc', '/std:c++17', '/utf-8', '/DGLFW_INCLUDE_NONE', '/DNDEBUG', '/MD', '/O2',
        ('/I"' + $buildProperties.ExternalIncludeDir + '"'),
        ('/I"' + (Join-Path $projectRoot 'src') + '"'),
        ('/Fo"' + $buildDir + '/"'),
        ('/Fe"' + $benchmarkExe + '"'),
        ('"' + (Join-Path $PSScriptRoot 'renderBenchmark.cpp') + '"'),
        '/link', '/LTCG', '/SUBSYSTEM:CONSOLE',
        ('/LIBPATH:"' + $buildProperties.ExternalLibraryDir + '"'),
        'glfw3.lib', 'opengl32.lib', 'assimp-vc143-mtd.lib',
        'user32.lib', 'gdi32.lib', 'shell32.lib', 'imm32.lib'
    )
    $arguments += $objects | ForEach-Object { '"' + $_.FullName + '"' }
    ($arguments -join ' ') | Set-Content -LiteralPath $responsePath -Encoding Unicode
    $devCmd = Join-Path $vsRoot 'Common7/Tools/VsDevCmd.bat'
    $compileCommand = 'call "' + $devCmd + '" -no_logo -arch=x64 -host_arch=x64 >nul && cl.exe @"' + $responsePath + '"'
    & cmd.exe /d /c $compileCommand
    if ($LASTEXITCODE -ne 0) { throw 'Benchmark build failed.' }
    $runName = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
    $runDir = Join-Path $projectRoot "x64/BenchmarkRuns/$runName"
    New-Item -ItemType Directory -Path $runDir -Force | Out-Null
    $metadata = [ordered]@{
        date = (Get-Date -Format o)
        configuration = 'Release x64 /MD /O2; bundled Debug Assimp (no imported models in benchmark)'
        cpu = @((Get-CimInstance Win32_Processor).Name)
        videoControllers = @((Get-CimInstance Win32_VideoController | Select-Object Name, DriverVersion))
        gitHead = (& git rev-parse HEAD)
        benchmarkSha256 = (Get-FileHash -LiteralPath $benchmarkExe -Algorithm SHA256).Hash
        width = $Width; height = $Height; warmupFrames = $WarmupFrames; sampleFrames = $SampleFrames; rounds = $Rounds
        cloudPresets = $CloudPresets.IsPresent
        cloudReference = $CloudReference.IsPresent
        cloudHorizon = $CloudHorizon.IsPresent
        cloudDistributionReference = $CloudDistributionReference.IsPresent
    }
    $metadata | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $runDir 'environment.json') -Encoding UTF8
    & $benchmarkExe --output $runDir --width $Width --height $Height --warmup $WarmupFrames --frames $SampleFrames --rounds $Rounds --cloud-presets ([int]$CloudPresets.IsPresent) --cloud-reference ([int]$CloudReference.IsPresent) --cloud-horizon ([int]$CloudHorizon.IsPresent) --cloud-distribution-reference ([int]$CloudDistributionReference.IsPresent)
    if ($LASTEXITCODE -ne 0) { throw "Benchmark failed; partial outputs: $runDir" }
    Write-Output "Benchmark outputs: $runDir"
} finally {
    Pop-Location
}
