param(
  [Parameter(Mandatory = $true)]
  [string]$SourcePath,

  [Parameter(Mandatory = $true)]
  [string]$OutPath
)

$source = Get-Content -Raw $SourcePath
$glyphDecl = [regex]::Match($source, 'SONY_ERICSSON_T230_FONT_GLYPHS\[\]\s*=\s*".*";').Value
$glyphLine = $glyphDecl.Substring($glyphDecl.IndexOf('"') + 1)
$glyphLine = $glyphLine.Substring(0, $glyphLine.LastIndexOf('"'))
$glyphChars = New-Object System.Collections.Generic.List[char]
for ($i = 0; $i -lt $glyphLine.Length; ++$i)
{
  if ($glyphLine[$i] -eq '\' -and ($i + 1) -lt $glyphLine.Length)
  {
    ++$i
  }
  [void]$glyphChars.Add($glyphLine[$i])
}

$bitmapMatches = [regex]::Matches($source, '\{\s*((?:0x[0-9A-Fa-f]{2},?\s*){12})\}')
$bitmaps = New-Object System.Collections.Generic.List[object]
foreach ($match in $bitmapMatches)
{
  $rows = [regex]::Matches($match.Groups[1].Value, '0x([0-9A-Fa-f]{2})') |
    ForEach-Object { [Convert]::ToByte($_.Groups[1].Value, 16) }
  [void]$bitmaps.Add(@($rows))
}

if ($glyphChars.Count -ne $bitmaps.Count)
{
  throw "Glyph count ($($glyphChars.Count)) does not match bitmap count ($($bitmaps.Count))."
}

$sourceGlyphs = @{}
for ($i = 0; $i -lt $glyphChars.Count; ++$i)
{
  $sourceGlyphs[[int][char]$glyphChars[$i]] = $bitmaps[$i]
}

function New-FallbackRows([char]$ch)
{
  switch ($ch)
  {
    ' ' { return @(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) }
    '$' { return @(0, 0x10, 0x38, 0x50, 0x50, 0x38, 0x14, 0x14, 0x38, 0x10, 0, 0) }
    '<' { return @(0, 0, 0x08, 0x10, 0x20, 0x40, 0x20, 0x10, 0x08, 0, 0, 0) }
    '>' { return @(0, 0, 0x40, 0x20, 0x10, 0x08, 0x10, 0x20, 0x40, 0, 0, 0) }
    '^' { return @(0, 0, 0x10, 0x28, 0x44, 0, 0, 0, 0, 0, 0, 0) }
    '`' { return @(0, 0, 0x40, 0x20, 0, 0, 0, 0, 0, 0, 0, 0) }
    '{' { return @(0, 0x18, 0x20, 0x20, 0x20, 0x40, 0x20, 0x20, 0x20, 0x18, 0, 0) }
    '|' { return @(0, 0, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0, 0, 0) }
    '}' { return @(0, 0x60, 0x10, 0x10, 0x10, 0x08, 0x10, 0x10, 0x10, 0x60, 0, 0) }
    '~' { return @(0, 0, 0, 0x48, 0xB0, 0, 0, 0, 0, 0, 0, 0) }
    default { return @(0, 0, 0x30, 0x48, 0x48, 0x08, 0x10, 0x10, 0, 0x10, 0, 0) }
  }
}

function Convert-GlyphRows([byte[]]$rows)
{
  $minX = 8
  $maxX = -1
  $minY = 12
  $maxY = -1

  for ($y = 0; $y -lt 12; ++$y)
  {
    for ($x = 0; $x -lt 8; ++$x)
    {
      if (($rows[$y] -band (0x80 -shr $x)) -ne 0)
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
    return [pscustomobject]@{
      Width = 0
      Height = 0
      Top = 0
      Advance = 4
      Rows = @(0) * 14
    }
  }

  $width = $maxX - $minX + 1
  $height = $maxY - $minY + 1
  $outRows = @(0) * 14
  for ($row = 0; $row -lt $height; ++$row)
  {
    $bits = 0
    for ($x = 0; $x -lt $width; ++$x)
    {
      if (($rows[$minY + $row] -band (0x80 -shr ($minX + $x))) -ne 0)
      {
        $bits = $bits -bor (1 -shl $x)
      }
    }
    $outRows[$row] = $bits
  }

  return [pscustomobject]@{
    Width = $width
    Height = $height
    Top = $minY
    Advance = [Math]::Min([Math]::Max($width + 1, 4), 9)
    Rows = $outRows
  }
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add('#ifndef MVM_SYSTEM_FONT_T230_H')
$lines.Add('#define MVM_SYSTEM_FONT_T230_H')
$lines.Add('')
$lines.Add('#include <stdint.h>')
$lines.Add('')
$lines.Add('/* Generated from the Sony Ericsson T230 screenshot-derived bitmap sheet. */')
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
$lines.Add('static const MVM_SystemGlyph_t MVM_SystemFontSmallCandidate[95] =')
$lines.Add('{')

for ($code = 32; $code -le 126; ++$code)
{
  if ($sourceGlyphs.ContainsKey($code))
  {
    $glyph = Convert-GlyphRows ([byte[]]$sourceGlyphs[$code])
  }
  else
  {
    $glyph = Convert-GlyphRows ([byte[]](New-FallbackRows ([char]$code)))
  }

  $rowText = ($glyph.Rows | ForEach-Object { '0x{0:X4}u' -f $_ }) -join ', '
  $lines.Add(('  {{ {0}u, {1}u, {2}u, {3}u, {{ {4} }} }},' -f $glyph.Width, $glyph.Height, $glyph.Top, $glyph.Advance, $rowText))
}

$lines.Add('};')
$lines.Add('')
$lines.Add('typedef struct MVM_SystemFontFace_t')
$lines.Add('{')
$lines.Add('  uint8_t nominal_width;')
$lines.Add('  uint8_t nominal_height;')
$lines.Add('  const MVM_SystemGlyph_t *glyphs;')
$lines.Add('} MVM_SystemFontFace_t;')
$lines.Add('')
$lines.Add('/*')
$lines.Add(' * Current screenshot-derived atlas is closer to a small Sony Ericsson')
$lines.Add(' * system-font candidate than to the final normal face. Keep the normal')
$lines.Add(' * mapping explicit as a placeholder until a reference-matched normal')
$lines.Add(' * atlas is reconstructed.')
$lines.Add(' */')
$lines.Add('static const MVM_SystemFontFace_t MVM_SystemFontFaceNormalPlaceholder =')
$lines.Add('{')
$lines.Add('  7u,')
$lines.Add('  10u,')
$lines.Add('  MVM_SystemFontSmallCandidate')
$lines.Add('};')
$lines.Add('')
$lines.Add('static const MVM_SystemFontFace_t MVM_SystemFontFaceSmallCandidate =')
$lines.Add('{')
$lines.Add('  6u,')
$lines.Add('  9u,')
$lines.Add('  MVM_SystemFontSmallCandidate')
$lines.Add('};')
$lines.Add('')
$lines.Add('#endif')

[System.IO.File]::WriteAllLines((Join-Path (Get-Location) $OutPath), $lines)
