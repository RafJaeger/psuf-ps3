<#
Fit the supplied artwork without cropping, stretching, or redrawing it.
ICON1 uses ravenDS/PAMFtool (GPL-3.0, https://github.com/ravenDS/PAMFtool)
and FFmpeg/ffprobe: native executables or a local Docker image containing both.
No downloads, installations, PATH changes, or proprietary SDK are used.
External tools and validation logs stay beside the checkout, not inside it.
Example: .\tools\build_ui_assets.ps1 -IconSource 'icon.gif' -BannerSource 'banner.png'
-BrandOnly skips font generation, as in the original script.
To build only the wide home logo: -HomeSource 'home.gif' -BrandOnly
#>
[CmdletBinding()]
param(
    [string]$IconSource,
    [string]$BannerSource,
    [string]$HomeSource,
    [switch]$BrandOnly,
    [string]$WorkDirectory,
    [string]$PamfToolPath,
    [string]$FfmpegPath,
    [string]$FfprobePath,
    [string]$DockerImage = 'psuf-ui-assets-ffmpeg:local'
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$root = Split-Path $PSScriptRoot -Parent
$dest = Join-Path $root 'release/USRDIR'
if (!$WorkDirectory) { $WorkDirectory = Join-Path (Split-Path $root -Parent) 'tmp-icon-animation' }
$WorkDirectory = [IO.Path]::GetFullPath($WorkDirectory)
if ($WorkDirectory.StartsWith($root.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'WorkDirectory must be outside the repository.'
}
if (!$PamfToolPath) { $PamfToolPath = Join-Path $WorkDirectory 'PAMFtool-win-x64.exe' }
foreach ($path in @($IconSource, $BannerSource, $HomeSource)) {
    if ($path -and !(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Artwork not found: $path" }
}
if ($IconSource) {
    if (!(Test-Path -LiteralPath $PamfToolPath -PathType Leaf)) { throw "Open-source PAMFtool required: $PamfToolPath" }
    if ($FfmpegPath -or $FfprobePath) {
        if (!$FfmpegPath -or !$FfprobePath) { throw 'Supply both FfmpegPath and FfprobePath.' }
        foreach ($tool in @($FfmpegPath, $FfprobePath)) { Get-Command $tool -ErrorAction Stop | Out-Null }
    } else {
        Get-Command docker -ErrorAction Stop | Out-Null
        & docker image inspect $DockerImage *> $null
        if ($LASTEXITCODE -ne 0) { throw "Docker image '$DockerImage' must already contain ffmpeg and ffprobe." }
    }
}
$run = Join-Path $WorkDirectory ('build-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $run -Force | Out-Null

# GDI+ stores BGRA; UIB1/UIA1 store A,R,G,B bytes, independent of endianness.
if (!('PsufUiAssetPixels' -as [type])) {
    $drawingReferences = @([Drawing.Bitmap].Assembly.Location, [Drawing.Color].Assembly.Location) | Select-Object -Unique
    if ($PSVersionTable.PSEdition -eq 'Core') {
        $drawingReferences += (Join-Path $PSHOME 'ref/System.Runtime.dll'), (Join-Path $PSHOME 'ref/System.Runtime.InteropServices.dll')
    }
    Add-Type -ReferencedAssemblies $drawingReferences -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public static class PsufUiAssetPixels {
    public static byte[] Argb(Bitmap bitmap) {
        Rectangle rect = new Rectangle(0, 0, bitmap.Width, bitmap.Height);
        BitmapData data = bitmap.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        try {
            byte[] result = new byte[bitmap.Width * bitmap.Height * 4];
            byte[] row = new byte[bitmap.Width * 4];
            for (int y = 0; y < bitmap.Height; y++) {
                Marshal.Copy(IntPtr.Add(data.Scan0, y * data.Stride), row, 0, row.Length);
                for (int x = 0; x < bitmap.Width; x++) {
                    int s = x * 4, d = (y * bitmap.Width + x) * 4;
                    result[d] = row[s + 3]; result[d + 1] = row[s + 2];
                    result[d + 2] = row[s + 1]; result[d + 3] = row[s];
                }
            }
            return result;
        } finally { bitmap.UnlockBits(data); }
    }
    public static byte[] Mask(Bitmap bitmap, int width, int height) {
        byte[] argb = Argb(bitmap), result = new byte[width * height];
        for (int y = 0; y < height; y++) for (int x = 0; x < width; x++)
            result[y * width + x] = argb[(y * bitmap.Width + x) * 4 + 1];
        return result;
    }
}
'@
}
function Fit-Art([Drawing.Image]$Source, [int]$Width, [int]$Height, [Drawing.Color]$Background) {
    $target = [Drawing.Bitmap]::new($Width, $Height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [Drawing.Graphics]::FromImage($target)
    $attributes = [Drawing.Imaging.ImageAttributes]::new()
    try {
        $g.Clear($Background)
        $g.CompositingQuality = [Drawing.Drawing2D.CompositingQuality]::HighQuality
        $g.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $attributes.SetWrapMode([Drawing.Drawing2D.WrapMode]::TileFlipXY)
        $scale = [Math]::Min($Width / [double]$Source.Width, $Height / [double]$Source.Height)
        $w = [single]($Source.Width * $scale); $h = [single]($Source.Height * $scale)
        $rect = [Drawing.RectangleF]::new([single](($Width-$w)/2), [single](($Height-$h)/2), $w, $h)
        $points = [Drawing.PointF[]]@(
            [Drawing.PointF]::new($rect.Left,$rect.Top),
            [Drawing.PointF]::new($rect.Right,$rect.Top),
            [Drawing.PointF]::new($rect.Left,$rect.Bottom)
        )
        $sourceRect = [Drawing.RectangleF]::new(0,0,$Source.Width,$Source.Height)
        $g.DrawImage($Source, $points, $sourceRect, [Drawing.GraphicsUnit]::Pixel, $attributes)
    } finally { $attributes.Dispose(); $g.Dispose() }
    return $target
}
function Invoke-Media([string]$Tool, [string[]]$ToolArguments) {
    if ($FfmpegPath) {
        $exe = if ($Tool -eq 'ffmpeg') { $FfmpegPath } else { $FfprobePath }
        $output = & $exe @ToolArguments 2>&1
    } else {
        $output = & docker run --rm -v "${run}:/work" -w /work $DockerImage $Tool @ToolArguments 2>&1
    }
    if ($LASTEXITCODE -ne 0) { throw "$Tool failed: $($output -join [Environment]::NewLine)" }
    return $output
}
function Invoke-Pamf([string[]]$ToolArguments) {
    $output = & $PamfToolPath @ToolArguments 2>&1
    if ($LASTEXITCODE -ne 0) { throw "PAMFtool failed: $($output -join [Environment]::NewLine)" }
    return $output
}
$outputs = @()
if ($HomeSource) {
    $source = [Drawing.Image]::FromFile([IO.Path]::GetFullPath($HomeSource))
    $writer = $null
    try {
        $dimension = [Drawing.Imaging.FrameDimension]::Time
        if ($source.FrameDimensionsList -notcontains $dimension.Guid) { throw 'HomeSource must be an animated GIF.' }
        $count = $source.GetFrameCount($dimension)
        if ($count -lt 2 -or $count -gt 64) { throw 'Home animation requires 2..64 frames.' }
        $expectedBytes = 16+4*$count+512*128*4*$count
        if ($expectedBytes -gt 8MB) { throw 'Home UIA1 exceeds the app loader limit.' }
        $delayBytes = $source.GetPropertyItem(0x5100).Value
        if ($delayBytes.Length -lt 4*$count) { throw 'Home GIF frame delays are incomplete.' }
        $writer = [IO.BinaryWriter]::new([IO.File]::Create((Join-Path $run 'home.anim')))
        $writer.Write([Text.Encoding]::ASCII.GetBytes('UIA1'))
        $writer.Write([uint32]512); $writer.Write([uint32]128); $writer.Write([uint32]$count)
        for ($frame=0; $frame -lt $count; $frame++) {
            $delay = [uint32](10*[BitConverter]::ToUInt32($delayBytes,4*$frame))
            if ($delay -lt 30) { throw 'Home GIF delays must be at least 30 ms to preserve loader timing.' }
            $writer.Write($delay)
        }
        for ($frame=0; $frame -lt $count; $frame++) {
            $source.SelectActiveFrame($dimension,$frame) | Out-Null
            $image = Fit-Art $source 512 128 ([Drawing.Color]::Black)
            try { $writer.Write([PsufUiAssetPixels]::Argb($image)) }
            finally { $image.Dispose() }
        }
    } finally { if ($writer) { $writer.Dispose() }; $source.Dispose() }
    if ((Get-Item -LiteralPath (Join-Path $run 'home.anim')).Length -ne $expectedBytes) { throw 'Invalid home UIA1 payload length.' }
    $outputs += 'home.anim'
}
if ($IconSource) {
    $source = [Drawing.Image]::FromFile([IO.Path]::GetFullPath($IconSource))
    $animWriter = $null
    try {
        $dimension = [Drawing.Imaging.FrameDimension]::Time
        if ($source.FrameDimensionsList -notcontains $dimension.Guid) { throw 'IconSource must be an animated GIF.' }
        $frameCount = $source.GetFrameCount($dimension)
        if ($frameCount -lt 2 -or $frameCount -gt 64) { throw 'Animation requires 2..64 frames.' }
        $delaysProperty = $source.GetPropertyItem(0x5100).Value
        if ($delaysProperty.Length -lt 4*$frameCount) { throw 'GIF frame delays are incomplete.' }
        $delays = @(); $videoFrames = 0
        for ($frame=0; $frame -lt $frameCount; $frame++) {
            $delay = [int]([BitConverter]::ToUInt32($delaysProperty,4*$frame)*10)
            # 25 fps is a standard PAMF rate: duplicate frames, preserve timing.
            if ($delay -le 0 -or $delay % 40) { throw 'PAMF at 25 fps requires GIF delays divisible by 40 ms.' }
            $delays += $delay; $videoFrames += [int]($delay/40)
        }
        $animBytes = 16 + 4*$frameCount + 320*176*4*$frameCount
        if ($animBytes -gt 8MB) { throw 'UIA1 exceeds the app loader limit.' }
        $animWriter = [IO.BinaryWriter]::new([IO.File]::Create((Join-Path $run 'brand.anim')))
        $animWriter.Write([Text.Encoding]::ASCII.GetBytes('UIA1'))
        $animWriter.Write([uint32]320); $animWriter.Write([uint32]176); $animWriter.Write([uint32]$frameCount)
        foreach ($delay in $delays) { $animWriter.Write([uint32]$delay) }
        $sequence = Join-Path $run 'frames'; New-Item -ItemType Directory -Path $sequence | Out-Null
        $streamDir = Join-Path $run 'streams'; New-Item -ItemType Directory -Path $streamDir | Out-Null
        $videoIndex = 0
        for ($frame=0; $frame -lt $frameCount; $frame++) {
            $source.SelectActiveFrame($dimension,$frame) | Out-Null
            # The app renderer ignores alpha, so every stored pixel is opaque.
            $logo = Fit-Art $source 320 176 ([Drawing.Color]::Black)
            try {
                $pixels = [PsufUiAssetPixels]::Argb($logo)
                $animWriter.Write([byte[]]$pixels)
                if ($frame -eq 0) {
                    $brandWriter = [IO.BinaryWriter]::new([IO.File]::Create((Join-Path $run 'brand.argb')))
                    try {
                        $brandWriter.Write([Text.Encoding]::ASCII.GetBytes('UIB1'))
                        $brandWriter.Write([uint32]320); $brandWriter.Write([uint32]176)
                        $brandWriter.Write([byte[]]$pixels)
                    } finally { $brandWriter.Dispose() }
                    $tile = Fit-Art $source 320 176 ([Drawing.Color]::Transparent)
                    try { $tile.Save((Join-Path $run 'ICON0.PNG'), [Drawing.Imaging.ImageFormat]::Png) }
                    finally { $tile.Dispose() }
                }
            } finally { $logo.Dispose() }
            # PAMF video has no alpha: composite the identical fitted art on black.
            $tile = Fit-Art $source 320 176 ([Drawing.Color]::Black)
            try {
                $first = Join-Path $sequence ('frame-{0:D4}.png' -f $videoIndex)
                $tile.Save($first, [Drawing.Imaging.ImageFormat]::Png); $videoIndex++
                for ($copy=1; $copy -lt ($delays[$frame]/40); $copy++) {
                    Copy-Item -LiteralPath $first -Destination (Join-Path $sequence ('frame-{0:D4}.png' -f $videoIndex))
                    $videoIndex++
                }
            } finally { $tile.Dispose() }
        }
    } finally { if ($animWriter) { $animWriter.Dispose() }; $source.Dispose() }
    if ((Get-Item -LiteralPath (Join-Path $run 'brand.anim')).Length -ne $animBytes -or
        (Get-Item -LiteralPath (Join-Path $run 'brand.argb')).Length -ne (12+320*176*4)) {
        throw 'Invalid UIA1/UIB1 payload length.'
    }
    # Release the validated app assets for visual QA while PAMF is encoded.
    foreach ($name in @('brand.argb','brand.anim')) {
        Copy-Item -LiteralPath (Join-Path $run $name) -Destination (Join-Path $dest $name) -Force
    }
    Write-Host "App brand ready: 176x176, $frameCount frames, $($delays -join ',') ms."
    Push-Location $run
    try {
        Invoke-Media 'ffmpeg' @('-hide_banner','-loglevel','error','-y','-framerate','25','-i','frames/frame-%04d.png',
            '-vf','loop=loop=4:size=32:start=0,fps=30000/1001:round=near,scale=iw:ih:out_color_matrix=bt709:out_range=tv,setsar=1',
            '-frames:v','192','-an','-c:v','libx264','-profile:v','main','-level:v','2.1','-pix_fmt','yuv420p','-preset','slow',
            '-b:v','2000k','-maxrate','4000k','-bufsize','4000k','-g','15','-keyint_min','15','-bf','0','-refs','2',
            '-color_range','tv','-colorspace','bt709','-color_primaries','bt709','-color_trc','bt709',
            '-x264-params','cabac=0:no-deblock=1:weightp=0:weightb=0:8x8dct=0:mbtree=0:open-gop=0:slices=4:nal-hrd=vbr:force-cfr=1:aud=1:repeat-headers=1:scenecut=0',
            '-f','h264','streams/icon.s00_avc.264') | Out-Null
        $avcPath = Join-Path $streamDir 'icon.s00_avc.264'
        $bytes = [IO.File]::ReadAllBytes($avcPath)
        for ($i=0; $i -lt $bytes.Length-7; $i++) {
            if ($bytes[$i] -eq 0 -and $bytes[$i+1] -eq 0 -and $bytes[$i+2] -eq 1 -and
                ($bytes[$i+3] -band 31) -eq 7 -and $bytes[$i+4] -eq 66) {
                $bytes[$i+4]=77; $bytes[$i+5]=0
            }
        }
        [IO.File]::WriteAllBytes($avcPath,$bytes)
        Invoke-Pamf @('-mux',$streamDir,(Join-Path $run 'ICON1.PAM'),'-pstd','529','-mmb','1','-nodeblock') | Set-Content -LiteralPath 'pamf-mux.log'
        $pam = [IO.File]::ReadAllBytes((Join-Path $run 'ICON1.PAM'))
        if ($pam.Length -gt 2400000 -or $pam.Length -lt 4096 -or [Text.Encoding]::ASCII.GetString($pam,0,4) -ne 'PAMF') {
            throw 'ICON1.PAM must be a real PAMF file no larger than 2,400,000 bytes.'
        }
        Invoke-Pamf @('-info',(Join-Path $run 'ICON1.PAM')) | Set-Content -LiteralPath 'pamf-info.log'
        $demux = Join-Path $run 'demux'
        Invoke-Pamf @('-demux',(Join-Path $run 'ICON1.PAM'),$demux) | Set-Content -LiteralPath 'pamf-demux.log'
        $avc = @(Get-ChildItem -LiteralPath $demux -File | Where-Object { $_.Extension -in '.264','.h264','.avc' })
        if ($avc.Count -ne 1) { throw 'Expected exactly one demuxed AVC video stream.' }
        $originalHash = (Get-FileHash -LiteralPath (Join-Path $streamDir 'icon.s00_avc.264') -Algorithm SHA256).Hash
        if ((Get-FileHash -LiteralPath $avc[0].FullName -Algorithm SHA256).Hash -ne $originalHash) {
            throw 'PAMF demuxed AVC differs from the encoded stream.'
        }
        $avcRelative = 'demux/' + $avc[0].Name
        $probeText = Invoke-Media 'ffprobe' @('-v','error','-select_streams','v:0','-count_frames',
            '-show_entries','stream=codec_name,profile,level,width,height,pix_fmt,r_frame_rate,nb_read_frames',
            '-of','json',$avcRelative)
        $probeText | Set-Content -LiteralPath 'ffprobe.json'
        $probe = ($probeText -join "`n" | ConvertFrom-Json).streams[0]
        if ($probe.codec_name -ne 'h264' -or $probe.width -ne 320 -or $probe.height -ne 176 -or
            $probe.pix_fmt -ne 'yuv420p' -or $probe.level -ne 21 -or
            $probe.r_frame_rate -ne '30000/1001' -or [int]$probe.nb_read_frames -ne 192) {
            throw 'Demuxed PAMF video dimensions, frame count, or timing are invalid.'
        }
        Invoke-Media 'ffmpeg' @('-hide_banner','-loglevel','error','-xerror','-i',$avcRelative,'-f','null','-') | Out-Null
    } finally { Pop-Location }
    $outputs += 'ICON0.PNG','ICON1.PAM'
}
if ($BannerSource) {
    $source = [Drawing.Image]::FromFile([IO.Path]::GetFullPath($BannerSource))
    try {
        $banner = Fit-Art $source 1920 1080 ([Drawing.Color]::Black)
        try { $banner.Save((Join-Path $run 'PIC1.PNG'), [Drawing.Imaging.ImageFormat]::Png) }
        finally { $banner.Dispose() }
    } finally { $source.Dispose() }
    $outputs += 'PIC1.PNG'
}
if (!$BrandOnly) {
    $writer = [IO.BinaryWriter]::new([IO.File]::Create((Join-Path $run 'font.uif')))
    $format = [Drawing.StringFormat]::GenericTypographic.Clone()
    try {
        $writer.Write([Text.Encoding]::ASCII.GetBytes('UIF1')); $writer.Write([uint32]4)
        $format.FormatFlags = $format.FormatFlags -bor [Drawing.StringFormatFlags]::MeasureTrailingSpaces
        foreach ($size in @(14,20,30,40)) {
            $font = [Drawing.Font]::new('Segoe UI', [single]$size, [Drawing.FontStyle]::Bold, [Drawing.GraphicsUnit]::Pixel)
            try {
                if ($font.FontFamily.Name -ne 'Segoe UI' -or !$font.Bold) { throw 'Segoe UI Bold is required; font substitution is not allowed.' }
                for ($code=32; $code -le 255; $code++) {
                    $bmp = [Drawing.Bitmap]::new(64,64,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
                    $g = [Drawing.Graphics]::FromImage($bmp)
                    try {
                        $g.Clear([Drawing.Color]::Black)
                        $g.TextRenderingHint = [Drawing.Text.TextRenderingHint]::AntiAliasGridFit
                        $glyph = [string][char]$code
                        $advance = [int][Math]::Ceiling($g.MeasureString($glyph,$font,100,$format).Width)
                        if ($advance -gt 64) { throw "Glyph $code at $size px exceeds UIF1 advance limit." }
                        $g.DrawString($glyph,$font,[Drawing.Brushes]::White,[single]0,[single]0,$format)
                        $w = [Math]::Min(64,[Math]::Max(1,$advance+2)); $h = [Math]::Min(64,$size+12)
                        $writer.Write([byte]$w); $writer.Write([byte]$h); $writer.Write([byte]$advance); $writer.Write([byte]0)
                        $writer.Write([PsufUiAssetPixels]::Mask($bmp,$w,$h))
                    } finally { $g.Dispose(); $bmp.Dispose() }
                }
            } finally { $font.Dispose() }
        }
    } finally { $writer.Dispose(); $format.Dispose() }
    $outputs += 'font.uif'
}
# Publish XMB assets only after PAMF round-trip checks succeeded.
foreach ($name in $outputs) {
    $target = if ($name -in 'ICON0.PNG','ICON1.PAM','PIC1.PNG') { Join-Path $root "release/$name" } else { Join-Path $dest $name }
    Copy-Item -LiteralPath (Join-Path $run $name) -Destination $target -Force
    Get-Item -LiteralPath $target | Select-Object FullName,Length
}
Write-Host "Validation logs and intermediate files: $run"
