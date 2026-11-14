# Emperor Spice: Projektstand

Stand: 14.11.2026 · Repo: https://github.com/badrig217/EmperorSpice · Aktuelles Release: **v1.1** (7.11.2026)

## Worum es geht

Emperor Spice macht „Emperor: Schlacht um Dune“ (2001, Westwood/EA) auf modernen Windows-PCs spielbar und modernisiert es nach und nach, auf Deutsch und Englisch. Es ist ein Fork von [Emperor Reborn](https://github.com/azmawee/EmperorReborn) (azmawee), das wiederum auf [EmperorLauncher](https://github.com/wheybags/EmperorLauncher) (wheybags) aufbaut. Im Repo und im Release stecken **keine Spieldateien**. Alles kommt von den CDs bzw. ISOs des Spielers und dessen Patch 1.09.

Das Ziel des Projektinhabers: ein „Remaster durch Modernisieren“. Die Original-Engine bleibt, und alles, was man sieht und anfasst, wird Schritt für Schritt erneuert. Eine komplett neue Engine (wie CorrinoEngine) wurde bewusst ausgeschlossen.

## Fertig

| Version | Inhalt |
|---|---|
| v1.0 | Fork, deutsch/englischer Launcher mit moderner DPI-fester Oberfläche, Installer von CDs **oder ISO-Dateien** (eigener ISO9660/Joliet-Leser, Disc-Erkennung über Inhalt, Ordner-Scan), Patch 1.09 DE/EN wird automatisch angewendet, Grafik-Paket (dgVoodoo2 + ReShade mit SMAA, Curves, Vibrance, LumaSharpen) wird beim Build geladen und per SHA256 geprüft, Desktop-Verknüpfung, Geld-Cheat (Strg+Umschalt+G = +25.000, nur Einzelspieler), neue README und INSTALL.txt, Aufräumen der Altlasten |
| v1.1 | Kamera-Zoom (Original/1,25/1,5/1,75), große Meldungen im Spiel (SmallHD 2x aus DejaVu Sans, ab 1440p automatisch an), Absicherung des Multiplayers (aus dem technischen Review), Versionsnummer 1.1 |

Alles davon hat der Projektinhaber auf Windows 11, deutsche Version, 2560×1440, AMD RX 7800 XT getestet. Die einzige Ausnahme ist der Multiplayer, dafür steht ein Test mit zwei Rechnern noch aus.

## Offen

## Ziel: „Emperor Spice Remaster“ (Vision des Projektinhabers, 14.11.)
Vorbild: **Mass Effect Legendary Edition**. Die alte Engine bleibt, drumherum wird alles modernisiert, damit sich das Spiel 2026 zeitgemäß anfühlt.
- Fertig: moderne Auflösungen, Breitbild, Schrift, Kamera-Zoom; Grafikstil HD (Kantenglättung, Licht, Farben); Stabilität, Multiplayer, Wine, Mods.
- In Arbeit: **Grafik-Werkstatt** im Launcher (Icons, Texturen, Boden per KI + Remaster, beim Spieler lokal, als Mod eingebunden, mit Fortschrittsanzeige). Gebaut, Spieltest steht aus (siehe unten).
- Später: Seitenleiste/Oberfläche neu in HD; **Videos** in HD (Bink 640×480 → KI-Video, eigene Wiedergabe über den Film-Hook; erst Test mit einer kurzen `.bik`); Bugfixes des Originals; Komfort (Steuerung, Speicherstände, Auflösung automatisch).
- Grenzen: 3D-Modelle und Animationen bleiben von 2001; keine modernen Materialien (Normal Maps) in der Engine, der moderne Look wird in die Texturen gebacken.

### Pull Requests / Branches
- PR #7 (Wine/Proton), #8 (Projektstand) und #9 (Mod-Ordner) sind gemergt, aber noch in keinem Release. Wine ist ungetestet.
- Lokale Branches (`large-font`, `camera-zoom`, `mp-hardening`, `release-1.1`, `cleanup`, `german-support`) sind gemergt und können gelöscht werden.

### Grafikstil „HD“ (PR #10, im Spiel getestet: passt)
- Launcher: „Grafikstil“ Original / Modern / HD (ersetzt den Haken „Grafik-Paket“). Registry: `GraphicsPack`, `GraphicsStyle` (0 = Modern, 1 = HD). Der Launcher setzt beim Start in den Kopien im Spielordner nur `dgVoodoo.conf [DirectX] Resolution` (2x/unforced), bei HD `[GeneralExt] Resampling = lanczos-3` und `ReShade.ini [GENERAL] PresetPath` (`Emperor_HD.ini`/`Emperor_Modern.ini`).
- Preset `graphics/Emperor_HD.ini`: Glamarye Fast Effects (MIT, rj200 @ 9dd9b82, von `fetch-graphics.ps1` geladen) mit Umgebungsverdeckung und Fake-GI, eigener Bloom (`graphics/reshade-shaders/Shaders/EmperorSpice/EmperorBloom.fx`), Curves, Vibrance, AMD CAS.
- Tiefe: Mit DisplayDepth „Normal map“ geprüft. Richtig ist **FAR_PLANE 1.5, nicht umgedreht** (Standard 1000 machte alles flach). Steht im Preset.
- Schwarzes Hauptmenü mit Glamarye (das Menü ist eine 3D-Szene): `EmperorHooks/ReShadeAddon.cpp` meldet sich über die offizielle Add-on-API bei ReShade an (Header v6.8.0 in `EmperorHooks/reshade/`) und schaltet Techniken, die mit „Glamarye“ beginnen, außerhalb einer Mission aus (Missions-Flag `0x817c0c`). Die Anmeldung am echten ReShade 6.8.0 ist unter Wine geprüft. Der Laufzeittest ging unter Wine nicht (D3D11 hängt dort), also im Spiel prüfen. Log-Zeilen: „ReShade add-on: …“.
- **Test im Spiel (13.11.):** Menü-Umschaltung läuft. Glamarye mit richtiger Tiefe ist aber unbrauchbar: Fake GI färbt alles grün, und auch mit `gi_strength=0` gibt es dunkle Schleier an Kartenrand/Seitenleiste und treppige schwarze Kanten am Schleier/Fog. Ohne Glamarye ist alles okay. Deshalb ist Glamarye im HD-Preset **aus** (liegt bei, `gi_strength=0`). HD = Supersampling + Bloom + Curves + Vibrance + CAS.
- Achtung: ReShade schaltet Add-ons ab, sobald es Netzwerkverkehr sieht (Multiplayer). Dann gibt es auch keine Tiefe, Glamarye bleibt wirkungslos.

### Community-Tools gesichtet (13.11.)
Siehe `docs/tools-and-formats.md`: BAG-Format (Musik/Sprache) komplett, Pflichtdateien einer Karte, `rules.txt`. Offen: ob das Spiel lose Dateien in `Data\<PREFIX>\` lädt (der Visual Mod Creator nutzt `Data\Model\rules.txt`). Die Tools selbst liegen nicht im Repo.

### Grafik-Werkstatt im Launcher (Branch `hd-workshop`, gebaut 14.11., Spieltest steht aus)
**Bedienung:** Im Launcher unter „Grafik“ das Häkchen **„HD-Texturen (KI)“** und der Knopf **„Erzeugen…“** (danach „Neu erzeugen…“). Erzeugt wird nur auf Knopfdruck bzw. nach Rückfrage beim Anhaken. Fortschrittsfenster mit Balken, „x von y Texturen“, Restzeit und „Abbrechen“. Registry: `HDTextures` (0/1).

**Ablauf** (`EmperorLauncher/TextureWorkshop.*`, `HdWorkshopCore.hpp`, `HdImage.hpp`, `TextureWorkshopUi.hpp`):
- Liest alle `Textures\*.tga` aus den 3DDATA-Archiven (je Name die neueste; Archive mit Mod- oder HD-Markierung zählen nicht) und die Bodentextur-Liste aus den `test.xbf` der Karten (MAPS-Archive).
- Sortiert: Icons (64×64 mit `Grey_`-Gegenstück) → Effekte (auslassen: `!`, `@`, `%` am Anfang, flash/spot/halo/beam/glow/muzzle) → Boden → Modell-Texturen. Ausgelassen: kleiner als 16 oder größer als 256 px, nur Magenta, Palettenbilder.
- In Paketen zu 48 Stück: Vorbereiten → `hdtools\realesrgan-ncnn-vulkan.exe` (ohne Fenster, Ausgabe in `hdtextures\realesrgan.log`) → Nachbearbeitung in bis zu 4 Threads → direkt ins Archiv schreiben (wenig Speicher, der Launcher ist 32 Bit).
- Ergebnis: `<Spiel>\hdtextures\3DDATA_HD.RFH/.RFD` mit Markierung `EmperorSpice\HDTextures.txt`, ca. 400 MB. Beim „Spielen“ mit Häkchen wird es als nächstes freies `3DDATA00xx` nach DATA verschoben, ohne Häkchen wieder heraus (dahinterliegende Archive rücken nach). Mods gewinnen weiter, weil ihre Einträge neuer sind.

**Rezepte** (aus den Test-Mods, im Spiel getestet):
- **Icons:** Scanlines entfernen (vertikaler [1,2,1]/4-Filter), Real-ESRGAN x4plus 4-fach, Lanczos auf 128 px. Gefiel besser als 3D-Neu-Rendern.
- **Texturen:** Magenta mit Nachbarfarben füllen → Real-ESRGAN 4-fach → „Remaster dezent“ (Relief 0.45, Staub 0.6, Kanten 0.6, Struktur 0.8) → Lanczos auf **2-fach** (4-fach treppte; Mipmaps waren nicht die Ursache) → **Hausfarbe** zurück: Pixel, die im Original reines Blau sind (R=G=0), werden wieder exakt reines Blau in den Original-Stufen, sonst färbt das Spiel sie nicht in der Spielerfarbe → Magenta-Schlüssel exakt zurück (erst ganz am Ende, so gibt es keine rosa Ränder) → Alpha aus dem Original hochskaliert.
- **Boden** (176 Texturen, gekachelt): Wrap-Padding, Real-ESRGAN **4-fach, ohne Remaster**, zuschneiden, feine dunkle Linien nur in glatten Sandflächen dämpfen (sonst Linienraster auf Sandrampen). Fels, Klippen, Eis bleiben.

**Geprüft:** Rechenkern unter Linux gegen die Test-Mods (Boden praktisch gleich, Abweichung 0,3; Icons/Gebäude sichtbar gleich, Unterschiede nur durch ncnn statt PyTorch und anderes Rauschen). Windows-Teil mit MinGW unter Wine: Real-ESRGAN-Start, Abbrechen (Prozess wird beendet, Arbeitsordner gelöscht), Fehlermeldung ohne Vulkan, kompletter Lauf über die Launcher-Oberfläche, Ein-/Ausschalten beim „Spielen“. Prüfwerkzeug: `tools/hdworkshop-check/` (läuft mit der Linux-Version von Real-ESRGAN).

**Mitgeliefert:** `hdtools\` (38 MB) aus dem offiziellen Real-ESRGAN-Release v0.2.5.0, per SHA256 geprüft von `fetch-hdtools.ps1`: Programm, `vcomp140.dll`, Modell x4plus, `LICENSES.txt`. Ohne Vulkan-Treiber gibt es eine Fehlermeldung (kein Ersatzweg).

**Noch offen:** Spieltest unter Windows mit echter Grafikkarte (Dauer, Aussehen aller Völker, Speicher). Nebenbei behoben: Absturz des Launchers, wenn das Grafik-Paket eine Datei nicht kopieren kann (statt Meldung).

### Roadmap (vom Projektinhaber bestätigt, Schwerpunkt: **bessere Grafik**)
1. **Bild:** Grafikstil HD (PR #10). Höchste Modell-Detailstufe erzwingen: **untersucht, bringt nichts** (H0/H1/H2/M3 sind Schadensstufen, die Detailstufe steht schon auf High, siehe reverse-engineering.md). Bleibt: eventuell Icons aus den 3D-Modellen neu rendern (groß).
2. **Steuerung:** Komfortfunktionen. Was der Projektinhaber vermisst, ist noch nicht gefragt.
3. **Technik:** Der Launcher soll beim ersten Start die Monitorauflösung vorschlagen statt 1280×720. Das ist klein und wirkt groß. Außerdem Multiplayer ohne Portfreigabe (schwer: Vermittlungsserver oder ZeroTier/Tailscale) und Speicherstände in „Dokumente“.
4. **Gefühl:** Effekte und Stimmung (zum Teil schon im HD-Preset).

### Ausprobiert und verworfen
- Bodentexturen der Karten hochrechnen: Das Spiel lädt `test.tga` zwar in jeder Größe, aber man sieht keinen Unterschied (siehe reverse-engineering.md).
- Icons 2x hochrechnen: kaum besser, die Vorlage hat nur 64 px.
- Einheiten- und Gebäudetexturen 2x hochrechnen (früher Test mit 3DDATA0004): nur mäßiger Gewinn.
- „Standard“-Schrift 2x: Menüs werden doppelt zu groß, weil das Spiel diese Schrift selbst skaliert.
- dgVoodoo-MSAA: Das Spiel hängt sich auf.

### Bekannte Punkte
- Einmal hat sich der AMD-Treiber zurückgesetzt („D3D11: Removing Device“), mit Zoom 1,25. Beim Wiederholen trat es nicht auf, vermutlich Zufall.
- Fehlende Texturen und Modelle im Log (`MISSING UNIT FILE … HK_Wall` usw.) stammen aus dem Original und sind harmlos.
- Die EXE ist nicht signiert. SmartScreen-Hinweis: „Weitere Informationen → Trotzdem ausführen“. Das wurde bewusst so entschieden.
- Ältere Schrift-Archive können sich im data-Ordner ansammeln (pro Font-Build eine neue Nummer). Das ist harmlos, ein Aufräumen wäre aber schön.
- Detours 4.0.1 ist alt, aber stabil. Der CI-Runner `windows-2022` muss irgendwann auf `windows-2025` umziehen.

## Release-Ablauf
1. Versionsnummer in `EmperorLauncher/EmperorLauncher.cpp` („Emperor Spice vX.Y“) und `package.ps1` (`$Version`) anpassen. Die CI prüft, ob beide gleich sind.
2. PR mergen, danach den Build auf `main` abwarten.
3. Aus dem Artifact `EmperorSpice-ci` die Datei `EmperorSpice-vci.zip` in `EmperorSpice-vX.Y.zip` umbenennen.
4. Das GitHub-Release mit Tag `vX.Y` legt **der Projektinhaber selbst an**: Text, ZIP, „latest“. `RELEASE-NOTES-TEMPLATE.md` dient als Vorlage.
- `thirdparty-v1` ist ein Release, das nur dgVoodoo2_87_6.zip und ReShade_Setup_6.8.0.exe für den Build enthält. Neue Versionen kommen als `thirdparty-v2` mit neuen SHA256-Werten in `fetch-graphics.ps1`.
