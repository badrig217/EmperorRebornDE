# Reverse Engineering: Game.exe 1.09 und Dateiformate

Alles hier gilt für **Game.exe v1.09**, MD5 `7dc4fda2f6b7e8a6b70e846686a81938`. Diese Datei ist für die deutsche und die englische Version identisch. Der Launcher prüft die MD5 vor jedem Start, deshalb dürfen die Hooks mit festen Adressen arbeiten. Jeder Patch prüft außerdem die Originalbytes, bevor er schreibt.

Werkzeuge: `tools/re/re_tools.py` (pefile + capstone), `tools/re/rfd.py` (Archive).

---

## Archive (RFH/RFD)

- Pro Thema `<PREFIX>0001.RFH/.RFD`, `<PREFIX>0002…`. Das Spiel lädt aufsteigend und **hört bei der ersten fehlenden Nummer auf**. Eigene Archive müssen deshalb die nächste freie Nummer bekommen. Vorhandene Archive stammen von den CDs oder vom Patch 1.09 (z. B. `MAPS0002`) und dürfen nie überschrieben werden.
- Kommt ein Eintrag doppelt vor, gewinnt der mit dem **neueren Zeitstempel**. Darüber lassen sich Originaldateien ersetzen.
- RFH, pro Eintrag (little-endian): `u32 name_len (inkl. NUL)`, `u32 timestamp`, `u32 flags (2 = zlib)`, `u32 stored_size (inkl. 4-Byte-Präfix)`, `u32 raw_size`, `u32 offset`, `char name[]`.
- RFD, an `offset`: `u32 raw_size`, danach der zlib-Stream.
- Präfixe im Spiel: 3DDATA (Modelle `*.xbf`, Animationen `*.xaf`, Texturen `Textures\*.tga`), UI (Menü-Szenen, `FONT\*.MAP`, `CAMERA.INI`), MAPS, MODEL, STRINGS, AI, MISSIONS, CAMPAIGN, SOUNDS.
- Texturen sind TGA mit 24 oder 32 Bit, selten 16 Bit ARGB1555. Magenta (255,0,255) dient als Farbschlüssel für Transparenz.

## Schriften (CFont3D)

- Font-Tabelle in `.rdata` ab `0x5d3540`, Datensätze zu 16 Byte: `char* name`, `u8 rowHeight (+4)`, `u8 (+5)`, `u32 pages (+8)`, `u8 (+0xc)`.
  - Index 0 „Small“: Zeilenhöhe 16, 2 Seiten (`small0.tga`, `small1.tga`). Damit werden Meldungen und Hinweise im Spiel gezeichnet.
  - Index 1 „Standard“: Zeilenhöhe 30, 1 Seite (`Standard.tga`). Damit werden Menüs, Buttons und der Ladebildschirm gezeichnet. **Das Spiel skaliert diese Schrift selbst mit der Auflösung.** Eine 2x-Version ist deshalb doppelt zu groß, das wurde bei 720p und 1440p getestet.
- Ladefunktion: `0x4ab330(mapPath, tgaPath, rowHeight, flag)`. Format-Strings `%s\%s%d.MAP` (mehrseitig) und `%s\%s.MAP`, Ordner „FONT“.
- `.MAP` = UTF-16 mit BOM, nur die Zeichenliste in Reihenfolge.
- Der Glyphen-Scanner läuft Zeilenband für Zeilenband (Höhe = rowHeight), **beginnt bei x = 1**, überspringt leere Spalten und nimmt eine zusammenhängende Folge nicht-leerer Spalten als Glyphe. Spalte 0 von Zeile 0 ist eine Markierung. Lücken innerhalb einer Glyphe (z. B. bei `"`) überbrückt man mit einem schwachen Pixel. Das Original nutzt dafür den Wert 16. Alle Werte sollten ≥ 16 sein, falls das Spiel eine 16-Bit-Kopie scannt.
- Leerzeichen = Breite von `_` mit leerem UV-Bereich bei (0,98–0,99). Diese Ecke der Seite muss leer bleiben.
- Emperor Spice: `tools/fonts/build_fontpack.py` rendert SmallHD/StandardHD aus DejaVu Sans und prüft das Layout gegen eine Nachbildung des Scanners. `EmperorHooks/LargeFont.cpp` biegt „Small“ auf „SmallHD“ mit Zeilenhöhe 32 um.

## Kamera (CAMERA.INI, UI-Archiv)

- Werte: `MinZoom 0.5` (am weitesten weg), `MaxZoom 1.25` (am nächsten), `StartZoom 0.35` usw.
- `0x532e5e: fdivr dword [0x5d0720]` mit 1.0f ⇒ `1/MinZoom`, gespeichert als größter Abstandsfaktor bei `[edi+0x30]`. MaxZoom landet analog bei `+0x34`.
- `EmperorHooks/CameraZoom.cpp` lässt den Operanden auf eine eigene Konstante k zeigen ⇒ der größte Abstand wird k/MinZoom. Getestet: 1,5 sieht gut aus, 2,0 ist zu weit.

