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

## Corrections after review (Stage 1, rev. 2)

1. **Array write off the miss critical path.** Returned miss data (and
   writeback-miss data) is installed after the normal `fill_latency`. The
   array write still occupies a port of the destination medium for its full
   latency (30 cycles SRAM, 120 cycles STT-RAM).
2. **Shared ports per medium.** Reads and writes of one medium share the same
   ports, so a bank busy with an STT-RAM write delays later reads.
   `*_read_ports` sets the number of shared ports; `*_write_ports` is ignored.
3. **One training event per access.** The PHC sampler is updated only when a
   tag check completes (hit, accepted miss, or accepted writeback), not on
   every retry of a miss stalled by a full MSHR. Placement is predicted once,
   when a new MSHR entry is allocated, so merged misses no longer count as
   placements.
4. **Initial cost.** A sampler entry starts at cost 0 and the access that
   inserts it is not counted (PHC Fig. 3).
5. **Dirty data on merge.** A load that merges into an in-flight writeback
   keeps the line dirty.

The configuration files now set `executable_name`, so each build writes its
own binary directly:

```sh
for c in sram_only sttram_only phc_wip; do
  ./config.sh configs/dam_$c.json && make -j8 || break
done
```

## Stage 2: DAM dead-block predictor + DVS (`dam_dvs.json`)

Placement is unchanged from Stage 1 (PHC's 4,096-entry write-intensity table).
Stage 2 changes only victim selection:

* **Dead-block predictor (DAM Sec. 3).** A 256-entry, 8-bit reuse-state table
  (RST) indexed by an 8-bit XOR hash of the PC. Every LLC block stores the
  hashed PC of the instruction that brought it in (writeback fills have no PC
  and are never predicted dead). The RST is trained by the shared 1/32-set
  sampler: a demand hit to a sampled block increments its PC's entry, and a
  block evicted from the sampler decrements it. A block is predicted dead
  when `RST[pc] < threshold`.
* **Set-dueling threshold.** 32 leader sets use `dam_threshold_a` (64) and 32
  use `dam_threshold_b` (128); every `dam_dueling_phase` (4,096) demand misses
  the group with fewer misses sets the threshold for all other sets.
* **DVS.** On a fill, an invalid way in the chosen portion is used first; then
  the least recently used predicted-dead block in that portion; otherwise
  plain LRU in that portion. No migration yet (Stage 3).

New statistics: `DAM dead victims`, `DAM LRU victims`, `DAM dead in other
portion` (LRU evictions where a dead block existed in the *other* portion:
the opportunity Stage 3's migration targets), RST training counts and the
number of set-dueling phases won by threshold B.

## Stage 2b: non-inclusive corrections to the dead-block predictor

ChampSim's LLC is non-inclusive, so an L2 writeback can miss in the LLC (in
DAM's inclusive LLC it always hits). Two switches address this:

* `dam_writeback_reuse`: a writeback hit to a sampled block trains as reuse
  (evicting a line that will receive a writeback forces a writeback-miss
  allocation).
* `dam_nopc_dead`: lines filled by a writeback miss (no trigger PC) are
  predicted dead instead of never dead.

| Config | `dam_writeback_reuse` | `dam_nopc_dead` |
| --- | --- | --- |
| `dam_dvs.json` (Stage 2) | off | off |
| `dam_dvs2b.json` | on | on |
| `dam_dvs2b_wb.json` | on | off |
| `dam_dvs2b_nopc.json` | off | on |

## Stage 3: DAM dead-block-based migration (`dam_dm.json`)

Built on `dam_dvs2b_wb.json` (PHC placement + dead-block predictor with
writeback hits counted as reuse) plus `dam_dm`. On a fill into a full
portion (DAM Fig. 4a):

1. A predicted-dead block in the target portion is evicted (DVS).
2. Otherwise, if the *other* portion has a predicted-dead block (the least
   recently used one is chosen), it is evicted and a block of the target
   portion is migrated into its slot; the incoming block takes the freed way:
   * incoming block -> SRAM: the most read-intensive SRAM block (lowest
     per-block cost; ties: least recently used) moves to STT-RAM;
   * incoming block -> STT-RAM: the most write-intensive STT-RAM block
     (highest cost; ties: most recently used) moves to SRAM.
3. Otherwise LRU within the target portion.

Per-block cost: 8-bit signed, reset to 0 on fill, +24 per write hit and -1
per read hit (the PHC deltas). The migrated block keeps its dirty bit,
predictor metadata, cost and LRU position. Migration accounting: one read
of the source medium and one write of the destination medium (counted in the
SRAM/STT-RAM read/write totals and reserving their ports, off the critical
path), plus `DAM migrations to SRAM` / `DAM migrations to STTRAM`. A dirty
dead block is written back as for a normal eviction.
