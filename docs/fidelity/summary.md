# Measured PCM results

| Fixture / profile | PCM | Frames (seconds) | Unequal samples | Max error (LSB) | Worst window relative / SNR | Verdict |
| --- | --- | ---: | ---: | ---: | --- | --- |
| native-ts.pt3 | 48000 Hz / 16-bit / 2 ch | 30,723 (0.6400625) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |
| effects-portamento.pt3 | 48000 Hz / 16-bit / 2 ch | 40,324 (0.8400833) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |
| effects-env-slide.pt3 | 48000 Hz / 16-bit / 2 ch | 37,444 (0.7800833) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |
| effects-gliss.pt3 | 48000 Hz / 16-bit / 2 ch | 46,085 (0.9601042) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |
| effects-vibrato.pt3 | 48000 Hz / 16-bit / 2 ch | 37,444 (0.7800833) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |
| effects-tempo.pt3 | 48000 Hz / 16-bit / 2 ch | 23,042 (0.4800417) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |
| noise-slide.pt3 | 48000 Hz / 16-bit / 2 ch | 37,444 (0.7800833) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |
| all-shapes-and-retrigger.psg | 48000 Hz / 16-bit / 2 ch | 61,446 (1.2801250) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |
| skip.psg | 48000 Hz / 16-bit / 2 ch | 13,441 (0.2800208) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |
| unknown-registers.psg | 48000 Hz / 16-bit / 2 ch | 4,800 (0.1000000) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |
| all-shapes.ym | 48000 Hz / 16-bit / 2 ch | 61,446 (1.2801250) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |
| all-shapes-loop.ym | 48000 Hz / 16-bit / 2 ch | 61,446 (1.2801250) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |
| Flexo02 | 48000 Hz / 16-bit / 2 ch | 2,457,879 (51.2058125) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |
| qualified-source-oracle | 48000 Hz / 16-bit / 2 ch | 2,457,879 (51.2058125) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |
| AY-48k | 48000 Hz / 16-bit / 2 ch | 2,457,879 (51.2058125) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |
| YM-22k | 22050 Hz / 16-bit / 2 ch | 1,129,087 (51.2057596) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |
| YM-96k | 96000 Hz / 16-bit / 2 ch | 4,915,758 (51.2058125) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |
| YM-8k | 8000 Hz / 16-bit / 2 ch | 409,646 (51.2057500) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |
| YM-2MHz | 48000 Hz / 16-bit / 2 ch | 2,457,602 (51.2000417) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |
| YM-averager | 48000 Hz / 16-bit / 2 ch | 2,457,879 (51.2058125) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |
| YM-preamp255 | 48000 Hz / 16-bit / 2 ch | 2,457,879 (51.2058125) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |
| AY-Pentagon | 48000 Hz / 16-bit / 2 ch | 2,516,579 (52.4287292) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |
| header-v3-table0 | 48000 Hz / 16-bit / 2 ch | 40,324 (0.8400833) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |
| header-v3-table2 | 48000 Hz / 16-bit / 2 ch | 40,324 (0.8400833) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |
| header-v5-table0 | 48000 Hz / 16-bit / 2 ch | 40,324 (0.8400833) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |
| header-v6-table3 | 48000 Hz / 16-bit / 2 ch | 40,324 (0.8400833) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |
| header-v7-table1 | 48000 Hz / 16-bit / 2 ch | 40,324 (0.8400833) | 0 | 0 | 0 / ∞ (zero error) | **BIT_EXACT_PCM** |

Measured 2026-10-04T04:15:01.509350+00:00 against source revision `a43c485766d237fa4891d0c07f74f27506804ed2`; renderer SHA-256 `8d6207f4c4933931725473351139946a8e3b6719d95e22f51dad801402e41d75`.
