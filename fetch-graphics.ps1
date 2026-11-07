<#
.SYNOPSIS
  Downloads the third-party graphics components of the Emperor Spice graphics pack
  (dgVoodoo2 + ReShade + shaders) from their official sources and assembles them in
  build-graphics\graphics, ready to be packaged next to EmperorSpice.exe.

.DESCRIPTION
  Nothing third-party is committed to the repository. Each component is pinned to a fixed
  version below - updating a component means changing one line. If a SHA256 is filled in,
  the download is verified against it and the build fails on a mismatch; an empty hash only
  prints the value (copy it in to pin the exact file).

  The dgVoodoo.conf is taken from the downloaded dgVoodoo package and only the few settings
  Emperor needs are changed, so the file always matches the dgVoodoo version in use:
    - Output API Direct3D 11, texture filtering forced to 16x anisotropic
    - NO MSAA (Emperor hangs with dgVoodoo MSAA - anti-aliasing comes from ReShade SMAA instead)
    - no dgVoodoo watermark
#>
param([string]$OutDir = (Join-Path $PSScriptRoot "build-graphics"))

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

# --- pinned versions (one line per component) ---------------------------------------------
$DgVoodooUrl    = "https://dege.fw.hu/dgVoodoo2/bin/dgVoodoo2_87_2.zip";      $DgVoodooSha256 = ""
$ReShadeUrl     = "https://reshade.me/downloads/ReShade_Setup_6.8.0.exe";      $ReShadeSha256  = ""
$ShadersRepo    = "https://github.com/crosire/reshade-shaders";                $ShadersRef     = "slim"
$SweetFxRepo    = "https://github.com/CeeJayDK/SweetFX";                       $SweetFxRef     = "master"
# -------------------------------------------------------------------------------------------

function Get-Pinned([string]$url, [string]$sha, [string]$dest) {
  Write-Host "Downloading $url"
  Invoke-WebRequest -Uri $url -OutFile $dest -UseBasicParsing -UserAgent "EmperorSpice-build"
  $actual = (Get-FileHash $dest -Algorithm SHA256).Hash.ToLower()
  if ($sha) {
    if ($actual -ne $sha.ToLower()) { throw "SHA256 mismatch for $url`n  expected $sha`n  got      $actual" }
    Write-Host "  SHA256 verified: $actual" -ForegroundColor Green
  } else {
    Write-Host "  SHA256 (not pinned yet): $actual" -ForegroundColor Yellow
  }
}

function Find-7Zip {
  foreach ($p in @("$env:ProgramFiles\7-Zip\7z.exe", "${env:ProgramFiles(x86)}\7-Zip\7z.exe")) { if (Test-Path $p) { return $p } }
  $cmd = Get-Command 7z -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }
  throw "7-Zip not found (needed to extract ReShade32.dll from the ReShade setup)."
}

# Change "Key = value" lines inside one [Section] of a dgVoodoo.conf; fails if a key is missing.
function Set-ConfValues([string[]]$lines, [string]$section, [hashtable]$values) {
  $current = ""
  $found = @{}
  for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match '^\s*\[(.+?)\]') { $current = $Matches[1]; continue }
    if ($current -ne $section) { continue }
    foreach ($key in $values.Keys) {
      if ($lines[$i] -match "^(\s*$([regex]::Escape($key))\s*=\s*)(.*)$") {
        $lines[$i] = $Matches[1] + $values[$key]
        $found[$key] = $true
      }
    }
  }
  foreach ($key in $values.Keys) { if (-not $found[$key]) { throw "dgVoodoo.conf: key '$key' not found in [$section]" } }
  return ,$lines
}

$work = Join-Path $OutDir "_work"
$gfx  = Join-Path $OutDir "graphics"
if (Test-Path $OutDir) { Remove-Item $OutDir -Recurse -Force }
New-Item -ItemType Directory -Path $work, $gfx, (Join-Path $gfx "licenses") -Force | Out-Null

