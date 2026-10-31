# Renders the MakerWorld images into ..\img\makerworld.
#   .\make_renders.ps1 -Blender "C:\Program Files\Blender Foundation\Blender 4.5\blender.exe"
#   .\make_renders.ps1 -Blender ... -Samples 32 -Shots hero,static     # quick look
# Needs OpenSCAD and Blender 4.5. Downloads the two CC0 background assets on first use,
# exports the parts with export.scad into build\, then runs render.py.
param(
    [Parameter(Mandatory = $true)][string]$Blender,
    [string]$OpenSCAD = "C:\Program Files\OpenSCAD\openscad.com",
    [int]$Samples = 256,
    [string[]]$Shots = @()
)
$ErrorActionPreference = "Stop"
$here = $PSScriptRoot
$assets = Join-Path $here "assets"
$build = Join-Path $here "build"
$stl = Join-Path $build "stl"
$out = Join-Path (Split-Path $here) "img\makerworld"
$screen = Join-Path $here "..\..\host\out\wl_burgtheater@3x.png"
New-Item -ItemType Directory -Force $assets, $stl, $out | Out-Null

# the screen frame, plus the other two styles for the "styles" shot
foreach ($frame in "wl_burgtheater", "compact_grouped", "oebb_delays") {
    $file = Join-Path (Split-Path $screen) "$frame@3x.png"
    if (-not (Test-Path $file)) { throw "UI frame $file missing: build and run the host renderer (host/render.c) first." }
}

# CC0 assets: Vienna tram stop photo (Wikimedia Commons) and urban HDRI (Poly Haven)
$downloads = @{
    "vienna_tram_stop.jpg"   = "https://commons.wikimedia.org/wiki/Special:FilePath/Vienna_tram_line_12_at_stop_Hillerstrasse_2025-09-01_b.jpg?width=1920"
    "zawiszy_czarnego_2k.hdr" = "https://dl.polyhaven.org/file/ph-assets/HDRIs/hdr/2k/zawiszy_czarnego_2k.hdr"
}
foreach ($name in $downloads.Keys) {
    $file = Join-Path $assets $name
    if (-not (Test-Path $file)) {
        Write-Host "downloading $name"
        Invoke-WebRequest -Uri $downloads[$name] -OutFile $file -UserAgent "bimfo-render/1.0"
    }
}

# Parts in their assembled positions. OpenSCAD wants the quotes in -D values, which
# Windows PowerShell would strip from normal arguments, so the command line is built as one string.
$parts = "shell", "labels", "back", "back_labels", "pole", "pole_top", "pole_cover", "speaker_bracket",
         "horns", "cap", "base", "base_strip", "fence", "can_grey", "can_orange", "screen_dummy",
         "board_lcd", "board_pcb", "board_metal", "board_black"
function Export([string]$name, [string]$what, [string]$extra = "") {
    $a = '-o "{0}\{1}.stl" -D "part=\"none\"" -D "what=\"{2}\"" {3} "{4}\export.scad"' -f $stl, $name, $what, $extra, $here
    # OpenSCAD reports on both streams; keep them per part
    Start-Process -FilePath $OpenSCAD -ArgumentList $a -NoNewWindow -PassThru `
        -RedirectStandardOutput "$stl\$name.out" -RedirectStandardError "$stl\$name.log"
}
$jobs = @()
foreach ($p in $parts) { $jobs += Export $p $p }
# the static model's closed back
$jobs += Export "back_closed" "back" '-D "header_pins=false"'
$jobs += Export "back_closed_labels" "back_labels" '-D "header_pins=false"'
Write-Host "exporting $($jobs.Count) parts"
$jobs | Wait-Process
$logs = Get-ChildItem "$stl\*.log", "$stl\*.out"
$bad = $logs | Where-Object { Select-String -Path $_.FullName -Pattern "ERROR|WARNING: Object may not be a valid" -Quiet }
if ($bad) { throw "export failed, see $($bad -join ', ')" }

# the dimensions render.py needs, from the export's echo
$echo = Select-String -Path "$stl\shell.log", "$stl\shell.out" -Pattern "RENDER = \[(.*)\]" | Select-Object -First 1
if (-not $echo) { throw "no RENDER echo from the shell export" }
"[" + $echo.Matches[0].Groups[1].Value + "]" | Set-Content -Encoding ascii (Join-Path $build "params.json")

& $Blender -b -P (Join-Path $here "render.py") -- $build (Resolve-Path $screen) $out $Samples @Shots
