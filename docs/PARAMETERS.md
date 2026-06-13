# Parameters

All parameters are host-automatable and persist with the session. IDs are the
stable automation identifiers (defined once in `src/dsp/Parameters.h`).

## Global

| ID              | Name          | Range            | Default | Notes                                              |
|-----------------|---------------|------------------|---------|----------------------------------------------------|
| `input_gain`    | Input Gain    | −24 … +24 dB     | 0 dB    | Drive into the whole processor.                    |
| `vitalize_mix`  | Vitalize      | 0 … 100 %        | 35 %    | Dry/wet blend of the harmonic enhancement.         |
| `air`           | Air           | 0 … +12 dB       | 3 dB    | High-shelf at 8 kHz on the wet (harmonic) sum.     |
| `output_gain`   | Output Gain   | −24 … +24 dB     | 0 dB    | Final make-up gain.                                |
| `oversampling`  | Oversampling  | Off / 2× / 4× / 8× | 4×    | Anti-alias quality. Higher = cleaner, more CPU + latency. |
| `bypass`        | Bypass        | off / on         | off     | Latency-compensated true bypass.                   |

## Crossover

| ID               | Name           | Range          | Default  | Notes                              |
|------------------|----------------|----------------|----------|------------------------------------|
| `xover_low_mid`  | Low/Mid Freq   | 40 … 1000 Hz   | 250 Hz   | Low ↔ Mid split (log scaled).      |
| `xover_mid_high` | Mid/High Freq  | 800 … 16000 Hz | 4000 Hz  | Mid ↔ High split (log scaled).     |

The Mid/High frequency is internally kept at least 5 % above the Low/Mid
frequency to keep the bands ordered.

## Per band — Low / Mid / High

| ID (Low/Mid/High)                       | Name        | Range        | Default (L/M/H)        | Notes                                                   |
|-----------------------------------------|-------------|--------------|------------------------|---------------------------------------------------------|
| `drive_low` · `drive_mid` · `drive_high`| Drive       | 0 … 36 dB    | 6 / 7 / 9 dB           | Pre-gain into the shaper; more drive = more harmonics.  |
| `char_low` · `char_mid` · `char_high`   | Character   | Even ↔ Odd   | 0.35 / 0.50 / 0.70     | 0 = pure even (tube-like), 1 = pure odd (transistor-like). |
| `gain_low` · `gain_mid` · `gain_high`   | Gain        | −24 … +24 dB | 0 dB                   | Per-band make-up after shaping.                         |
| `mute_low` · `mute_mid` · `mute_high`   | Mute        | off / on     | off                    | Silences the band.                                      |
| `solo_low` · `solo_mid` · `solo_high`   | Solo        | off / on     | off                    | If any band is soloed, only soloed bands are heard.     |

### Defaults rationale

The factory defaults lean musical rather than neutral: the **low** band is set
warm and even-dominant (subtle weight), the **mid** band balanced, and the
**high** band brighter and odd-dominant (presence/"air"). Combined with a 35 %
Vitalize blend, the out-of-the-box sound is a gentle enhancement rather than an
obvious distortion.

## Tips

- For a classic *aural exciter* sheen: low Vitalize (20–40 %), high band Drive
  up, Character toward Odd, and a touch of Air.
- For *tape-like warmth*: low band Character toward Even with moderate Drive.
- Use **Solo** to audition exactly what each band is contributing — it is the
  harmonic content you are adding, not the full-range signal.
- Raise **Oversampling** to 8× when driving bands hard; drop to Off/2× to save
  CPU on gentle settings.
