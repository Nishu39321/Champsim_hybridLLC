# DAM configuration with a PHC placement baseline

These three one-core configurations use the geometry and timing stated in the
DAM paper: 32 KiB L1I (4-way), 32 KiB L1D (8-way), 256 KiB L2 (8-way), and an
8 MiB 16-way LLC with 4 SRAM and 12 STT-RAM ways for the hybrid design. The
technology-array latencies are 30/30 cycles for SRAM and 30/120 cycles for
STT-RAM reads/writes. Eight read and eight write ports are configured per
technology, with matching LLC tag/fill bandwidth.

| File | LLC composition | Placement policy |
| --- | --- | --- |
| `dam_sram_only.json` | 16 SRAM ways | SRAM baseline |
| `dam_sttram_only.json` | 16 STT-RAM ways | STT-RAM baseline |
| `dam_phc_wip.json` | 4 SRAM + 12 STT-RAM ways | PHC write-intensity predictor |

`dam_phc_wip.json` implements the first project stage only. It uses PHC's
4,096-entry two-bit predictor, initialized weakly cold, and a 1/32-set plain
LRU sampler. The sampler trains on eviction using an 8-bit signed cost with
read delta -1, write delta +24, and threshold 20. A predicted-hot demand fill
is placed in SRAM; a predicted-cold one is placed in STT-RAM. Prefetches are
placed in STT-RAM and writebacks, which do not have a useful trigger PC, are
placed in SRAM.

There is no dead-block predictor, migration, or threshold selector in this
stage. `hybrid_lru` selects a victim only within the medium selected at miss
allocation. The decision is stored in the MSHR/fill entry and is used for the
later fill and medium timing.

Build one configuration at a time:

```sh
./config.sh configs/dam_phc_wip.json
make
```
