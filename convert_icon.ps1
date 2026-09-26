Add-Type -AssemblyName System.Drawing

$pngPath = "$PSScriptRoot/Standalone/Assets/icon.png"
$icoPath = "$PSScriptRoot/Standalone/Assets/icon.ico"

if (-not (Test-Path $pngPath)) {
    Write-Error "icon.png not found at $pngPath"
    exit 1
}

$img = [System.Drawing.Image]::FromFile((Resolve-Path $pngPath).Path)

$sizes = @(16, 32, 48, 64, 128, 256)
$images = @()
$pngStreams = @()

foreach ($sz in $sizes) {
    $bmp = New-Object System.Drawing.Bitmap $sz, $sz
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.DrawImage($img, 0, 0, $sz, $sz)
    $g.Dispose()
    
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngStreams += $ms
    $images += $bmp
}

$stream = [System.IO.File]::Create((Resolve-Path "$PSScriptRoot/Standalone/Assets/").Path + "/icon.ico")
$writer = New-Object System.IO.BinaryWriter $stream

# ICONDIR Header
$writer.Write([UInt16]0)
$writer.Write([UInt16]1)
$writer.Write([UInt16]$images.Count)

$offset = 6 + ($images.Count * 16)
for ($i = 0; $i -lt $images.Count; $i++) {
    $sz = $sizes[$i]
    $w = if ($sz -ge 256) { 0 } else { $sz }
    $h = if ($sz -ge 256) { 0 } else { $sz }
    $len = $pngStreams[$i].Length

    $writer.Write([Byte]$w)
    $writer.Write([Byte]$h)
    $writer.Write([Byte]0)
    $writer.Write([Byte]0)
    $writer.Write([UInt16]1)
    $writer.Write([UInt16]32)
    $writer.Write([UInt32]$len)
    $writer.Write([UInt32]$offset)
    $offset += $len
}

for ($i = 0; $i -lt $images.Count; $i++) {
    $bytes = $pngStreams[$i].ToArray()
    $writer.Write($bytes)
}

$writer.Flush()
$stream.Close()
Write-Host "icon.ico created successfully!"
