# Mods für Emperor Spice

**Deutsch** · [English below](#english)

Mit Emperor Spice kannst du Dateien des Spiels ersetzen, ohne seine Archive anzufassen. Du legst einfach Dateien in einen Ordner.

## So funktioniert es

Neben `EmperorSpice.exe` liegt der Ordner **`mods`**. Im Launcher kommst du über „Mod-Ordner öffnen“ hin. Jede Mod ist dort ein eigener Unterordner, darin ein Ordner pro Spiel-Archiv und darin die Dateien mit ihrem Pfad im Spiel:

```
mods\
  MeineMod\
    MODEL\Rules.txt                    ← Regeln: Kosten, Bauzeiten, Stärke von Einheiten …
    3DDATA\Textures\AT_Windtrap.tga    ← eine Textur bzw. ein Icon
    UI\CAMERA.INI                      ← Kamera-Einstellungen
```

Bei jedem Klick auf **Spielen** packt der Launcher alle Mods in eigene Archive, die das Spiel statt der Originale benutzt. Die Originaldateien bleiben unverändert.

- **Mod ausschalten:** einen Unterstrich vor den Ordnernamen setzen (`_MeineMod`), oder alle Mods im Launcher unter „Extras“ abhaken.
- **Mehrere Mods mit derselben Datei:** Es gewinnt die Mod, die im Alphabet weiter hinten steht. Mit Nummern lässt sich die Reihenfolge festlegen, z. B. `10_Basis`, `20_Feinschliff`.
- **Multiplayer:** Alle Spieler müssen dieselben Mods aktiv haben, sonst laufen die Partien auseinander.

## Die Originaldateien als Vorlage holen

Doppelklick auf **`modding\Extract game files.cmd`**. Das Skript kopiert die Dateien aus deiner Installation nach `mods\_Original\`. Den Ordner ignoriert der Launcher wegen des Unterstrichs. Kopiere von dort die Dateien, die du ändern willst, in deine eigene Mod und bearbeite sie.

- Es wird jeweils die neueste Fassung genommen, z. B. `Rules.txt` aus dem Patch 1.09.
- Die Karten (MAPS, ca. 1 GB) werden nur mit `-All` mit entpackt.

## Was sich ändern lässt

| Archiv | Inhalt |
|---|---|
| `MODEL` | `Rules.txt`: Einheiten, Gebäude, Kosten, Bauzeiten, Waffen, Panzerung |
| `3DDATA` | `Textures\*.tga` (Texturen, Seitenleisten-Icons), Modelle `*.xbf`, Animationen `*.xaf` |
| `UI` | Menüs, `CAMERA.INI`, Schriftzuordnungen |
| `STRINGS` | Texte des Spiels |
| `AI` | KI-Skripte und -Einstellungen |
| `MAPS` | Karten |

Texturen: TGA, 24 oder 32 Bit. Magenta (255, 0, 255) bedeutet durchsichtig. Größere Texturen als im Original sind meist möglich.

## Mods weitergeben

Gib nur deine eigenen geänderten Dateien weiter, keine kompletten Spielarchive. Die Spieldaten gehören EA. Wer deine Mod nutzt, hat das Spiel ja selbst.

---

<a id="english"></a>
## English

Put files into **`mods\<YourMod>\<ARCHIVE>\<path in the game>`** (e.g. `mods\MyMod\MODEL\Rules.txt`, `mods\MyMod\3DDATA\Textures\AT_Windtrap.tga`). Each time you click **Play**, the launcher packs all mods into extra archives that the game uses instead of the originals. The original files are never changed.

- Disable a mod: prefix its folder with `_`, or untick mods under "Extras" in the launcher.
- Same file in several mods: the mod that comes later alphabetically wins (use `10_`, `20_` …).
- Multiplayer: everybody needs the same mods.
- Starting point: double-click `modding\Extract game files.cmd`. It copies the game's own files (newest version each) to `mods\_Original\`, which the launcher ignores. Add `-All` to include the maps (~1 GB).
- Share only your own changed files, never whole game archives (the game data belongs to EA).