# --- dgVoodoo2 -----------------------------------------------------------------------------
$dgvZip = Join-Path $work "dgVoodoo2.zip"
Get-Pinned $DgVoodooUrl $DgVoodooSha256 $dgvZip
$dgvDir = Join-Path $work "dgVoodoo2"
Expand-Archive $dgvZip -DestinationPath $dgvDir
Copy-Item (Join-Path $dgvDir "MS\x86\DDraw.dll")   $gfx
Copy-Item (Join-Path $dgvDir "MS\x86\D3DImm.dll")  $gfx
$conf = Get-Content (Join-Path $dgvDir "dgVoodoo.conf")
$conf = Set-ConfValues $conf "General"  @{ "OutputAPI" = "d3d11_fl10_1" }
$conf = Set-ConfValues $conf "DirectX"  @{ "Filtering" = "16"; "Antialiasing" = "appdriven"; "dgVoodooWatermark" = "false" }
Set-Content -Path (Join-Path $gfx "dgVoodoo.conf") -Value $conf -Encoding ascii
Get-ChildItem $dgvDir -File | Where-Object { $_.Name -match '(?i)readme|license|\.url$' } | ForEach-Object {
  Copy-Item $_.FullName (Join-Path $gfx "licenses\dgVoodoo2-$($_.Name)")
}

# --- ReShade -------------------------------------------------------------------------------
$rsExe = Join-Path $work "ReShade_Setup.exe"
Get-Pinned $ReShadeUrl $ReShadeSha256 $rsExe
$sevenZip = Find-7Zip
& $sevenZip e $rsExe "ReShade32.dll" "-o$work" -y | Out-Null
if (-not (Test-Path (Join-Path $work "ReShade32.dll"))) { throw "ReShade32.dll not found inside the ReShade setup." }
Copy-Item (Join-Path $work "ReShade32.dll") $gfx

# --- shaders (ReShade standard effects incl. SMAA, and SweetFX) -----------------------------
$shaderOut = Join-Path $gfx "reshade-shaders"
foreach ($src in @(@{ Repo = $ShadersRepo; Ref = $ShadersRef; Name = "reshade-shaders" },
                   @{ Repo = $SweetFxRepo; Ref = $SweetFxRef; Name = "SweetFX" })) {
  $dir = Join-Path $work $src.Name
  git clone --depth 1 --branch $src.Ref $src.Repo $dir 2>&1 | Out-Null
  if ($LASTEXITCODE -ne 0) { throw "git clone failed: $($src.Repo)" }
  $commit = (git -C $dir rev-parse HEAD).Trim()
  Write-Host "  $($src.Name) @ $($src.Ref) = $commit"
  foreach ($sub in @("Shaders", "Textures")) {
    $from = Join-Path $dir $sub
    if (Test-Path $from) { Copy-Item $from (Join-Path $shaderOut $sub) -Recurse -Force }
  }
  foreach ($lic in @("LICENSE", "LICENSE.md", "LICENSE.txt", "README.md")) {
    $p = Join-Path $dir $lic
    if (Test-Path $p) { Copy-Item $p (Join-Path $gfx "licenses\$($src.Name)-$lic") }
  }
}
foreach ($fx in @("SMAA.fx", "Curves.fx", "Vibrance.fx", "LumaSharpen.fx")) {
  if (-not (Get-ChildItem $shaderOut -Recurse -Filter $fx)) { throw "Shader $fx missing after download." }
}

# --- our own config ------------------------------------------------------------------------
Copy-Item (Join-Path $PSScriptRoot "graphics\ReShade.ini")        $gfx
Copy-Item (Join-Path $PSScriptRoot "graphics\Emperor_Modern.ini") $gfx

Remove-Item $work -Recurse -Force
Write-Host "Graphics pack ready: $gfx" -ForegroundColor Green
Get-ChildItem $gfx | Select-Object Name, @{N="KB";E={[math]::Round($_.Length/1KB,1)}} | Format-Table -AutoSize
