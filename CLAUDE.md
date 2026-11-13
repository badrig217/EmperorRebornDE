# CLAUDE.md – Hinweise für Claude Code in diesem Repo

**Emperor Spice** macht „Emperor: Schlacht um Dune“ (2001) auf modernen PCs spielbar und modernisiert es. Es ist ein Fork von Emperor Reborn.
Bevor du etwas tust, lies **`docs/PROJECT-STATUS.md`** (Stand, Roadmap, offene Punkte) und bei Arbeit an Game.exe oder Dateiformaten **`docs/reverse-engineering.md`** und **`docs/tools-and-formats.md`** (alte Community-Tools, BAG, Karten, rules.txt).

## Zusammenarbeit mit dem Projektinhaber
- Sprache: **Deutsch**, locker und knapp. Er ist kein Programmierer, technische Dinge also verständlich erklären.
- **Vor neuer Arbeit kurz absprechen**, was du vorhast. Er entscheidet.
- **Keine Builds auf gut Glück.** Erst lokal prüfen (Syntax-Check, Skripte, Simulation), dann ein CI-Build. Er testet alles im Spiel, deshalb ist jeder Fehlversuch teuer.
- Er testet unter Windows 11, deutsche Version, 2560×1440, AMD RX 7800 XT, mit dgVoodoo + ReShade. Bei Problemen fragst du nach `data\emperor.txt` (Log der Hooks und des Spiels).
- Releases und Repo-Einstellungen macht **er selbst**. Du bereitest PRs, Versionsnummer und Release-Text vor.
- Commits und PRs auf Deutsch oder Englisch. PR-Beschreibungen auf Deutsch.

## Rechtliches (wichtig)
- **Keine Spieldateien von EA ins Repo oder Release**, auch nicht bearbeitet oder hochgerechnet (Texturen, Modelle, MAP-Dateien, Game.exe). Erlaubt ist eigener Code, eigene Grafik (Logo, Header) und Daten aus freien Quellen (DejaVu Sans, Glamarye MIT …). Alles, was auf Spieldaten aufbaut, erzeugt der Launcher **beim Spieler lokal**.
- Neue Fremdkomponenten gehören mit Lizenz in `THIRD-PARTY-NOTICES.txt`.

## Aufbau
- `EmperorLauncher/` – Win32-Launcher (`EmperorSpice.exe`): UI, Installer (CD/ISO, `IsoReader.hpp`), Grafik-Paket (`GraphicsPack.hpp`), Font-Pack (`FontPack.hpp`), Sprache (`Lang.hpp`, `T(en, de)`), Verknüpfung. Startet Game.exe per Detours mit `EmperorHooks.dll`.
- `EmperorHooks/` – DLL im Spielprozess, läuft vor WinMain (`dllmain.cpp`, `runHooks`). Einstellungen liegen in `Settings.hpp` (Registry `HKCU\Software\WestwoodRedirect\Emperor\LauncherCustomSettings`). Eigene Hooks: `MoneyCheat`, `CameraZoom`, `LargeFont`, ReShade-Loader und ReShade-Add-on (`ReShadeAddon.cpp`: Tiefen-Effekte nur in Missionen). Aus Reborn: Breitbild/HUD, MovieFix, SmoothScroll, WOL-Netzwerk usw.
- `WolServerStandalone/`, `CommsDataDumper/` – aus Reborn übernommen.
- `graphics/` – ReShade-Konfiguration für das Grafik-Paket: `Emperor_Modern.ini`, `Emperor_HD.ini` und eigene Shader (`reshade-shaders/`).
- `fonts/` – fertige Schrift-Archive (gebaut mit `tools/fonts/build_fontpack.py`).
- `fetch-graphics.ps1` – lädt dgVoodoo/ReShade/Shader (SHA256 bzw. Commit gepinnt) nach `build-graphics/` (gitignored).
- `package.ps1 -Version X.Y` – baut das Release-ZIP. Die CI (`.github/workflows/build.yml`, windows-2022) ruft es mit `-Version ci` auf.
- `tools/re/` – `rfd.py` (Archive packen/entpacken), `re_tools.py` (Game.exe untersuchen), `upscale.py` (Real-ESRGAN für TGA).
- `tools/experiments/` – frühere Versuche (Karten-Wash, Icons). `tools/reshadefx-check/` – prüft ReShade-Shader unter Linux.

## Stolperfallen
- **Umlaute in C++-Quellen nur als `\uXXXX`** (MSVC liest UTF-8 sonst falsch).
- Zeilenenden und BOM der bestehenden Dateien beibehalten. Manche Dateien sind CRLF und/oder UTF-8 mit BOM.
- Hooks nutzen feste Adressen aus Game.exe 1.09 (MD5 `7dc4fda2f6b7e8a6b70e846686a81938`). Vor jedem Patch die Originalbytes prüfen und im Log melden, wie in `CameraZoom.cpp`.
- Spiel-Archive: Das Spiel lädt `PREFIX0001`, `0002` … und hört bei der ersten Lücke auf. Eigene Archive bekommen die nächste freie Nummer, vorhandene werden nie überschrieben. Ein gleichnamiger Eintrag mit neuerem Zeitstempel gewinnt.
- dgVoodoo-MSAA hängt das Spiel auf. ReShade-Effekte, die Tiefe brauchen, müssen in Menüs (ohne Tiefe) aussetzen.
- Die Schrift „Standard“ skaliert das Spiel selbst, nur „Small“ hat feste Pixel.
- Lokal kein MSVC: Für Syntax-Checks geht `i686-w64-mingw32-g++ -std=c++20 -fsyntax-only`, für Launcher-Screenshots MinGW + Wine + Xvfb. Der echte Build läuft in der CI.
