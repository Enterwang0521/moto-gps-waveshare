$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Push-Location $repo
try {
    foreach ($size in @(16, 24, 48)) {
        & npx.cmd --yes lv_font_conv@1.5.3 --font third_party/lvgl/scripts/built_in_font/unscii-8.ttf --size $size --bpp 1 --format lvgl --no-compress --no-kerning --range '0x20-0x7E,0xB0' --lv-font-name "moto_pixel_$size" -o "platforms/smartring/assets/moto_pixel_$size.c"
        if ($LASTEXITCODE -ne 0) { throw "Pixel font generation failed at size $size" }
    }
} finally { Pop-Location }
