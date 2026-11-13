<#
  Emperor Spice - copy the game's own files out of its archives, as a starting point for mods.

  Writes to  mods\_Original\<ARCHIVE>\...  next to EmperorSpice.exe. The launcher ignores mod
  folders starting with "_", so this is only a reference: copy the files you want to change into
  your own mod folder, e.g.  mods\MyMod\MODEL\Rules.txt
  For every file the newest version is taken (e.g. Rules.txt from the 1.09 patch, not from the CD).
  Our own generated archives (mods, large fonts) are skipped.

  Usage (or just double-click "Extract game files.cmd"):
    powershell -ExecutionPolicy Bypass -File Extract-GameFiles.ps1            # all except MAPS
    powershell -ExecutionPolicy Bypass -File Extract-GameFiles.ps1 -Archive UI,MODEL
    powershell -ExecutionPolicy Bypass -File Extract-GameFiles.ps1 -All       # including MAPS (~1 GB)
#>
param(
  [string[]]$Archive = @(),
  [switch]$All,
  [string]$GameDir = (Split-Path -Parent $PSScriptRoot)
)
$ErrorActionPreference = "Stop"

$dataDir = Join-Path $GameDir "data\DATA"
if (-not (Test-Path $dataDir)) { throw "Game data not found: $dataDir (install the game with EmperorSpice.exe first)" }
$outRoot = Join-Path $GameDir "mods\_Original"

$latin1 = [System.Text.Encoding]::GetEncoding(28591)
$marker = "EmperorSpice\ModPack.txt"
$ourFonts = @("textures\smallhd0.tga", "font\smallhd0.map")

function Read-Index([string]$rfh) {
  $d = [System.IO.File]::ReadAllBytes($rfh)
  $pos = 0; $list = @()
  while ($pos + 24 -le $d.Length) {
    $nameLen = [BitConverter]::ToUInt32($d, $pos)
    $e = [pscustomobject]@{
      Ts = [BitConverter]::ToUInt32($d, $pos + 4); Flags = [BitConverter]::ToUInt32($d, $pos + 8)
      Stored = [BitConverter]::ToUInt32($d, $pos + 12); Raw = [BitConverter]::ToUInt32($d, $pos + 16)
      Offset = [BitConverter]::ToUInt32($d, $pos + 20); Name = ""; Rfd = ""
    }
    $pos += 24
    if ($nameLen -eq 0 -or $pos + $nameLen -gt $d.Length) { break }
    $e.Name = $latin1.GetString($d, $pos, $nameLen).TrimEnd([char]0)
    $pos += $nameLen
    $list += $e
  }
  return $list
}

# archive prefixes = <PREFIX>0001.RFH
$prefixes = Get-ChildItem $dataDir -Filter "*0001.RFH" | ForEach-Object { $_.Name.Substring(0, $_.Name.Length - 8) }
if ($Archive.Count -gt 0) { $prefixes = $prefixes | Where-Object { $Archive -contains $_ } }
elseif (-not $All) { $prefixes = $prefixes | Where-Object { $_ -ne "MAPS" } }

foreach ($prefix in $prefixes) {
  # newest version of every entry over 0001, 0002, ... (the game stops at the first missing number)
  $newest = @{}
  for ($n = 1; $n -lt 100; $n++) {
    $rfh = Join-Path $dataDir ("{0}{1:D4}.RFH" -f $prefix, $n)
    $rfd = Join-Path $dataDir ("{0}{1:D4}.RFD" -f $prefix, $n)
    if (-not (Test-Path $rfh)) { break }
    $entries = Read-Index $rfh
    $names = $entries | ForEach-Object { $_.Name.ToLowerInvariant() }
    if ($names -contains $marker.ToLowerInvariant()) { continue }               # our mod archive
    if (@($names | Where-Object { $ourFonts -contains $_ }).Count -gt 0) { continue } # our font archive
    foreach ($e in $entries) {
      $e.Rfd = $rfd
      $key = $e.Name.ToLowerInvariant()
      if (-not $newest.ContainsKey($key) -or $e.Ts -ge $newest[$key].Ts) { $newest[$key] = $e }
    }
  }

  Write-Host ("{0}: {1} files" -f $prefix, $newest.Count)
  $streams = @{}
  try {
    foreach ($e in $newest.Values) {
      if (-not $streams.ContainsKey($e.Rfd)) { $streams[$e.Rfd] = [System.IO.File]::OpenRead($e.Rfd) }
      $fs = $streams[$e.Rfd]
      $fs.Position = $e.Offset
      $blob = [byte[]]::new($e.Stored)
      [void]$fs.Read($blob, 0, $blob.Length)
      if ($e.Flags -band 2) {
        # u32 raw size, then a zlib stream: skip the 2-byte zlib header and inflate the deflate data
        $ms = [System.IO.MemoryStream]::new($blob, 6, $blob.Length - 6)
        $ds = [System.IO.Compression.DeflateStream]::new($ms, [System.IO.Compression.CompressionMode]::Decompress)
        $content = [byte[]]::new($e.Raw)
        $read = 0
        while ($read -lt $content.Length) {
          $r = $ds.Read($content, $read, $content.Length - $read)
          if ($r -le 0) { break }
          $read += $r
        }
        $ds.Dispose()
        if ($read -ne $e.Raw) { Write-Warning "$($e.Name): size mismatch, skipped"; continue }
      } else {
        $content = $blob
      }
      $dest = Join-Path (Join-Path $outRoot $prefix) ($e.Name -replace '\\', [IO.Path]::DirectorySeparatorChar)
      [void](New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dest))
      [System.IO.File]::WriteAllBytes($dest, $content)
    }
  } finally {
    foreach ($s in $streams.Values) { $s.Dispose() }
  }
}
Write-Host "Done: $outRoot" -ForegroundColor Green
