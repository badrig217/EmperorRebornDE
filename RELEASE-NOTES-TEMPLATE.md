# Vorlage für Release-Notizen / Release notes template

Für jedes neue Release in das Beschreibungsfeld auf GitHub kopieren und ausfüllen.
Konkret bleiben: was sich wirklich geändert hat und was wirklich getestet wurde.

**Ablauf / Steps:** Build unter *Actions* abwarten → Artifact `EmperorSpice-ci` laden →
`EmperorSpice-vci.zip` in `EmperorSpice-vX.Y.zip` umbenennen → *Releases → Draft a new release* →
Tag `vX.Y` (Target `main`) → Text einfügen, ZIP anhängen → *Set as the latest release* → veröffentlichen.
Vorher die Versionsnummer in `EmperorLauncher.cpp` („Emperor Spice vX.Y“) und `package.ps1` anpassen.

---

## Emperor Spice vX.Y

**Deutsch** · [English below](#english)

### Neu
- (eine Zeile pro Änderung, z. B. „Bodentexturen in doppelter Auflösung“)
- (Bekannte Probleme oder Halbfertiges hier ehrlich nennen)

### Getestet
- (z. B. „Windows 11, deutsche Version, Installation aus ISO“)

### Installation
ZIP in einen Ordner ohne Umlaute entpacken, eigenen Patch `EM109DE.EXE` / `EM109EN.EXE` daneben legen,
`EmperorSpice.exe` starten, CDs oder ISO-Dateien wählen. Windows-Warnung: *Weitere Informationen → Trotzdem ausführen*.
Spiel und Patch 1.09 sind **nicht** enthalten.

---

<a id="english"></a>
**English**

- (the same changes in English)

Install: extract the zip, put your own `EM109EN.EXE` / `EM109DE.EXE` next to `EmperorSpice.exe`, run it and
choose CDs or ISO files. SmartScreen: *More info → Run anyway*. The game and the v1.09 patch are **not** included.

---

Based on [Emperor Reborn](https://github.com/azmawee/EmperorReborn) by azmawee and
[EmperorLauncher](https://github.com/wheybags/EmperorLauncher) by wheybags. Unofficial fan project,
not affiliated with Electronic Arts / Westwood.
