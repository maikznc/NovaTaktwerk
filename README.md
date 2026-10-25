# NOVA Taktwerk

Drum-Machine-Plugin von NOVA, gebaut für harte Techno-Produktionen: 12 Spuren, jede mit einer
synthetisierten Drum-Stimme und einem Sample-Layer obendrauf. Pro Spur vier Seiten mit je 8 Reglern
(SYNTH, SAMPLE, FILTER, AMP) wie bei klassischen Hardware-Drum-Machines, dazu eine FX-/Master-Sektion mit
Distortion, Kompressor, Reverb (inkl. Rumble-Ducking) und Delay.

Formate: **VST3** (Mac + Windows), **AU** (Mac), **Standalone-App** (Mac + Windows).
Gebaut mit JUCE 9.0.4 und CMake.

## Bedienung

| Aktion | Was passiert |
|---|---|
| Klick auf ein Pad | Spielt die Spur. Je weiter oben du klickst, desto lauter (Velocity). |
| Doppelklick auf ein Pad | Öffnet den Sound-Editor der Spur |
| `•••` auf dem Pad oder Rechtsklick | Menü: Sound bearbeiten, Vorhören, Sample laden/entfernen, Spur zurücksetzen |
| Audiodatei aufs Pad ziehen | Lädt sie als Sample-Layer (WAV, AIFF, FLAC, OGG, MP3) |
| Doppelklick auf einen Regler | Setzt ihn auf den Standardwert zurück |
| `X` oder Klick neben den Editor | Schließt den Sound-Editor |

Die Pads leuchten auch, wenn sie per MIDI aus der DAW gespielt werden. Die zwei Balken unten auf
jedem Pad zeigen den Pegel von Synth (türkis) und Sample (koralle).

## Kits

Oben im Kit-Menü (auch als Programme in der DAW):

| Kit | Charakter |
|---|---|
| **Hard Techno** | Standard. Kick mit Hold, langem Sweep, Wave und Overdrive, leichte Master-Distortion, Kompressor als Kleber |
| **Rumble** | Kick in den Hall, Hall verzerrt (Reverb-Drive), tiefpassgefiltert und unter der Kick weggeduckt |
| **Industrial** | Alles übersteuert, asymmetrische Master-Distortion, Kick per Amp-Hüllkurve abgehackt |
| **Clean** | Ohne Dreck und Kompressor, zum Vergleichen oder als Basis für eigene Sounds |

Ein Kit-Wechsel setzt alle Regler neu, geladene Samples bleiben.

## Sound-Editor: 4 Seiten à 8 Regler

**SYNTH**, abhängig von der Stimme:

| Stimme | Regler |
|---|---|
| Kick (BD) | Level, Tune, Decay, **Hold**, **Sweep** (Tiefe), **Sw.Time**, **Snap** (Click), **Wave** (Sinus → übersteuert) |
| Toms (BT/LT/MT/HT) | wie Kick |
| Snare (SD) | Level, Tune, Decay, Detune, Snap (Rauschen), N.Decay, N.Tone, Sweep |
| Rimshot (RS) | Level, Tune, Decay, Balance, Interval, Snap, Sweep, Cut |
| Clap (CP) | Level, Tune, Decay, Tone, Claps (Anzahl), Rate, Snap, Body |
| Hats/Cymbal (CH/OH/CY) | Level, Tune, Decay, Tone, Cut, Color, Noise, Snap |
| Cowbell (CB) | Level, Tune, Decay, Interval, Tone, Color, Snap, Hold |

**SAMPLE:** Level, Tune, Fine, Bit Rd, Decim, Start, End (End vor Start = rückwärts), Loop

**FILTER:** Attack, Decay, Sustain, Release (eigene Hüllkurve), Freq, Reso, Type (LP 24, LP 12, BP, HP 12, HP 24,
BS, PK), Env (±7 Oktaven)

**AMP:** Attack, Hold, Decay (ganz rechts = unendlich), Drive (Overdrive der Spur), Delay-Send, Reverb-Send, Pan, Volume

Signalweg pro Spur: Synth + Sample → Filter → Overdrive → Amp-Hüllkurve → Pan/Volume → Sends.
Die Amp-Hüllkurve sitzt nach dem Overdrive: So lässt sich eine verzerrte Kick sauber abschneiden, bevor die
nächste kommt.

## FX & Master

Button **FX / MASTER** oben rechts.

- **Distortion:** Drive, Sym (Symmetrie, asymmetrisch = mehr Fuzz), Tone, Rev Post (Aus = Hall läuft durch Distortion und Kompressor)
- **Reverb:** Pre-Dly, Decay, Damp, HPF, LPF, **Drive** (verzerrt nur den Hall), **Duck** (duckt den Hall unter den trockenen Sounds), Volume
- **Kompressor:** Threshold, Ratio, Attack, Release, Makeup, SC HPF, Mix (parallel), mit Gain-Reduction-Anzeige
- **Delay:** Time (im Songtempo, 1/32 bis 1/2, triolisch und punktiert), Ping-Pong, Feedback, HPF, LPF, Volume

**Rumble selbst bauen:** Kick → AMP-Seite → Reverb-Send hoch. Im FX-Fenster Reverb-LPF auf 250–400 Hz,
Decay hoch, Drive nach Geschmack, Duck auf 80–90 %. Fertig ist das Pumpen.

