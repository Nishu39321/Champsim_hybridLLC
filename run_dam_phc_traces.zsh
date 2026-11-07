#!/bin/zsh
# Run the DAM-style hybrid-LLC configurations on every trace in TRACE_DIR and
# collect the key metrics into one CSV.
#
#   ./run_dam_phc_traces.zsh                              # all configs, one run at a time (live output)
#   PARALLEL=1 JOBS=4 ./run_dam_phc_traces.zsh            # all configs, up to 4 runs at once (logs only)
#   CONFIGS="dvs" PARALLEL=1 JOBS=4 ./run_dam_phc_traces.zsh   # only the selected configs
#
# Configs: sram_only sttram_only phc_wip dvs
# WARMUP / SIMULATION set the instruction counts (default 10M / 100M).

set -euo pipefail

CHAMPSIM_DIR="${CHAMPSIM_DIR:-/Users/nishuverma/Downloads/ChampSim}"
TRACE_DIR="${TRACE_DIR:-/Users/nishuverma/Desktop/traces}"
WARMUP="${WARMUP:-10000000}"
SIMULATION="${SIMULATION:-100000000}"
PARALLEL="${PARALLEL:-0}"
JOBS="${JOBS:-3}"
CONFIGS="${CONFIGS:-sram_only sttram_only phc_wip dvs}"

typeset -A config_bin
config_bin=(
  sram_only   "${CHAMPSIM_DIR}/bin/champsim-dam-sram-only"
  sttram_only "${CHAMPSIM_DIR}/bin/champsim-dam-sttram-only"
  phc_wip     "${CHAMPSIM_DIR}/bin/champsim-dam-phc-wip"
  dvs         "${CHAMPSIM_DIR}/bin/champsim-dam-dvs"
)

run_one() {
  local outdir="$1" workload="$2" config="$3" trace="$4"
  local bin="${config_bin[$config]}"
  local log="${outdir}/${workload}_${config}.log"
  local json="${outdir}/${workload}_${config}.json"

  print "[$(date '+%H:%M:%S')] Starting ${workload} | ${config}"
  if [[ "$PARALLEL" == "1" ]]; then
    "$bin" --warmup-instructions "$WARMUP" --simulation-instructions "$SIMULATION" \
      --json "$json" "$trace" > "$log" 2>&1 || print -u2 "FAILED: ${workload} | ${config} (see ${log})"
  else
    # 'script' keeps heartbeat lines flowing live to the terminal and the log.
    /usr/bin/script -q /dev/null \
      "$bin" --warmup-instructions "$WARMUP" --simulation-instructions "$SIMULATION" \
      --json "$json" "$trace" 2>&1 | /usr/bin/tee "$log"
  fi
  print "[$(date '+%H:%M:%S')] Finished ${workload} | ${config}"
}

# Internal entry point used by the parallel mode (one run per invocation).
if [[ "${1:-}" == "--one" ]]; then
  run_one "$2" "$3" "$4" "$5"
  exit 0
fi

typeset -a configs traces
configs=(${=CONFIGS})
traces=("${TRACE_DIR}"/*.champsimtrace.xz(N))

if (( ${#traces} == 0 )); then
  print -u2 "No .champsimtrace.xz files found in: ${TRACE_DIR}"
  exit 1
fi
for config in "${configs[@]}"; do
  if [[ -z "${config_bin[$config]:-}" ]]; then
    print -u2 "Unknown configuration: ${config} (known: ${(k)config_bin})"
    exit 1
  fi
  if [[ ! -x "${config_bin[$config]}" ]]; then
    print -u2 "Missing executable: ${config_bin[$config]}"
    print -u2 "Build the binaries before running this script."
    exit 1
  fi
done
command -v jq >/dev/null || { print -u2 "jq is required to create the CSV."; exit 1; }

RUN_ID="$(date '+%Y%m%d_%H%M%S')"
OUTDIR="${CHAMPSIM_DIR}/results/dam_stage_${RUN_ID}"
mkdir -p "$OUTDIR"
print "Results directory: ${OUTDIR}"
print "Configs: ${configs[*]}  Traces: ${#traces}  Warmup: ${WARMUP}  Simulation: ${SIMULATION}  Parallel: ${PARALLEL} (jobs ${JOBS})"

if [[ "$PARALLEL" == "1" ]]; then
  for trace in "${traces[@]}"; do
    for config in "${configs[@]}"; do
      print -r -- "$OUTDIR ${${trace:t}%.champsimtrace.xz} $config $trace"
    done
  done | xargs -P "$JOBS" -L 1 "$0" --one
else
  for trace in "${traces[@]}"; do
    for config in "${configs[@]}"; do
      run_one "$OUTDIR" "${${trace:t}%.champsimtrace.xz}" "$config" "$trace"
    done
  done
fi

CSV="${OUTDIR}/comparison.csv"
print 'workload,configuration,instructions,cycles,ipc,llc_load_misses,llc_load_mpki,llc_rfo_misses,llc_demand_misses,llc_demand_mpki,llc_miss_latency_cycles,sram_reads,sram_writes,sttram_reads,sttram_writes,phc_sram_placements,phc_sttram_placements,phc_hot_training_events,phc_cold_training_events,dam_dead_victims,dam_lru_victims,dam_dead_in_other_portion,dam_reuse_training,dam_dead_training,dam_phases,dam_phases_b' > "$CSV"

for json in "${OUTDIR}"/*.json(N); do
  filename="${json:t:r}"
  config="" workload=""
  for c in "${(k)config_bin[@]}"; do
    if [[ "$filename" == *_${c} ]]; then
      # longest matching suffix wins (e.g. sttram_only vs sram_only)
      if [[ -z "$config" || ${#c} -gt ${#config} ]]; then config="$c"; fi
    fi
  done
  [[ -z "$config" ]] && continue
  workload="${filename%_${config}}"

  jq -r --arg workload "$workload" --arg config "$config" '
    .[0].roi as $roi |
    $roi.cores[0].instructions as $instructions |
    $roi.cores[0].cycles as $cycles |
    $roi.LLC as $llc |
    ($llc["hybrid LLC"] // {}) as $h |
    ($llc.LOAD.miss[0] // 0) as $load_misses |
    ($llc.RFO.miss[0] // 0) as $rfo_misses |
    [
      $workload, $config, $instructions, $cycles,
      (if $cycles == 0 then 0 else $instructions / $cycles end),
      $load_misses,
      (if $instructions == 0 then 0 else 1000 * $load_misses / $instructions end),
      $rfo_misses,
      ($load_misses + $rfo_misses),
      (if $instructions == 0 then 0 else 1000 * ($load_misses + $rfo_misses) / $instructions end),
      ($llc["miss latency"] // 0),
      ($h["SRAM reads"] // 0), ($h["SRAM writes"] // 0), ($h["STTRAM reads"] // 0), ($h["STTRAM writes"] // 0),
      ($h["PHC SRAM placements"] // 0), ($h["PHC STTRAM placements"] // 0),
      ($h["PHC hot training events"] // 0), ($h["PHC cold training events"] // 0),
      ($h["DAM dead victims"] // 0), ($h["DAM LRU victims"] // 0), ($h["DAM dead in other portion"] // 0),
      ($h["DAM reuse training"] // 0), ($h["DAM dead training"] // 0),
      ($h["DAM dueling phases"] // 0), ($h["DAM dueling phases threshold B"] // 0)
    ] | @csv
  ' "$json" >> "$CSV"
done

print ""
print "Done: ${#traces} traces x ${#configs} configs."
print "Results directory: ${OUTDIR}"
print "Comparison CSV: ${CSV}"
