param(
  [Parameter(Mandatory = $true)]
  [string]$FontPath,

  [Parameter(Mandatory = $true)]
  [string]$OutPath,

  [int]$PixelSize = 12
)

Add-Type -AssemblyName System.Drawing

$collection = New-Object System.Drawing.Text.PrivateFontCollection
$collection.AddFontFile((Resolve-Path $FontPath))
$family = $collection.Families[0]
$font = New-Object System.Drawing.Font($family, $PixelSize, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
$lines = New-Object System.Collections.Generic.List[string]

$lines.Add('#ifndef MVM_SYSTEM_FONT_J220_H')
$lines.Add('#define MVM_SYSTEM_FONT_J220_H')
$lines.Add('')
$lines.Add('#include <stdint.h>')
$lines.Add('')
$lines.Add('typedef struct MVM_SystemGlyph_t')
$lines.Add('{')
$lines.Add('  uint8_t width;')
$lines.Add('  uint8_t height;')
$lines.Add('  uint8_t top;')
$lines.Add('  uint8_t advance;')
$lines.Add('  uint16_t rows[14];')
$lines.Add('} MVM_SystemGlyph_t;')
$lines.Add('')
$lines.Add('static const MVM_SystemGlyph_t MVM_SystemFontJ220[95] =')
$lines.Add('{')

for ($code = 32; $code -le 126; ++$code)
{
  $bitmap = New-Object System.Drawing.Bitmap 16, 16
  $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
  $graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::SingleBitPerPixelGridFit
  $graphics.Clear([System.Drawing.Color]::Black)
  $graphics.DrawString([string][char]$code, $font, [System.Drawing.Brushes]::White, -1, -2)

  $minX = 16
  $maxX = -1
  $minY = 16
  $maxY = -1
  for ($y = 0; $y -lt 16; ++$y)
  {
    for ($x = 0; $x -lt 16; ++$x)
    {
      if ($bitmap.GetPixel($x, $y).R -gt 0)
      {
        if ($x -lt $minX) { $minX = $x }
        if ($x -gt $maxX) { $maxX = $x }
        if ($y -lt $minY) { $minY = $y }
        if ($y -gt $maxY) { $maxY = $y }
      }
    }
  }

  if ($maxX -lt 0)
  {
    $width = 0
    $height = 0
    $top = 0
    $advance = 4
    $rows = @(0) * 14
  }
  else
  {
    $width = $maxX - $minX + 1
    $height = $maxY - $minY + 1
    $top = $minY
    $advance = [Math]::Min([Math]::Max($width + 1, 4), 11)
    $rows = New-Object uint16[] 14
    for ($row = 0; $row -lt $height -and $row -lt 14; ++$row)
    {
      $bits = 0
      for ($x = 0; $x -lt $width; ++$x)
      {
        if ($bitmap.GetPixel($minX + $x, $minY + $row).R -gt 0)
        {
          $bits = $bits -bor (1 -shl $x)
        }
      }
      $rows[$row] = [uint16]$bits
    }
  }

  $rowText = ($rows | ForEach-Object { '0x{0:X4}u' -f $_ }) -join ', '
  $lines.Add(('  {{ {0}u, {1}u, {2}u, {3}u, {{ {4} }} }},' -f $width, $height, $top, $advance, $rowText))

  $graphics.Dispose()
  $bitmap.Dispose()
}

$lines.Add('};')
$lines.Add('')
$lines.Add('#endif')

[System.IO.File]::WriteAllLines((Join-Path (Get-Location) $OutPath), $lines)

$font.Dispose()
$collection.Dispose()
