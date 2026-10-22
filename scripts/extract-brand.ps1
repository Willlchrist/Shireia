Add-Type -AssemblyName System.Drawing
$source = [System.Drawing.Bitmap]::new((Join-Path $PSScriptRoot '..\assets\references\settings.png'))
$crop = [System.Drawing.Bitmap]::new(54, 52, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
for ($y = 0; $y -lt 52; $y++) {
    for ($x = 0; $x -lt 54; $x++) {
        $pixel = $source.GetPixel($x + 54, $y + 30)
        $strength = [Math]::Max(0, [Math]::Min(255, [int](($pixel.R + $pixel.B - 2 * $pixel.G) * 2)))
        $crop.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($strength, $pixel.R, $pixel.G, $pixel.B))
    }
}
$asset = Join-Path $PSScriptRoot '..\assets\brand'
New-Item -ItemType Directory -Force -Path $asset | Out-Null
$png = Join-Path $asset 'butterfly.png'
$crop.Save($png, [System.Drawing.Imaging.ImageFormat]::Png)
$sizes = @(16, 24, 32, 48, 64, 128, 256)
$images = @()
foreach ($size in $sizes) {
    $bitmap = [System.Drawing.Bitmap]::new($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.DrawImage($crop, 0, 0, $size, $size)
    $graphics.Dispose()
    $stream = [System.IO.MemoryStream]::new()
    $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
    $images += ,$stream.ToArray()
    $stream.Dispose(); $bitmap.Dispose()
}
$stream = [System.IO.File]::Create((Join-Path $asset 'shireia.ico'))
$writer = [System.IO.BinaryWriter]::new($stream)
$writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$sizes.Count)
$offset = 6 + $sizes.Count * 16
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $writer.Write([byte]($sizes[$i] % 256)); $writer.Write([byte]($sizes[$i] % 256))
    $writer.Write([byte]0); $writer.Write([byte]0)
    $writer.Write([uint16]1); $writer.Write([uint16]32)
    $writer.Write([uint32]$images[$i].Length); $writer.Write([uint32]$offset)
    $offset += $images[$i].Length
}
foreach ($data in $images) { $writer.Write([byte[]]$data) }
$writer.Dispose(); $crop.Dispose(); $source.Dispose()
