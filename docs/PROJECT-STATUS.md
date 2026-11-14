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

## Ziel: „Emperor Spice Remaster“ (Vision des Projektinhabers, 14.11.)
Vorbild: **Mass Effect Legendary Edition**. Die alte Engine bleibt, drumherum wird alles modernisiert, damit sich das Spiel 2026 zeitgemäß anfühlt.
- Fertig: moderne Auflösungen, Breitbild, Schrift, Kamera-Zoom; Grafikstil HD (Kantenglättung, Licht, Farben); Stabilität, Multiplayer, Wine, Mods.
- Als Nächstes: **Grafik-Werkstatt** im Launcher (Icons, Texturen, Boden per KI + Remaster, beim Spieler lokal, als Mod eingebunden, mit Fortschrittsanzeige).
- Später: Seitenleiste/Oberfläche neu in HD; **Videos** in HD (Bink 640×480 → KI-Video, eigene Wiedergabe über den Film-Hook; erst Test mit einer kurzen `.bik`); Bugfixes des Originals; Komfort (Steuerung, Speicherstände, Auflösung automatisch).
- Grenzen: 3D-Modelle und Animationen bleiben von 2001; keine modernen Materialien (Normal Maps) in der Engine, der moderne Look wird in die Texturen gebacken.

### Pull Requests / Branches
- **PR #7 – Linux/Steam Deck/macOS (Wine/Proton/CrossOver)**: Fix aus 2dChans Fork (`OaEnablePerUserTLibRegistration` dynamisch laden). Build ist grün. **Noch nicht gemergt und nicht in v1.1 enthalten.** Unter Windows ändert sich nichts. Unter Linux ist es ungetestet.
- Lokale Branches (`large-font`, `camera-zoom`, `mp-hardening`, `release-1.1`, `cleanup`, `german-support`) sind gemergt und können gelöscht werden.

### Laufendes Experiment: Grafik „HD“ (Test 2 an den Tester verschickt, Rückmeldung steht aus)
Dateien: `graphics/experimental/`
- dgVoodoo-Supersampling: `[DirectX] Resolution = 2x`, `[GeneralExt] Resampling = lanczos-3`
- ReShade: Glamarye Fast Effects (MIT, rj200/Glamarye_Fast_Effects_for_ReShade @ 9dd9b82) mit Umgebungsverdeckung und Fake-GI, dazu ein eigener Bloom (`EmperorBloom.fx`), Curves, Vibrance und AMD CAS
- Ergebnis Test 1: Im Spiel läuft es, **im Menü wird das Bild schwarz** (keine Tiefe). Der Effekt wurde als „nicht so krass“ empfunden.
- Test 2: `depth_detect`/`sky_detect` an (Menü/HUD ausgelassen), alle Effekte kräftiger.
- Noch unklar: Kommt ReShade unter dgVoodoo mit 2x überhaupt an die Tiefe? Zum Prüfen gibt es die Taste **Ende** (DisplayDepth), ein Screenshot davon fehlt noch.
- Wird es übernommen, muss `fetch-graphics.ps1` Glamarye (gepinnter Commit) mitladen, und der Launcher bekommt die Wahl zwischen den Presets „Klassisch“ und „HD“.

### Community-Tools gesichtet (13.11.)
Siehe `docs/tools-and-formats.md`: BAG-Format (Musik/Sprache) komplett, Pflichtdateien einer Karte, `rules.txt`. Offen: ob das Spiel lose Dateien in `Data\<PREFIX>\` lädt (der Visual Mod Creator nutzt `Data\Model\rules.txt`). Die Tools selbst liegen nicht im Repo.

### Geplant: Grafik-Werkstatt im Launcher (Stand 14.11., noch nicht gebaut)
Ergebnis der Tests mit Test-Mods (Prototyp-Skripte lokal, nicht im Repo):
- **Icons:** alle (auch Infanterie, graue Varianten) Scanlines entfernen (vertikaler [1,2,1]/4-Filter), Real-ESRGAN x4plus 4-fach, auf 128 px. Gefiel besser als 3D-Neu-Rendern (näher am Original, kein Renderer nötig).
- **Texturen (im Spiel getestet, HD-Test4 passt):** Real-ESRGAN 4-fach → „Remaster dezent“ (Relief, Staub, Kanten, Struktur) → mit Lanczos auf **2-fach** verkleinern (4-fach treppte an harten Linien; Mipmaps waren nicht die Ursache, `Mipmapping=autogen_bilinear` brachte nichts). Effekt-Texturen (Flash, Spot, Halo …) auslassen. Magenta-Farbschlüssel exakt erhalten.
- **Hausfarbe:** Pixel, die im Original reines Blau sind (R=G=0, Stufen u. a. 146/165/190/220/255), müssen nach dem Hochrechnen wieder exakt reines Blau in den Original-Stufen sein (Maske aus dem Original, nächster Nachbar). Sonst erkennt das Spiel sie nicht als Hausfarbe („Recolour Textures“) und färbt sie nicht in der Spielerfarbe.
- Größe für alle ~2.460 Texturen bei 2-fach ≈ 350 MB.
- **Boden (176 Gelände-Texturen aus den test.xbf der Karten, liegen in 3DDATA, 128 px, gekachelt):** Real-ESRGAN **4-fach, ohne Remaster** (Variante „A“, im Spiel besser als mit Remaster), nahtlos durch Wrap-Padding vor dem Hochrechnen. Danach feine dunkle Linien nur in glatten Sandflächen dämpfen (Black-Top-Hat, Maske „warm, hell, wenig Struktur“); sonst entsteht auf Sandrampen ein sichtbares Linienraster. Fels, Klippen, Eis bleiben unberührt.
- **Anforderungen des Projektinhabers:** Fortschrittsanzeige im Launcher; das Ergebnis wird **als Mod eingebunden** (an/abschaltbar, wie der Mod-Ordner). Alles entsteht beim Spieler lokal (EA-Daten).
- Mitzuliefern: realesrgan-ncnn-vulkan (Windows, Real-ESRGAN v0.2.5.0, BSD-3/MIT) mit Modell x4plus; Fallback ohne Vulkan: normales Hochrechnen.

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
