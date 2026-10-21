param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vsRoot = (& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
if (-not $vsRoot) { throw 'Visual Studio C++ tools were not found.' }
$msbuild = Join-Path $vsRoot 'MSBuild/Current/Bin/MSBuild.exe'
Push-Location $projectRoot
try {
    & $msbuild TinyOpenGLRenderer.sln /t:Build "/p:Configuration=$Configuration" /p:Platform=x64 /nologo /verbosity:minimal
    if ($LASTEXITCODE -ne 0) { throw 'Renderer build failed.' }
    $buildPropertyJson = & $msbuild TinyOpenGLRenderer.vcxproj "/p:Configuration=$Configuration" /p:Platform=x64 /getProperty:IntDir,OutDir,ExternalIncludeDir,ExternalLibraryDir /nologo
    if ($LASTEXITCODE -ne 0) { throw 'Renderer build paths could not be read.' }
    $buildProperties = ($buildPropertyJson -join [Environment]::NewLine | ConvertFrom-Json).Properties
    $intDir = $buildProperties.IntDir
    $objects = @(Get-ChildItem -LiteralPath (Join-Path $projectRoot $intDir) -Filter '*.obj' | Where-Object Name -ne 'main.obj')
    if ($objects.Count -eq 0) { throw 'Renderer object files were not found.' }
    $outputDir = $buildProperties.OutDir
    $testDir = Join-Path $outputDir 'SafetyChecks'
    New-Item -ItemType Directory -Path $testDir -Force | Out-Null
    $testExe = Join-Path $outputDir 'renderSafety.exe'
    $responsePath = Join-Path $testDir 'compile.rsp'
    $arguments = @(
        '/nologo', '/EHsc', '/std:c++17', '/DGLFW_INCLUDE_NONE', '/Zi',
        ('/I"' + $buildProperties.ExternalIncludeDir + '"'),
        ('/I"' + (Join-Path $projectRoot 'src') + '"'),
        ('/Fo"' + $testDir + '/"'),
        ('/Fd"' + (Join-Path $testDir 'compile.pdb') + '"'),
        ('/Fe"' + $testExe + '"'),
        ('"' + (Join-Path $PSScriptRoot 'renderSafety.cpp') + '"'),
        ('"' + (Join-Path $PSScriptRoot 'renderCorrectness.cpp') + '"'),
        ('"' + (Join-Path $PSScriptRoot 'cloudNoiseChecks.cpp') + '"'),
        ('"' + (Join-Path $PSScriptRoot 'cloudSamplingChecks.cpp') + '"'),
        ('"' + (Join-Path $PSScriptRoot 'renderPerformance.cpp') + '"'),
        ('"' + (Join-Path $PSScriptRoot 'renderStartup.cpp') + '"')
    )
    if ($Configuration -eq 'Debug') { $arguments += '/MDd', '/D_DEBUG' }
    else { $arguments += '/MD', '/DNDEBUG', '/O2' }
    $arguments += '/link', '/DEBUG', '/SUBSYSTEM:CONSOLE',
        ('/PDB:"' + (Join-Path $testDir 'renderSafety.pdb') + '"'),
        ('/LIBPATH:"' + $buildProperties.ExternalLibraryDir + '"'),
        'glfw3.lib', 'opengl32.lib', 'assimp-vc143-mtd.lib',
        'user32.lib', 'gdi32.lib', 'shell32.lib', 'imm32.lib'
    if ($Configuration -eq 'Debug') { $arguments += '/NODEFAULTLIB:msvcrt.lib' }
    else { $arguments += '/LTCG' }
    $arguments += $objects | ForEach-Object { '"' + $_.FullName + '"' }
    # CL expects /link and its options on the same response-file line.
    ($arguments -join ' ') | Set-Content -LiteralPath $responsePath -Encoding Unicode
    $devCmd = Join-Path $vsRoot 'Common7/Tools/VsDevCmd.bat'
    $compileCommand = 'call "' + $devCmd + '" -no_logo -arch=x64 -host_arch=x64 >nul && cl.exe @"' + $responsePath + '"'
    & cmd.exe /d /c $compileCommand
    if ($LASTEXITCODE -ne 0) { throw 'Safety check build failed.' }
    & $testExe
    if ($LASTEXITCODE -ne 0) { throw 'Safety checks failed.' }
} finally {
    Pop-Location
}