Die Closed Hat (CH) schneidet die Open Hat (OH) ab. Alle Regler sind in der DAW automatisierbar
(z. B. „BD Synth Sweep", „Reverb Duck").

## MIDI-Belegung (General MIDI)

| Spur | Note | | Spur | Note |
|---|---|---|---|---|
| BD Kick | 36 (C1) | | LT Low Tom | 43 (G1) |
| RS Rimshot | 37 (C#1) | | MT Mid Tom | 45 (A1) |
| SD Snare | 38 (D1) | | OH Open Hat | 46 (A#1) |
| CP Clap | 39 (D#1) | | HT High Tom | 47 (B1) |
| BT Bass Tom | 41 (F1) | | CY Cymbal | 49 (C#2) |
| CH Closed Hat | 42 (F#1) | | CB Cowbell | 56 (G#2) |

Zusätzlich: 35 → BD, 40 → SD, 44 → CH, 48/50 → HT, 51/57 → CY. Damit funktionieren die
Drum-Editoren der meisten DAWs direkt. (Notennamen mit C3 = 60, wie in Ableton, Cubase, Logic.)

## Bauen

### Variante A: GitHub baut für dich (Mac und Windows)

1. Projektordner in ein GitHub-Repository hochladen.
2. Unter **Actions** läuft „Build NOVA Taktwerk" automatisch bei jedem Push
   (oder manuell über „Run workflow").
3. Nach ca. 10–15 Minuten liegen unten im Lauf zwei Downloads: `NOVA-Taktwerk-macOS` und
   `NOVA-Taktwerk-Windows`.

Der Mac-Build ist ein Universal Binary (Apple Silicon + Intel), ab macOS 11.

### Variante B: lokal auf dem Mac

Voraussetzungen: Xcode (oder die Command Line Tools) und CMake (`brew install cmake`).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Beim ersten Mal lädt CMake JUCE automatisch herunter. Mit `-DNOVA_COPY_PLUGIN_AFTER_BUILD=ON`
wird das Plugin nach dem Bauen direkt installiert.

### Variante C: lokal unter Windows

Voraussetzungen: Visual Studio 2022 (Workload „Desktopentwicklung mit C++") und CMake.

```bat
cmake -S . -B build
cmake --build build --config Release --parallel
```

Die fertigen Dateien liegen in `build/NovaTaktwerk_artefacts/Release/`.

## Installieren

| | Mac | Windows |
|---|---|---|
| VST3 | `~/Library/Audio/Plug-Ins/VST3/` | `C:\Program Files\Common Files\VST3\` |
| AU | `~/Library/Audio/Plug-Ins/Components/` | – |
| Standalone | beliebig, z. B. Programme | beliebig |

**Mac, wenn das Plugin aus GitHub geladen wurde:** macOS blockiert unsignierte Downloads. Einmal im
Terminal ausführen, dann DAW neu starten:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"NOVA Taktwerk.vst3"
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"NOVA Taktwerk.component"
```

Für die Weitergabe an andere braucht es später eine Apple-Developer-Signatur mit Notarisierung.

## Markenangaben

In `CMakeLists.txt` oben stehen Firma (`NOVA`), Produktname, Herstellercode (`Nova`) und
Plugin-Code (`Tkwk`). Der **Herstellercode sollte bei allen NOVA-Plugins gleich sein**, z. B. wie bei
NOVA Reese, damit DAWs sie zusammen unter NOVA einsortieren. Der Plugin-Code muss pro Plugin
eindeutig sein. Wichtig: beides vor der ersten Veröffentlichung festlegen, danach nicht mehr
ändern, sonst finden DAWs gespeicherte Projekte nicht wieder.

## Projektstruktur

```
Source/
  TrackInfo.h            die 12 Spuren (Name, Stimmtyp, Grundton, MIDI-Note)
  ParamDefs.h            alle Regler mit Bereichen und Standardwerten (Seiten + FX)
  Parameters.h           daraus die Plugin-Parameter für die DAW
  Kits.h                 die Werks-Kits
  PluginProcessor.*      Audio, MIDI, Samples, Speichern/Laden
  PluginEditor.*         Hauptfenster
  DSP/DrumSynth.*        Synthese der Stimmen (SYNTH-Seite)
  DSP/DrumTrack.*        Synth + Sample -> Filter -> Overdrive -> Amp -> Pan/Sends
  DSP/SampleData.h       Sample-Layer (SAMPLE-Seite)
  DSP/MasterFx.*         Delay, Reverb, Distortion, Kompressor
  UI/PadComponent.*      die Pads mit •••-Menü
  UI/TrackEditorPanel.*  der Sound-Editor mit den 4 Seiten
  UI/FxPanel.*           das FX-/Master-Fenster
  UI/NovaLookAndFeel.*   NOVA-Farben, Regler, Logo
Tools/
  RenderTest.cpp         prüft alle Stimmen, Filter, Hüllkurven, Sample-Layer und FX
  Snapshot.cpp           prüft MIDI, Kits, Speichern, rendert einen Groove pro Kit (WAV) und die Oberfläche (PNG)
```

Die Tests baust du mit `-DNOVA_BUILD_TOOLS=ON`.

## Nächste Schritte

1. LFO pro Spur (wie auf der Hardware) für Bewegung in Filter, Tune, Pan
2. Eigene Kits speichern und laden
3. Eigener Step-Sequencer mit Parameter-Locks
4. Performance-Macros und Scenes auf den Pads
