# Executar no PC de desenvolvimento depois de build.cmd (PowerShell 5 ou posterior).
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$build = Join-Path $root 'build'
$exe = (Get-Item -LiteralPath (Join-Path $build 'AtomTube.exe')).FullName
$cache = [IO.Directory]::CreateDirectory((Join-Path $build 'youtube-deps')).FullName
$release = '2026.10.26'
$source = "https://raw.githubusercontent.com/yt-dlp/yt-dlp/$release"
function Asset($name, $uri, $hash) {
    $path = Join-Path $cache $name
    if (!(Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path).Hash -ne $hash) {
        Invoke-WebRequest -UseBasicParsing $uri -OutFile $path
    }
    if ((Get-FileHash -LiteralPath $path).Hash -ne $hash) { throw "SHA-256 divergente em $name" }
    return $path
}
$extractor = Asset 'yt-dlp-vista-x86.exe' "https://github.com/3dyd/yt-dlp-builds/releases/download/$release/yt-dlp_x86.exe" 'f53f65e89e2477a68fce6af12f2d70beb4a88b390d98d07502ceff39c1f20a3d'
$js = Asset 'quickjs.zip' 'https://bellard.org/quickjs/binary_releases/quickjs-win-i686-2026-06-04.zip' 'b582d28d9f02bd8dec24a425192fddeabf5a7207561c7af7792b2aec169ff874'
$ff = Asset 'ffmpeg-6.zip' 'https://github.com/sudo-nautilus/FFmpeg-Builds-Win32/releases/download/latest/ffmpeg-n6.0-latest-win32-lgpl-shared-6.0.zip' 'acb272b24862f938aebcca6c8e51b9739ea619e96cba5da89b831b1e64442552'
$vlc = Asset 'vlc.zip' 'https://download.videolan.org/pub/videolan/vlc/3.0.23/win32/vlc-3.0.23-win32.zip' '8a12a33416ecb319ee06569fd7b5319af7362dfb6ef680df74b4a2c49af6515a'
$licenses = @(
    Asset "yt-dlp-$release-LICENSE.txt" "$source/LICENSE" '7e12e5df4bae12cb21581ba157ced20e1986a0508dd10d0e8a4ab9a4cf94e85c'
    Asset "yt-dlp-$release-THIRD_PARTY_LICENSES.txt" "$source/THIRD_PARTY_LICENSES.txt" '472aefe951c7db35e1657c1d13fd337140511ed6f2b329205105ad441c5a02b7'
    Asset 'QuickJS-LICENSE.txt' 'https://raw.githubusercontent.com/bellard/quickjs/master/LICENSE' '598fd7fc928e4350abce36e337ba5a1346923c5c692f5be92c3d8e29ddd7c18d'
    Asset 'winpthreads-LICENSE.txt' 'https://raw.githubusercontent.com/mingw-w64/mingw-w64/master/mingw-w64-libraries/winpthreads/COPYING' '63263614cdd29f2f93cba85e992f041b31f9fc7b4033692f31269489a8a1b177'
    Asset 'COPYING.GPLv3.txt' 'https://raw.githubusercontent.com/FFmpeg/FFmpeg/549430e14d/COPYING.GPLv3' '8ceb4b9ee5adedde47b31e975c1d90c73ad27b6b165a1dcd80c7c545eb65b903'
)
# O staging novo evita incluir DLLs de ensaios ou versões anteriores no ZIP.
$stage = [IO.Directory]::CreateDirectory((Join-Path $build ('kit-' + [Guid]::NewGuid()))).FullName
try {
    $content = [IO.Directory]::CreateDirectory((Join-Path $stage 'app')).FullName
    $yt = [IO.Directory]::CreateDirectory((Join-Path $content 'youtube')).FullName
    $python = if ($env:ATOMTUBE_PYTHON) { $env:ATOMTUBE_PYTHON } else { 'python' }
    & $python (Join-Path $PSScriptRoot 'extrator-portatil.py') $extractor $yt
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao preparar extrator portátil' }
    $plugin = Join-Path $yt 'yt-dlp-plugins\atomtube\yt_dlp_plugins\extractor'
    [IO.Directory]::CreateDirectory($plugin) | Out-Null
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'yt-dlp-plugins\atomtube\yt_dlp_plugins\extractor\atomtube_cache.py') -Destination $plugin
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'yt-dlp-plugins\atomtube\yt_dlp_plugins\extractor\atomtube_home.py') -Destination $plugin
    & (Join-Path $PSScriptRoot 'extrair-vlc.ps1') $vlc (Join-Path $yt 'vlc')
    [IO.Compression.ZipFile]::ExtractToDirectory($js,$yt)
    Copy-Item -LiteralPath $licenses -Destination $yt
    $ffdir = [IO.Directory]::CreateDirectory((Join-Path $yt 'ffmpeg')).FullName
    $archive = [IO.Compression.ZipFile]::OpenRead($ff)
    try {
        foreach ($item in $archive.Entries) {
            if ($item.FullName -match '/bin/([^/]+\.dll|ffmpeg\.exe|ffprobe\.exe)$' -or $item.FullName -match '/LICENSE\.txt$') {
                [IO.Compression.ZipFileExtensions]::ExtractToFile($item,(Join-Path $ffdir $item.Name))
            }
        }
    } finally { $archive.Dispose() }
    Copy-Item -LiteralPath $exe,(Join-Path $root 'LEIA-ME.txt'),(Join-Path $root 'LICENSE'),(Join-Path $root 'TERCEIROS.txt') -Destination $content
    foreach ($file in (Get-ChildItem -LiteralPath $content -Recurse -File | Where-Object Extension -in '.exe','.dll','.pyd')) {
        $bin = [IO.File]::ReadAllBytes($file.FullName)
        $pe = [BitConverter]::ToInt32($bin,60)
        if ([BitConverter]::ToUInt32($bin,$pe) -ne 0x4550 -or [BitConverter]::ToUInt16($bin,$pe+4) -ne 0x14c) {
            throw "Arquivo fora da arquitetura Win32: $($file.Name)"
        }
    }
    $zip = Join-Path $stage 'kit.zip'
    [IO.Compression.ZipFile]::CreateFromDirectory($content,$zip)
    $out = Join-Path $root 'AtomTube-0.9.2-x86.zip'
    Move-Item -LiteralPath $zip -Destination $out -Force
    Get-Item -LiteralPath $out | Select-Object Name,Length
} finally {
    $actual = (Resolve-Path -LiteralPath $stage).Path
    if ([IO.Path]::GetDirectoryName($actual) -eq $build -and [IO.Path]::GetFileName($actual) -match '^kit-[0-9a-f-]{36}$') {
        Remove-Item -LiteralPath $actual -Recurse -Force
    }
}
