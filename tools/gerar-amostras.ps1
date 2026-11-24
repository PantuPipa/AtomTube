param([string]$Destino = (Join-Path $PSScriptRoot '..\amostras'))
$ErrorActionPreference = 'Stop'
$ffmpeg = (Get-Command ffmpeg.exe -ErrorAction Stop).Source
$dest = [IO.Directory]::CreateDirectory($Destino).FullName
$temp = Join-Path $dest ([Guid]::NewGuid().ToString() + '.mp4')
function Encode([string[]]$Argumentos) {
    & $ffmpeg -hide_banner -loglevel error -nostdin @Argumentos
    if ($LASTEXITCODE -ne 0) { throw "FFmpeg terminou com erro $LASTEXITCODE." }
}
try {
    foreach ($ref in @('A 640 360 24','B 854 480 24','C 1280 720 24','D 1280 720 30')) {
        $id,$w,$h,$fps = $ref.Split(' ')
        $size = "${w}x${h}"
        $name = "${id}_${size}_${fps}fps.mp4"
        Write-Host "Gerando $name"
        # Movimento em um quadro central, letras finas e sinal sincronizado a cada segundo.
        $halfW = 2 * [math]::Floor([int]$w / 4)
        $halfH = 2 * [math]::Floor([int]$h / 4)
        $font = ($env:WINDIR.Replace('\','/').Replace(':','\:') + '/Fonts/arial.ttf')
        $filter = "testsrc2=s=${halfW}x${halfH}:r=$fps,pad=${w}:${h}:(ow-iw)/2:(oh-ih)/2:color=0x152135,"
        $filter += "drawtext=fontfile='${font}':text='AtomTube   |   Leitura e movimento':fontsize=h/18:fontcolor=white:x=(w-tw)/2:y=h/12,"
        $filter += "drawtext=fontfile='${font}':text='0123456789   a b c   x + y = 24':fontsize=h/36:fontcolor=white:x=(w-tw)/2:y=h*0.81,"
        $filter += "drawbox=x=iw/12:y=ih/2:w=iw/12:h=ih/8:color=yellow:t=fill:enable='lt(mod(t,1),0.10)'"
        Encode @('-y','-f','lavfi','-i',$filter,'-t','10','-an','-c:v','libx264','-threads','4','-preset','fast','-crf','22',
            '-maxrate','2M','-bufsize','4M','-profile:v','main','-level:v','3.1','-pix_fmt','yuv420p','-g',"$([int]$fps * 2)",$temp)
        $tone = 'aevalsrc=0.16*sin(2*PI*660*t)*lt(mod(t\,1)\,0.10):s=48000'
        Encode @('-y','-stream_loop','29','-i',$temp,'-f','lavfi','-i',$tone,'-t','300','-map','0:v','-map','1:a','-c:v','copy',
            '-c:a','aac','-b:a','64k','-movflags','+faststart',(Join-Path $dest $name))
    }
} finally {
    if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp }
}
