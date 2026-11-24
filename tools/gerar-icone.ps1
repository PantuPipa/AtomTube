param([Parameter(Mandatory=$true)][string]$Path)
$ErrorActionPreference = 'Stop'
$sizes = @(16, 32, 48)
$file = [IO.BinaryWriter]::new([IO.File]::Create($Path))
try {
    $file.Write([uint16]0)
    $file.Write([uint16]1)
    $file.Write([uint16]$sizes.Count)
    $offset = 6 + 16 * $sizes.Count
    foreach ($size in $sizes) {
        $bytes = 40 + 4 * $size * $size + 4 * [math]::Ceiling($size / 32) * $size
        $file.Write([byte]$size)
        $file.Write([byte]$size)
        $file.Write([uint16]0)
        $file.Write([uint16]1)
        $file.Write([uint16]32)
        $file.Write([uint32]$bytes)
        $file.Write([uint32]$offset)
        $offset += $bytes
    }
    foreach ($size in $sizes) {
        $file.Write([uint32]40)
        $file.Write([int]$size)
        $file.Write([int](2 * $size))
        $file.Write([uint16]1)
        $file.Write([uint16]32)
        $file.Write([byte[]]::new(24))
        for ($y=0; $y -lt $size; $y++) {
            for ($x=0; $x -lt $size; $x++) {
                $u=($x+0.5)/$size
                $v=($y+0.5)/$size
                $play=($u -ge 0.375) -and ($u -le 0.75) -and ([math]::Abs($v-0.5) -le (0.75-$u)*2/3)
                if ($play) { $file.Write([byte[]]@(31,32,12,255)) }
                else { $file.Write([byte[]]@(196,225,100,255)) }
            }
        }
        $file.Write([byte[]]::new(4 * [math]::Ceiling($size / 32) * $size))
    }
} finally { $file.Dispose() }