## Geld / Skript-Interpreter

- Skript-Interpreter: Switch bei `0x4ef5d5`, Sprungtabelle `0x4f3fb4`, Index = Funktions-ID.
- Funktionstabelle `0x60a978`, Datensätze zu 92 Byte (`name[36]` + id). Ids: GetPlayerSide 14, GetSideCash 0x13, AddSideCash 0x3e, SetSideCash 0x98, Multiplayer 2.
- `[0x7d75d4]` Setup-Objekt; `0x5344e0(setup)` = Spieltyp (3 = Multiplayer); `0x5356a0(setup)` = Spielerseite.
- `[0x817c00]` Seiten-Manager (nur in einer Mission ≠ 0); `0x597bb0(sides, i)` = GetSide.
- `0x53ebf0(side, amount)` AddCash, `0x53ec10` SetCash; das Geld liegt bei `side+0x868`.

## Kartenboden (MAPS-Archiv, pro Karte ein Ordner)

- `map.inf`: WIDTH/HEIGHT in Zellen.
- `test.CPT`: „Wash“, also die Farb- und Übergangsschicht des Bodens. Header `u32 w=2048, u32 h=2048`, danach DXT3 in 64 Blöcken zu 256×256, angeordnet als Raster 8×8. Benutzt wird nur der Bereich der Karte. Das entspricht 8 Pixeln pro Zelle.
- `test.CPF`: 256×256 RGB565, Variante für niedrige Detailstufe.
- `texture.dat`: 2048×2048, 8 Bit, Schattenkarte.
- `test.xbf`: Geländenetz; `test.lit`: Licht.
- **Lädt zuerst `test.tga`**, falls vorhanden („Loading TGA Wash“, Funktion `0x54f680`). Die Größe ist beliebig quadratisch, Blockgröße = Breite/8.
- Ergebnis eines Tests mit 2x per Real-ESRGAN: **im Spiel kaum sichtbar**. Der Wash ist nur eine weiche Farbschicht, die scharfen Risse und Krusten kommen von gekachelten Detailtexturen aus 3DDATA. Nicht weiterverfolgt.

## Icons im Seitenmenü

- `Textures\AT_*.tga`, `HK_*.tga`, `OR_*.tga` und `Grey_*` (nicht verfügbar), 64×64 mit 24 Bit, inklusive Scanline-Muster.
- Test mit 2x per Real-ESRGAN in einem eigenen 3DDATA-Archiv: wird geladen, ist aber **kaum besser**, weil die Vorlage zu klein ist. Wirklich besser würde es nur durch Neu-Rendern aus den 3D-Modellen. Das wäre ein großes Projekt, als Vorlage für einen XBF-Reader eignet sich CorrinoEngine.

## Grafik-Pipeline

- Das Spiel nutzt D3D7 über DirectDraw. dgVoodoo2 (`DDraw.dll`, `D3DImm.dll`) übersetzt das nach D3D11.
- MSAA in dgVoodoo **hängt das Spiel auf**. Supersampling über `[DirectX] Resolution = 2x` mit `Resampling = lanczos-3` ist der Test-Stand.
- ReShade wird von EmperorHooks per `LoadLibrary("ReShade32.dll")` aus dem data-Ordner geladen. Ein Proxy über dxgi/d3d11 funktioniert mit dgVoodoo nicht.
- Tiefe für ReShade: Im Gefecht passt `RESHADE_DEPTH_LINEARIZATION_FAR_PLANE=1.5`, nicht umgedreht, nicht kopfüber. Das Hauptmenü ist eine 3D-Szene, deren Tiefe Glamarye nicht verträgt (Bild schwarz). Deshalb schaltet `ReShadeAddon.cpp` solche Effekte ab, solange das Missions-Flag `0x817c0c` 0 ist.
- Modelle gibt es in Detailstufen (`*_H0/H1/H2/M3.xbf`, „Cached …“ im Log). Idee: immer die höchste Stufe erzwingen. **Noch nicht untersucht.**

## Netzwerk (WOL-Emulation aus Emperor Reborn)

- IRC-ähnlicher Server auf TCP 4005, dazu ein UDP-Proxy (WolProxyServer/Client). Bei v1.1 abgesichert: unbekannte Befehle werden ignoriert, die Zahl der Verbindungen und die Puffer sind begrenzt, der Client akzeptiert nur Pakete vom eigenen Host.
- Wine/Proton: `OaEnablePerUserTLibRegistration` muss dynamisch geladen werden (PR #7).
