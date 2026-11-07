<#
.SYNOPSIS
  Assembles the Emperor Spice graphics pack (dgVoodoo2 + ReShade + shaders) in build-graphics/graphics,
  ready to be packaged next to EmperorSpice.exe.

.DESCRIPTION
  No third-party program is committed to this repository. dgVoodoo2 and the ReShade setup are kept,
  unmodified, as assets of the GitHub release "thirdparty-v1" of this repository (the official servers
  remove old versions, so they cannot be pinned there). Every file is checked against its SHA256 below -
  the build fails if anything differs. The shaders come from their official GitHub repositories,
  pinned to a commit.

  To update a component: attach the new file to a release, then change its line below
  (file name + SHA256).

  dgVoodoo.conf is taken from the dgVoodoo2 package and only these settings are changed:
    - OutputAPI d3d11_fl10_1 (Direct3D 11, needed by ReShade)
    - Filtering 16 (16x anisotropic texture filtering)
    - Antialiasing appdriven = NO MSAA (Emperor hangs with dgVoodoo MSAA; ReShade SMAA is used instead)
    - dgVoodooWatermark false

.PARAMETER SourceDir
  Use local copies of the release files instead of downloading them (for offline tests).
#>
param(
  [string]$OutDir = (Join-Path $PSScriptRoot "build-graphics"),
  [string]$SourceDir = ""
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

# --- pinned components (one line each) -------------------------------------------------------
$ReleaseUrl = "https://github.com/badrig217/EmperorSpice/releases/download/thirdparty-v1"
$DgVoodoo = @{ File = "dgVoodoo2_87_6.zip";      Sha256 = "9afa493396feec7108f42a2d97661eef96bbaac5addf0dfb5cfbc3001cd76a0a" }
$ReShade  = @{ File = "ReShade_Setup_6.8.0.exe"; Sha256 = "207aea16205fbf952bc8fe1879966672454cf04002e7ad34237c7990a5b3c0b4" }
$Shaders  = @{ Repo = "https://github.com/crosire/reshade-shaders"; Commit = "fd0022170615ce0d8162d219bff07232fa6dd84f" }
$SweetFx  = @{ Repo = "https://github.com/CeeJayDK/SweetFX";        Commit = "93ddf39b357f5da534ed6d34ba4ec8cc7dcfa361" }
# ----------------------------------------------------------------------------------------------

function Get-Verified([hashtable]$item, [string]$dest) {
  if ($SourceDir) {
    Copy-Item (Join-Path $SourceDir $item.File) $dest
  } else {
    $url = "$ReleaseUrl/$($item.File)"
    Write-Host "Downloading $url"
    Invoke-WebRequest -Uri $url -OutFile $dest -UseBasicParsing
  }
  $actual = (Get-FileHash $dest -Algorithm SHA256).Hash.ToLower()
  if ($actual -ne $item.Sha256) { throw "SHA256 mismatch for $($item.File)`n  expected $($item.Sha256)`n  got      $actual" }
  Write-Host "  $($item.File): SHA256 OK" -ForegroundColor Green
}

function Find-7Zip {
  foreach ($p in @("$env:ProgramFiles\7-Zip\7z.exe", "${env:ProgramFiles(x86)}\7-Zip\7z.exe")) {
    if ($p -and (Test-Path $p)) { return $p }
  }
  $cmd = Get-Command 7z -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }
  throw "7-Zip not found (needed to extract ReShade32.dll from the ReShade setup)."
}

# Change "Key = value" lines inside one [Section] of dgVoodoo.conf; fails if a key is missing.
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
$lic  = Join-Path $gfx "licenses"
if (Test-Path $OutDir) { Remove-Item $OutDir -Recurse -Force }
New-Item -ItemType Directory -Path $work, $gfx, $lic -Force | Out-Null

# --- dgVoodoo2 --------------------------------------------------------------------------------
$dgvZip = Join-Path $work $DgVoodoo.File
Get-Verified $DgVoodoo $dgvZip
$dgvDir = Join-Path $work "dgVoodoo2"
Expand-Archive $dgvZip -DestinationPath $dgvDir
$x86 = Join-Path (Join-Path $dgvDir "MS") "x86"
Copy-Item (Join-Path $x86 "DDraw.dll")  $gfx
Copy-Item (Join-Path $x86 "D3DImm.dll") $gfx
$conf = Get-Content (Join-Path $dgvDir "dgVoodoo.conf")
$conf = Set-ConfValues $conf "General" @{ "OutputAPI" = "d3d11_fl10_1" }
$conf = Set-ConfValues $conf "DirectX" @{ "Filtering" = "16"; "Antialiasing" = "appdriven"; "dgVoodooWatermark" = "false" }
Set-Content -Path (Join-Path $gfx "dgVoodoo.conf") -Value $conf -Encoding ascii
Get-ChildItem (Join-Path $dgvDir "Doc") -File -ErrorAction SilentlyContinue | ForEach-Object {
  Copy-Item $_.FullName (Join-Path $lic "dgVoodoo2-$($_.Name)")
}

# --- ReShade ----------------------------------------------------------------------------------
$rsExe = Join-Path $work $ReShade.File
Get-Verified $ReShade $rsExe
$sevenZip = Find-7Zip
& $sevenZip e $rsExe "ReShade32.dll" "-o$work" -y | Out-Null
if (-not (Test-Path (Join-Path $work "ReShade32.dll"))) { throw "ReShade32.dll not found inside the ReShade setup." }
Copy-Item (Join-Path $work "ReShade32.dll") $gfx

# --- shaders (ReShade standard effects incl. SMAA, and SweetFX), pinned to a commit -------------
$shaderOut = Join-Path $gfx "reshade-shaders"
foreach ($src in @(@{ Name = "reshade-shaders"; Repo = $Shaders.Repo; Commit = $Shaders.Commit },
                   @{ Name = "SweetFX";         Repo = $SweetFx.Repo; Commit = $SweetFx.Commit })) {
  $dir = Join-Path $work $src.Name
  git init -q $dir
  git -C $dir fetch -q --depth 1 $src.Repo $src.Commit
  if ($LASTEXITCODE -ne 0) { throw "git fetch failed: $($src.Repo) $($src.Commit)" }
  git -C $dir checkout -q FETCH_HEAD
  foreach ($sub in @("Shaders", "Textures")) {
    $from = Join-Path $dir $sub
    if (Test-Path $from) {
      $to = Join-Path $shaderOut $sub
      New-Item -ItemType Directory -Path $to -Force | Out-Null
      Copy-Item (Join-Path $from "*") $to -Recurse -Force
    }
  }
  foreach ($name in @("LICENSE", "LICENSE.md", "LICENSE.txt")) {
    $p = Join-Path $dir $name
    if (Test-Path $p) { Copy-Item $p (Join-Path $lic "$($src.Name)-$name") }
  }
  Write-Host "  $($src.Name) @ $($src.Commit)"
}
foreach ($fx in @("SMAA.fx", "Curves.fx", "Vibrance.fx", "LumaSharpen.fx", "ReShade.fxh")) {
  if (-not (Get-ChildItem $shaderOut -Recurse -Filter $fx)) { throw "Shader file $fx missing." }
}

# --- our own config ---------------------------------------------------------------------------
$ours = Join-Path $PSScriptRoot "graphics"
Copy-Item (Join-Path $ours "ReShade.ini")        $gfx
Copy-Item (Join-Path $ours "Emperor_Modern.ini") $gfx

Remove-Item $work -Recurse -Force
Write-Host "Graphics pack ready: $gfx" -ForegroundColor Green
