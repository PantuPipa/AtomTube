param([string]$ArchivePath,[string]$Destination)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$modules = 'avcodec','faad','dxva2','direct3d9','drawable','directsound','mmdevice','wasapi','waveout','packetizer_h264','packetizer_mpeg4audio','cache_read','cache_block','prefetch','float_mixer','integer_mixer','audio_format','simple_channel_mixer','trivial_channel_mixer','ugly_resampler','samplerate','gain','swscale','i420_nv12','wingdi','filesystem','i420_rgb','i420_yuy2','yuy2_i420'
$modules += 'ts','mp4','sharpen','imem','access_imem','es','i420_rgb_sse2','direct3d9_filters','chain','vmem'
$zip = [IO.Compression.ZipFile]::OpenRead((Resolve-Path -LiteralPath $ArchivePath).Path)
try {
    foreach ($entry in $zip.Entries) {
        $name = $entry.FullName -replace '^vlc-3\.0\.23/',''
        if ($name -notin 'libvlc.dll','libvlccore.dll','COPYING.txt','THANKS.txt','AUTHORS.txt' -and
            !($name -match '^plugins/[a-z0-9_]+/lib([a-z0-9_]+)_plugin\.dll$' -and $modules -contains $Matches[1])) { continue }
        $path = Join-Path $Destination $name
        [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path))
        [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$path,$true)
    }
} finally { $zip.Dispose() }

