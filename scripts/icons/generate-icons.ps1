param(
    [string]$OutDir = (Join-Path $PSScriptRoot "..\..\resources\icons"),
    [string]$AssetsDir = (Join-Path $PSScriptRoot "..\..\packaging\msix\Assets")
)

Add-Type -AssemblyName System.Drawing

$ErrorActionPreference = "Stop"

New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

$teal = [System.Drawing.Color]::FromArgb(255, 20, 98, 84)
$red  = [System.Drawing.Color]::FromArgb(255, 229, 72, 77)
$white = [System.Drawing.Color]::FromArgb(255, 255, 255, 255)

function New-RoundedRectPath {
    param([float]$x, [float]$y, [float]$w, [float]$h, [float]$radius)
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $radius * 2
    $path.AddArc($x, $y, $d, $d, 180, 90)
    $path.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $path.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
    $path.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $path.CloseFigure()
    return $path
}

function Draw-MicGlyph {
    param(
        [System.Drawing.Graphics]$g,
        [float]$cx,
        [float]$cy,
        [float]$scale,
        [System.Drawing.Color]$color
    )

    $brush = New-Object System.Drawing.SolidBrush($color)
    $penWidth = [Math]::Max(1.0, $scale * 0.09)
    $pen = New-Object System.Drawing.Pen($color, $penWidth)
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round

    # Mic capsule (body)
    $bodyW = $scale * 0.34
    $bodyH = $scale * 0.62
    $bodyX = $cx - $bodyW / 2
    $bodyY = $cy - $scale * 0.46
    $bodyPath = New-RoundedRectPath -x $bodyX -y $bodyY -w $bodyW -h $bodyH -radius ($bodyW / 2)
    $g.FillPath($brush, $bodyPath)

    # Stand (arc cradle under the capsule)
    $standR = $scale * 0.30
    $standRect = New-Object System.Drawing.RectangleF(($cx - $standR), ($bodyY + $bodyH * 0.42), ($standR * 2), ($standR * 2))
    $g.DrawArc($pen, $standRect, 20, 140)

    # Stem + base
    $stemTop = $bodyY + $bodyH * 0.42 + $standR
    $stemBottom = $cy + $scale * 0.46
    $g.DrawLine($pen, $cx, $stemTop, $cx, $stemBottom)

    $baseHalf = $scale * 0.16
    $g.DrawLine($pen, ($cx - $baseHalf), $stemBottom, ($cx + $baseHalf), $stemBottom)

    $pen.Dispose()
    $brush.Dispose()
    $bodyPath.Dispose()
}

function New-GlyphBitmap {
    param(
        [int]$size,
        [System.Drawing.Color]$bg,
        [System.Drawing.Color]$glyph,
        [bool]$circle
    )

    $bmp = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)

    $bgBrush = New-Object System.Drawing.SolidBrush($bg)
    if ($circle) {
        $g.FillEllipse($bgBrush, 0, 0, $size, $size)
    } else {
        $radius = $size * 0.22
        $path = New-RoundedRectPath -x 0 -y 0 -w $size -h $size -radius $radius
        $g.FillPath($bgBrush, $path)
        $path.Dispose()
    }
    $bgBrush.Dispose()

    Draw-MicGlyph -g $g -cx ($size / 2) -cy ($size / 2) -scale ($size * 0.62) -color $glyph

    $g.Dispose()
    return $bmp
}

function Save-Ico {
    param(
        [System.Drawing.Bitmap[]]$Bitmaps,
        [string]$Path
    )

    $images = @()
    foreach ($bmp in $Bitmaps) {
        $ms = New-Object System.IO.MemoryStream
        $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
        $images += ,@{ Size = $bmp.Width; Bytes = $ms.ToArray() }
        $ms.Dispose()
    }

    $fs = New-Object System.IO.FileStream($Path, [System.IO.FileMode]::Create)
    $bw = New-Object System.IO.BinaryWriter($fs)

    $bw.Write([UInt16]0)      # reserved
    $bw.Write([UInt16]1)      # type = icon
    $bw.Write([UInt16]$images.Count)

    $headerSize = 6 + (16 * $images.Count)
    $offset = $headerSize

    foreach ($img in $images) {
        $wByte = if ($img.Size -ge 256) { 0 } else { $img.Size }
        $bw.Write([Byte]$wByte)   # width
        $bw.Write([Byte]$wByte)   # height
        $bw.Write([Byte]0)        # color count
        $bw.Write([Byte]0)        # reserved
        $bw.Write([UInt16]1)      # planes
        $bw.Write([UInt16]32)     # bit count
        $bw.Write([UInt32]$img.Bytes.Length)
        $bw.Write([UInt32]$offset)
        $offset += $img.Bytes.Length
    }

    foreach ($img in $images) {
        $bw.Write($img.Bytes)
    }

    $bw.Flush()
    $bw.Dispose()
    $fs.Dispose()
}

$appSizes = @(16,24,32,48,64,128,256)
$traySizes = @(16,20,24,32,40,48,64)

# App icon: rounded square, teal, white mic glyph
$appBitmaps = $appSizes | ForEach-Object { New-GlyphBitmap -size $_ -bg $teal -glyph $white -circle $false }
Save-Ico -Bitmaps $appBitmaps -Path (Join-Path $OutDir "app.ico")

# Tray idle icon: teal circular badge, white mic glyph
$idleBitmaps = $traySizes | ForEach-Object { New-GlyphBitmap -size $_ -bg $teal -glyph $white -circle $true }
Save-Ico -Bitmaps $idleBitmaps -Path (Join-Path $OutDir "tray_idle.ico")

# Tray listening icon: red circular badge, white mic glyph
$listeningBitmaps = $traySizes | ForEach-Object { New-GlyphBitmap -size $_ -bg $red -glyph $white -circle $true }
Save-Ico -Bitmaps $listeningBitmaps -Path (Join-Path $OutDir "tray_listening.ico")

# MSIX assets (flat square teal, white glyph) at required sizes
$msixSpecs = @(
    @{ Name = "Square44x44Logo.png"; Size = 44 },
    @{ Name = "Square150x150Logo.png"; Size = 150 },
    @{ Name = "StoreLogo.png"; Size = 50 }
)
foreach ($spec in $msixSpecs) {
    $bmp = New-GlyphBitmap -size $spec.Size -bg $teal -glyph $white -circle $false
    $bmp.Save((Join-Path $AssetsDir $spec.Name), [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

foreach ($b in $appBitmaps) { $b.Dispose() }
foreach ($b in $idleBitmaps) { $b.Dispose() }
foreach ($b in $listeningBitmaps) { $b.Dispose() }

Write-Host "Icons written to $OutDir"
Write-Host "MSIX assets updated in $AssetsDir"
