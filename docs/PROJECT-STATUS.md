# Emperor Spice: Projektstand

Stand: 13.11.2026 · Repo: https://github.com/badrig217/EmperorSpice · Aktuelles Release: **v1.1** (7.11.2026)

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

### Roadmap (vom Projektinhaber bestätigt, Schwerpunkt: **bessere Grafik**)
1. **Bild:** Grafikstil HD (PR #10, fertig). Danach **höchste Modell-Detailstufe erzwingen** (H0 statt H1/H2/M3, gerade mit Zoom sichtbar, noch nicht untersucht). Danach eventuell Icons aus den 3D-Modellen neu rendern (groß).
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
