# Lirah-1 Drumlogue Synth Unit

`lirah_1_drumlogue` is a drumlogue synth-unit adaptation of the Lirah-1 voice.

## User Manual

### Sound engine overview

- Sine-carrier + sine-modulator FM core.
- Hyper-LFO behavior from two LFO lanes that are AND-gated.
- Wave folding and feedback in the carrier path.
- Extra assignable modulation lane (`LFO TARGET` + `LFO RATE`).
- Velocity-sensitive amplitude and pressure/aftertouch FM response.

### Parameters

1. `FM DEPTH` (`0..1023`): FM modulation amount.
2. `HYPER LFO` (`0..1023`): Depth of Hyper-LFO pitch movement.
3. `LFO1 RATE` (`0..100`): Hyper-LFO lane 1 rate.
4. `LFO2 RATE` (`0..100`): Hyper-LFO lane 2 rate.
5. `FOLD` (`0..100`): Wave-fold amount.
6. `FM TUNE` (`0..100`): Modulator tuning offset.
7. `PITCH` (`0..100`): Carrier tuning offset.
8. `FEEDBACK` (`0..100`): Feedback amount.
9. `LFO TARGET` (`0..8`): Selects destination for the extra modulation lane.
10. `LFO RATE` (`0..100`): Rate for the extra modulation lane.

### `LFO TARGET` map

- `0` = `OFF`
- `1` = `FMDEP`
- `2` = `HDEP`
- `3` = `HR1`
- `4` = `HR2`
- `5` = `FOLD`
- `6` = `FMTUN`
- `7` = `OTUN`
- `8` = `FDBK`

### Performance behavior

- `Note On`: sets pitch and amplitude from velocity.
- `Note Off`/`All Note Off`: releases envelope.
- `Pitch Bend`: ±2 semitone range.
- `Channel Pressure`/`Aftertouch`: scales FM depth response.

## Build Instructions

### Prerequisites

- A working ARM cross toolchain that supports:
  - `-march=armv7-a`
  - `-mfpu=neon-vfpv4`
  - `-mfloat-abi=hard`
- GNU Make

### Build commands

From repository root:

```sh
cd drumlogue-lirah-1
make clean
make CROSS_COMPILE=arm-none-eabi-
make CROSS_COMPILE=arm-none-eabi- install
```

Build outputs are generated in `drumlogue-lirah-1/build/`, and install places:

- `drumlogue-lirah-1/lirah_1_drumlogue.drmlgunit`

### Load on drumlogue

Use the drumlogue unit loader workflow to import `lirah_1_drumlogue.drmlgunit` into a synth slot.
