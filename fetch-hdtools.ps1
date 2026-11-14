<#
.SYNOPSIS
  Assembles the graphics workshop tools (Real-ESRGAN) in build-graphics/hdtools, ready to be packaged
  next to EmperorSpice.exe.

.DESCRIPTION
  The launcher's "HD textures (AI)" option runs realesrgan-ncnn-vulkan.exe on the player's PC to upscale
  the player's own game textures (no game data is shipped). The program comes unmodified from the
  official Real-ESRGAN release on GitHub; the zip is checked against its SHA256 below - the build fails
  if anything differs. Only what the launcher needs is taken: the program, its OpenMP runtime and the
  realesrgan-x4plus model. The license texts come from hdtools/LICENSES.txt in this repository.

.PARAMETER SourceDir
  Use a local copy of the zip instead of downloading it (for offline tests).
#>
param(
  [string]$OutDir = (Join-Path $PSScriptRoot "build-graphics"),
  [string]$SourceDir = ""
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

# --- pinned component ------------------------------------------------------------------------
$Url    = "https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.5.0"
$Zip    = "realesrgan-ncnn-vulkan-20220424-windows.zip"
$Sha256 = "abc02804e17982a3be33675e4d471e91ea374e65b70167abc09e31acb412802d"
# ----------------------------------------------------------------------------------------------

$tmp = Join-Path $OutDir "tmp-hdtools"
$dest = Join-Path $OutDir "hdtools"
foreach ($d in @($tmp, $dest)) { if (Test-Path $d) { Remove-Item $d -Recurse -Force } }
New-Item -ItemType Directory -Force -Path $tmp, $dest, (Join-Path $dest "models") | Out-Null

$zipPath = Join-Path $tmp $Zip
if ($SourceDir) {
  Copy-Item (Join-Path $SourceDir $Zip) $zipPath
} else {
  Write-Host "Downloading $Url/$Zip"
  Invoke-WebRequest -Uri "$Url/$Zip" -OutFile $zipPath -UseBasicParsing
}
$actual = (Get-FileHash $zipPath -Algorithm SHA256).Hash.ToLower()
if ($actual -ne $Sha256) { throw "SHA256 mismatch for $Zip`n  expected $Sha256`n  got      $actual" }
Write-Host "  ${Zip}: SHA256 OK" -ForegroundColor Green

Expand-Archive $zipPath -DestinationPath (Join-Path $tmp "x")
$x = Join-Path $tmp "x"
foreach ($f in @("realesrgan-ncnn-vulkan.exe", "vcomp140.dll")) {
  Copy-Item (Join-Path $x $f) $dest
}
foreach ($f in @("realesrgan-x4plus.param", "realesrgan-x4plus.bin")) {
  Copy-Item (Join-Path $x "models\$f") (Join-Path $dest "models")
}
Copy-Item (Join-Path $PSScriptRoot "hdtools\LICENSES.txt") $dest
Remove-Item $tmp -Recurse -Force

$sizeMB = (Get-ChildItem $dest -Recurse -File | Measure-Object Length -Sum).Sum / 1MB
Write-Host ("Graphics workshop tools ready in {0} ({1:N1} MB)" -f $dest, $sizeMB) -ForegroundColor Green
