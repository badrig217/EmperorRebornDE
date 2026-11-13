# Grafik-Preset „HD“ (experimentell)

Stand: Test 2 ist beim Tester, die Rückmeldung steht aus. Details in `docs/PROJECT-STATUS.md`.

Zum Ausprobieren in den `data`-Ordner der Installation kopieren:
- `Emperor_HD.ini` → als `Emperor_Modern.ini` (ersetzt das Standard-Preset)
- `reshade-shaders/Shaders/EmperorSpice/EmperorBloom.fx` → gleicher Pfad unter `data`
- Glamarye (MIT): `Shaders/Glamayre_Fast_Effects.fx` aus
  https://github.com/rj200/Glamarye_Fast_Effects_for_ReShade (Commit 9dd9b826fa2cbea818ef1bc487e5f2e7f427c750)
  nach `data/reshade-shaders/Shaders/Glamarye/`
- In `data/dgVoodoo.conf`: `[DirectX] Resolution = 2x` und `[GeneralExt] Resampling = lanczos-3` (Supersampling)

Tasten: Rollen = Effekte an/aus, Ende = Tiefenpuffer anzeigen (Diagnose), Pos1 = ReShade-Menü.
Zurück zum Standard: `dgVoodoo.conf` und `Emperor_Modern.ini` im `data`-Ordner löschen. Der Launcher legt sie neu an.
