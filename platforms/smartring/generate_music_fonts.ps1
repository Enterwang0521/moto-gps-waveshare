# Generate the SmartRing music-page CJK fallback from LVGL's Source Han Sans SC.
# The two ranges keep each uncompressed LVGL bitmap below its 1 MiB limit.
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Push-Location $repo
try {
    $font = 'third_party/lvgl/scripts/built_in_font/SourceHanSansSC-Normal.otf'
    $common = @('--yes', 'lv_font_conv@1.5.3', '--font', $font, '--size', '16',
        '--bpp', '2', '--format', 'lvgl', '--no-compress', '--no-kerning')
    & npx.cmd @common --range '0x00A0-0x024F,0x2000-0x206F,0x2190-0x22FF,0x3000-0x30FF,0x4E00-0x77FF,0xFF00-0xFFEF' `
        --lv-font-name moto_music_font_16_a -o platforms/smartring/assets/moto_music_font_16_a.c
    if ($LASTEXITCODE -ne 0) { throw 'Generating the first music font failed' }
    & npx.cmd @common --range '0x7800-0x9FFF' `
        --lv-font-name moto_music_font_16_b -o platforms/smartring/assets/moto_music_font_16_b.c
    if ($LASTEXITCODE -ne 0) { throw 'Generating the second music font failed' }
}
finally { Pop-Location }
