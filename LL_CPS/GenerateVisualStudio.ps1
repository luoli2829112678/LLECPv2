param([switch]$Build,[switch]$Rebuild,[switch]$Clean,[ValidateSet('Debug','Release','RelWithDebInfo')][string]$Configuration='Release',[string]$BuildDirectory)
$ErrorActionPreference='Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswherePath)) { throw '找不到 vswhere，请安装 Visual Studio C++ 桌面开发组件。' }
$installation = (& $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json | ConvertFrom-Json)[0]
if (!$installation) { throw '找不到带有 x64 C++ 工具链的 Visual Studio。' }
$major = [int]($installation.installationVersion.Split('.')[0])
if ($major -ge 18) { $generator = 'Visual Studio 18 2026' }
elseif ($major -ge 17) { $generator = 'Visual Studio 17 2022' }
else { throw '需要 Visual Studio 2022 或更新版本。' }
$cmakePath = Join-Path $installation.installationPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (!(Test-Path -LiteralPath $cmakePath)) { $cmakePath = (Get-Command cmake -ErrorAction Stop).Source }
if (!$BuildDirectory) { $BuildDirectory = Join-Path $projectRoot 'build\windows-vs' }
$buildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
if ($Clean) {
    if (Test-Path -LiteralPath (Join-Path $buildDirectory 'CMakeCache.txt')) {
        & $cmakePath --build $buildDirectory --config $Configuration --target clean
        if ($LASTEXITCODE -ne 0) { throw '清理失败。' }
    }
    Write-Host '清理完成；解决方案和源文件保留在 LL_CPS。'
    exit 0
}
& $cmakePath -S $projectRoot -B $buildDirectory -G $generator -A x64 -DLLECP_BUILD_CONTROLLER=OFF -DLLECP_BUILD_CPS=ON -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) { throw 'CMake 配置失败。' }
if ($Build -or $Rebuild) {
    $buildArgs = @("--build", $buildDirectory, "--config", $Configuration, "--parallel", "6")
    if ($Rebuild) { $buildArgs += "--clean-first" }
    & $cmakePath @buildArgs
    if ($LASTEXITCODE -ne 0) { throw 'Visual Studio 编译失败。' }
}
Write-Host ('解决方案: ' + (Join-Path $PSScriptRoot 'LL_CPS.sln'))
Write-Host "程序目录: $buildDirectory\bin"
