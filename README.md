# Lirah-4 Drumlogue Synth Unit

`lirah_1_drumlogue` is a four-voice Drumlogue synth unit inspired by the Soma Laboratory Lyra-4 organismic synthesizer. Its sound engine combines two-operator FM, dual Hyper LFO lanes, wavefolding, and feedback.

# Highlights

- Four-voice FM synth engine.
- Each voice has independent FM carrier/modulator phases, Hyper-LFO phases, feedback memory, and amp envelope.
- With 4 voices actively playing, the next `Note On` played will 'steal' the slot of the oldest active voice.
- Hyper-LFO behavior comes from two sine LFO lanes that are AND-gated (both positive = boosted frequency burst).
- Wave folding and feedback are applied in the carrier path.
- Global assignable modulation lane (`LFO TARGET` + `LFO RATE` + `LFO DEPTH`, called LFO3 internally).
- Velocity-sensitive amplitude, global channel-pressure FM response, and note-specific aftertouch FM response.
- Voice spread controls offset selected parameters across active voices.
- Spread is centered for 1/2/3/4 active voices so chords do not lean to one side.
- Envelope section includes 5 modes: `AR`, `ADSR`, `AHR`, `LOOP`, `OPEN`.
- `Pitch Bend`: +/-2 semitone range.
- `Channel Pressure`: increases FM response across all active voices.
- `Aftertouch`: increases FM response only for voices playing the matching note.

# User Parameters

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

# `LFO TARGET` Map

- `0` = `OFF`
- `1` = `FMDEP` (FM Depth)
- `2` = `HDEP` (Hyper LFO Depth)
- `3` = `HR1` (Hyper LFO1 Rate)
- `4` = `HR2` (Hyper LFO2 Rate)
- `5` = `FOLD` (Wavefold Amount)
- `6` = `FMTUN` (Modulator Tuning)
- `7` = `OTUN` (Carrier Tuning)
- `8` = `FDBK` (Feedback Amount)

# `ENV TYPE` Map

- `0` = `AR`
- `1` = `ADSR`
- `2` = `AHR`
- `3` = `LOOP`
- `4` = `OPEN`

# `OPEN` Envelope Behavior

`OPEN` bypasses the normal note-controlled amplitude envelope and holds the VCA fully open.

- Selecting `OPEN` immediately opens one voice at the most recently received pitch. It uses middle C if no note has been received since startup.
- The first `Note On` replaces this temporary voice, so middle C is not layered beneath the played note.
- Further `Note On` messages allocate voices normally, up to the four-voice limit.
- Voices in `OPEN` use full amplitude and ignore `Note Off` and `Gate Off` messages.
- Changing from `OPEN` to another envelope mode releases the open voices using the current release setting.
- `All Note Off` immediately silences every voice and remains available as a panic command.

# Envelope Speed Behavior (`ENV RANGE`)

The `ATTACK` and `RELEASE` knobs are remapped based on `ENV RANGE`.

- `FAST`: very short times for punchy/plucky response.
- `MED`: balanced times for general use.
- `SLOW`: long swell/tail times for ambient and drone textures.

# Dependencies

The project requires the logue SDK sources. It
uses `./logue-sdk` by default.

# Build

From the project folder, initialize the dependencies:

```sh
git submodule update --init --recursive
```

Make sure Docker is running, then download the build image if needed:

```sh
docker pull xiashj/logue-sdk:latest
```

Compile and package the unit:

```sh
./logue-sdk/docker/run_cmd.sh --platform=. build -f --drumlogue .
```

See Korg’s [logue SDK Docker build instructions](https://github.com/korginc/logue-sdk/blob/main/docker/README.md) for the complete build-environment documentation.
