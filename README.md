# Lirah-1 Drumlogue Synth Unit

`lirah_1_drumloguePOLY` is the polyphonic drumlogue synth-unit adaptation of the Lirah-1 voice (`Lirah-1Ply` in the unit header).

## User Manual

### Sound Engine Overview

- 4-voice polyphonic FM synth engine.
- Each voice has independent FM carrier/modulator phases, Hyper-LFO phases, feedback memory, and amp envelope.
- Hyper-LFO behavior comes from two sine LFO lanes that are AND-gated (both positive = boosted frequency burst).
- Wave folding and feedback are applied in the carrier path.
- Global assignable modulation lane (`LFO TARGET` + `LFO RATE` + `LFO DEPTH`, called LFO3 internally).
- Velocity-sensitive amplitude and pressure/aftertouch FM response.
- Voice spread controls offset selected parameters across active voices.
- Spread is centered for 1/2/3/4 active voices so chords do not lean to one side.
- Envelope section includes 5 modes: `AR`, `ADSR`, `AHR`, `LOOP`, `OPEN`.

### Polyphony and Voice Allocation

- Voice count: 4.
- Allocation order on `Note On`: free slot -> oldest releasing voice -> oldest active voice (oldest-first steal).
- `Note Off` releases all matching active voices for that note.
- `All Note Off` releases all voices.
- Output mix is scaled by 0.25 to keep level consistent when chords are held.

### Parameters (19 Total)

1. `FM DEPTH` (`0..1023`): FM modulation amount.
2. `HYPER LFO` (`0..1023`): Hyper-LFO depth.
3. `LFO1 RATE` (`0..100`): Hyper-LFO lane 1 rate.
4. `LFO2 RATE` (`0..100`): Hyper-LFO lane 2 rate.
5. `FOLD` (`0..100`): Wave-fold amount.
6. `FM TUNE` (`0..100`): Modulator tuning offset.
7. `PITCH` (`0..100`): Carrier tuning offset.
8. `FEEDBACK` (`0..100`): Feedback amount.
9. `LFO TARGET` (`0..8`): Selects destination for the global LFO3 lane.
10. `LFO RATE` (`0..100`): Rate for the global LFO3 lane.
11. `LFO DEPTH` (`0..100`): Depth amount for the global LFO3 lane.
12. `FM SPREAD` (`0..100`): FM depth spread across active voices.
13. `FOLD SPRD` (`0..100`): Fold spread across active voices.
14. `FDBK SPRD` (`0..100`): Feedback spread across active voices.
15. `TUN SPREAD` (`0..100`): Tune spread (detune) across active voices.
16. `ENV TYPE` (`0..4`): Envelope mode selector.
17. `ENV RANGE` (`0..2`): Envelope speed range (`FAST`, `MED`, `SLOW`).
18. `ATTACK` (`0..100`): Envelope attack time control.
19. `RELEASE` (`0..100`): Envelope release time control.

### `LFO TARGET` Map

- `0` = `OFF`
- `1` = `FMDEP`
- `2` = `HDEP`
- `3` = `HR1`
- `4` = `HR2`
- `5` = `FOLD`
- `6` = `FMTUN`
- `7` = `OTUN`
- `8` = `FDBK`

### `ENV TYPE` Map

- `0` = `AR`
- `1` = `ADSR`
- `2` = `AHR`
- `3` = `LOOP`
- `4` = `OPEN`

### Envelope Speed Behavior (`ENV RANGE`)

The `ATTACK` and `RELEASE` knobs are remapped based on `ENV RANGE`.

- `FAST`: very short times for punchy/plucky response.
- `MED`: balanced times for general use.
- `SLOW`: long swell/tail times for ambient and drone textures.

### MIDI and Performance Behavior

- `Note On`: allocates a voice, sets note pitch, scales amp from velocity, and retriggers voice phases.
- `Gate On`: retriggers using the last received MIDI note.
- `Note Off` / `Gate Off`: releases the note envelope.
- `Pitch Bend`: +/-2 semitone range.
- `Channel Pressure` and `Aftertouch`: increase FM response depth.

## Build

Clone the project together with its pinned dependencies:

```sh
git clone --recurse-submodules https://github.com/DanielMajid/lirah_1_drumlogue.git
cd lirah_1_drumlogue
```

Set up and activate the drumlogue toolchain as described by the logue SDK, then
run:

```sh
make clean
make install
```

The project Makefile contains the complete build method from the official
drumlogue `dummy-synth` project. It uses the project-local `logue-sdk` submodule
by default; set
`LOGUE_SDK_PATH=/path/to/logue-sdk` to select another initialized SDK checkout.
The install target writes `lirah_1_drumlogue_poly.drmlgunit` to this directory;
load it into a Drumlogue synth slot with the Korg librarian.
