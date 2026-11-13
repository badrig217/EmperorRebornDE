# Community-Tools für Emperor und was wir daraus lernen

Stand: 13.11.2026. Der Projektinhaber hat sechs alte Tools aus der Emperor-Szene (2001/2002) bereitgestellt. Sie wurden **nur von außen untersucht**: Archive entpackt, Hilfetexte und mitgelieferter Quellcode gelesen. Keines der Programme wurde gestartet.

**Die Programme selbst liegen nicht im Repo.** Der Map-Editor gehört EA („All rights reserved“). Bei den meisten anderen Tools fehlt eine Lizenz, die eine Weitergabe erlaubt. Hier steht deshalb nur, was die Tools können und was wir über die Dateiformate gelernt haben, in eigenen Worten. Die Original-Websites der Tools gibt es nicht mehr. Wer ein Tool braucht, findet die meisten in Community-Archiven (z. B. [ModDB](https://www.moddb.com/games/emperor-battle-for-dune)).

| Tool | Version / Autor | Was es macht | Lizenz | Nutzen für uns |
|---|---|---|---|---|
| **Dune Map Editor** | Beta, Westwood 2001 (erschien mit Patch 1.09) | Der offizielle Karteneditor, mit Handbuch und Tutorial (Word) | © EA, nicht weitergeben | Aufbau der Kartenordner, Gelände-LOD, Licht/Schatten |
| **MapPacker** | 1.2, „Stealth“ | Packt einen Kartenordner in ein `.cmf`-Archiv und installiert es wieder | keine Angabe | Liste der Pflichtdateien einer Karte, CMF-Format |
| **BagTool** | 0.3, khchua (2001) | Liest, baut und zerlegt `.bag`-Dateien (Musik, Sprache) | **GPL 2, mit C++-Quellcode** | Vollständiges BAG-Format |
| **DuneEx 2** | Beta 1 / Build 77, „scorpio9a“ (2001) | Explorer für RFH/RFD- und BAG-Archive: ansehen, entpacken, `.w` → WAV | keine Angabe (Freeware) | Nichts Neues, unser `tools/re/rfd.py` kann das auch |
| **Dune Visual Mod Creator** | 1.08 / „Skirmish Compiler 1.12“, „Riper“ (RPG Creations, 2001) | Grafischer Editor für `rules.txt` (Einheiten, Gebäude, Waffen, Kosten), VB6 | Quellcode liegt bei; Weitergabe nur mit Nennung und eigenem Quellcode | Aufbau von `rules.txt`; Hinweis auf lose Dateien (s. u.) |
| **TibEd** | unbekannt | NSIS-Installer, ließ sich nicht entpacken (alter, komprimierter Header) | unbekannt | Unklar. Der Name spricht für einen Regel-Editor aus der Tiberian-Sun-Szene. Bei Bedarf unter Windows installieren und nachsehen. |

---

## Karten (MAPS-Archiv bzw. `Data\Maps\<Name>\`)

**Pflichtdateien.** MapPacker packt nur, wenn alle sieben Dateien da sind. Das ist also der vollständige Inhalt eines Kartenordners:

| Datei | Inhalt | Erzeugt von |
|---|---|---|
| `test.xbf` | Geländenetz (3D) | Map Editor „Build Map“ |
| `test.cpt` | Farb-Wash, DXT3 (siehe reverse-engineering.md) | WashCompressor aus `test.tga` |
| `test.cpf` | kleine Wash-Variante (RGB565) | WashCompressor |
| `test.lit` | Gelände-Licht | Shadowmaker |
| `texture.dat` | Boden-/Schattendaten | Map Editor / Shadowmaker |
| `!05%thumb.tga` | Vorschaubild im Gefechtsmenü | von Hand aus einem Map-Dump |
| `<Kartenordner>.txt` | Autoreninfo (nur MapPacker, das Spiel braucht sie nicht) | von Hand |

`map.inf` (Größe in Zellen) gehört ebenfalls in den Ordner, MapPacker prüft sie nur nicht.

**Aus dem Handbuch des Map-Editors:**
- Kartengröße 64 bis 256 Zellen pro Seite, in Vierer-Schritten, Standard 96×96. Planeten-Themen kommen aus einer „world ini“ (z. B. Arrakis).
- Die Arbeitsdatei des Editors ist `.dme`. Spielbar ist erst der gebaute Ordner unter `Data\Maps`. Der Pfad dorthin steht in `Resource.cfg` (Eintrag `GAME_MAPS`).
- **Gelände-LOD 0 bis 2:** Felsen und Klippen bestehen aus „Profilen“. Gebaut werden soll mit LOD 2 (am feinsten). Das betrifft nur das Geländenetz der Karte, nicht die LOD-Stufen der Einheitenmodelle (`*_H0/H1/H2/M3.xbf`).
- **Schatten im Gelände sind vorgebacken:** Shadowmaker berechnet sie mit einer Lichtstimmung (`.ssf`, z. B. „dawn“). Gebäude und Einheiten werfen dagegen Echtzeit-Schatten im Spiel. `light.ini` des Editors enthält zwei RGB-Tripel (vermutlich Sonnenlicht und Umgebungslicht).
- Der Wash muss `test.tga` heißen, bevor er komprimiert wird. Das passt dazu, dass das Spiel eine vorhandene `test.tga` bevorzugt lädt (siehe reverse-engineering.md, „Kartenboden“).
- Kachel-Typen (Klippe, Dustbowl, Infanteriefels, Rampe, Sand) werden auf das Raster gemalt. Objekte (`*.xaf`) sind unzerstörbare Deko wie Brücken und Felsnadeln.
- Mehrspieler: Alle Spieler brauchen denselben Kartenordner mit gleichem Namen und gleichen Daten.

**CMF-Format (MapPacker):** ein eigenes Archivformat des Autors, kein Spielformat.
- Header: `s32 cmfVersion (1)`, `s32 mapVersion`, `char mapDir[32]`, `s32 fileCount (7)`.
- Danach `fileCount` Einträge: `s32 offset`, `char name[32]`, `s32 packedSize`.
- Jede Datei: `s32 rawSize`, `s32 packedSize`, dann die Daten. Laut Autor „dieselbe Kompression wie RFH/RFD“. Er nennt sie LZH, wir kennen RFD aber als zlib. Ungeprüft.

## Musik und Sprache: BAG-Format (aus dem BagTool-Quellcode)

Alles little-endian. Die Audiodaten liegen ohne RIFF-Header in der Datei.

- **Header, 16 Byte:** `char id[8]` (`"GABA"`, dann `0x04` und Nullen), `u32 trackCount`, `u32 unknown` (immer `0x40`).
- **Danach `trackCount` Einträge zu je 64 Byte:**
  `char name[32]`, `u32 offset` (ab Dateianfang), `u32 length`, `u32 sampleRate`, `u32 format`, `u8 mp3Start[4]` (die ersten 4 Bytes der MP3-Daten, umgedreht), `u8 zero[12]`.
- **Danach die Audiodaten.** Der erste Datenblock beginnt bei `16 + 64 × trackCount`.
- **Werte für `format`**, wie BagTool sie behandelt:
  | Wert | Bedeutung |
  |---|---|
  | `0x06`, `0x16` | PCM 16 Bit mono |
  | `0x02` | PCM 8 Bit mono |
  | `0x0C` | komprimiert, Format unbekannt (in `Dialog.bag`) |
  | `0x24` | MP3 64 kbit |
  | `0x25` | MP3 128 kbit |
  - Vermutung: Es ist ein Bitfeld, etwa 0x04 = 16 Bit, 0x08 = ADPCM-komprimiert, 0x20 = MP3. `0x0C` wäre dann 16-Bit-ADPCM, wahrscheinlich IMA, wie bei anderen Westwood-Spielen. **Ungeprüft.**
- BagTool akzeptiert beim Einfügen nur WAV mit 22.050 Hz, 16 Bit mono PCM, oder MP3 mit 64/128 kbit. Namen werden auf 32 Zeichen gekürzt.
- Laut BagTool-Hilfe beschreibt eine `music.txt` im data-Ordner, wie Musik und Sprache von der Festplatte statt von CD gestreamt werden. **Noch nicht geprüft**, ob unsere Installation das schon so macht.
- **Ideen für später:** ein Musik-Mod über den Mod-Ordner (eigene BAG-Dateien), bessere Tonqualität, Lautstärke-Ausgleich.

## Spielregeln: `rules.txt` (MODEL-Archiv)

- Eine große INI-Datei (rund 10.000 Zeilen) mit `[General]` (Spice-Wert, Reparaturrate, Sternenhafen-Preise, Wurm, Tarnung, Reichweitenboni …) und je einem Abschnitt pro Einheit, Gebäude und Waffe.
- Der Visual Mod Creator schreibt eine komplette `rules.txt` und lässt sie als **lose Datei** nach `Emperor\Data\Model\rules.txt` legen (Ordner `Model` anlegen). Das Spiel liest also offenbar **lose Dateien in `Data\<PREFIX>\…` statt der Archiv-Version**. Er unterscheidet dabei Mods für Gefechte und Mods nur für Kampagnen.
  - **Für uns wichtig, aber noch ungeprüft mit 1.09:** Wenn lose Dateien gehen, könnte der Mod-Ordner sie direkt kopieren, statt Archive zu bauen. Der Archiv-Weg (neuerer Zeitstempel gewinnt) bleibt der sichere und funktioniert nachweislich.
- Die Vorlage des Mod Creators ist eine Kopie der Original-`rules.txt` (EA-Daten). Sie gehört nicht ins Repo. Spieler holen sich das Original mit `modding\Extract-GameFiles.ps1`.

## 3D-Modelle (XBF)

Keines der Tools liest XBF. Für das nächste Grafik-Thema (höchste Modell-Detailstufe erzwingen) bleibt es dabei: Game.exe selbst untersuchen. Als Vorlage für das Format taugt CorrinoEngine.

## Offene Punkte aus dieser Sichtung

1. Laden lose Dateien in `Data\<PREFIX>\` mit Patch 1.09? Test: geänderte `Data\Model\rules.txt` hinlegen und im Log bzw. im Spiel prüfen.
2. BAG-Format `0x0C` (Sprache) entschlüsseln. Nur nötig, wenn wir an den Ton wollen.
3. `music.txt`: Wo liegen Musik und Sprache bei unserer Installation, und wird von Festplatte gestreamt?
4. TibEd bei Bedarf unter Windows installieren und ansehen.
