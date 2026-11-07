#!/bin/zsh
# Run the DAM-style SRAM, STT-RAM, and PHC-baseline configurations on every
# trace in TRACE_DIR. Output is live on the terminal and is saved per run.

set -euo pipefail

CHAMPSIM_DIR="/Users/nishuverma/Downloads/ChampSim"
TRACE_DIR="/Users/nishuverma/Desktop/traces"
WARMUP="${WARMUP:-10000000}"
SIMULATION="${SIMULATION:-100000000}"
RUN_ID="$(date '+%Y%m%d_%H%M%S')"
OUTDIR="${CHAMPSIM_DIR}/results/dam_phc_stage1_${RUN_ID}"

typeset -a config_names config_bins traces
config_names=(sram_only sttram_only phc_wip)
config_bins=(
  "${CHAMPSIM_DIR}/bin/champsim-dam-sram-only"
  "${CHAMPSIM_DIR}/bin/champsim-dam-sttram-only"
  "${CHAMPSIM_DIR}/bin/champsim-dam-phc-wip"
)
traces=("${TRACE_DIR}"/*.champsimtrace.xz(N))

if (( ${#traces} == 0 )); then
  print -u2 "No .champsimtrace.xz files found in: ${TRACE_DIR}"
  exit 1
fi

for bin in "${config_bins[@]}"; do
  if [[ ! -x "$bin" ]]; then
    print -u2 "Missing executable: $bin"
    print -u2 "Build the Stage-1 binaries before running this script."
    exit 1
  fi
done

command -v jq >/dev/null || { print -u2 "jq is required to create the CSV."; exit 1; }
mkdir -p "$OUTDIR"

run_one() {
  local workload="$1"
  local config="$2"
  local bin="$3"
  local trace="$4"
  local log="${OUTDIR}/${workload}_${config}.log"
  local json="${OUTDIR}/${workload}_${config}.json"

  print ""
  print "============================================================"
  print "Starting workload: ${workload} | configuration: ${config}"
  print "Log: ${log}"
  print "============================================================"

  # Do not add --hide-heartbeat. 'script' makes line-buffered heartbeats
  # visible in both this terminal and the log file as they are produced.
  /usr/bin/script -q /dev/null \
    "$bin" \
    --warmup-instructions "$WARMUP" \
    --simulation-instructions "$SIMULATION" \
    --json "$json" \
    "$trace" 2>&1 | /usr/bin/tee "$log"

  print "Finished workload: ${workload} | configuration: ${config}"
}

for trace in "${traces[@]}"; do
  trace_name="${trace:t}"
  workload="${trace_name%.champsimtrace.xz}"

  for index in {1..${#config_names}}; do
    run_one "$workload" "${config_names[index]}" "${config_bins[index]}" "$trace"
  done
done

CSV="${OUTDIR}/stage1_comparison.csv"
print 'workload,configuration,instructions,cycles,ipc,llc_load_misses,llc_load_mpki,llc_rfo_misses,llc_demand_misses,llc_demand_mpki,llc_miss_latency_cycles,sram_reads,sram_writes,sttram_reads,sttram_writes,phc_sram_placements,phc_sttram_placements,phc_hot_training_events,phc_cold_training_events' > "$CSV"

for json in "${OUTDIR}"/*.json(N); do
  filename="${json:t}"
  config="${filename##*_}"
  config="${config%.json}"
  workload="${filename%_*}"

  # phc_wip includes an underscore, so recover it explicitly from the suffix.
  if [[ "$filename" == *_phc_wip.json ]]; then
    config="phc_wip"
    workload="${filename%_phc_wip.json}"
  elif [[ "$filename" == *_sttram_only.json ]]; then
    config="sttram_only"
    workload="${filename%_sttram_only.json}"
  elif [[ "$filename" == *_sram_only.json ]]; then
    config="sram_only"
    workload="${filename%_sram_only.json}"
  fi

  jq -r --arg workload "$workload" --arg config "$config" '
    .[0].roi as $roi |
    $roi.cores[0].instructions as $instructions |
    $roi.cores[0].cycles as $cycles |
    $roi.LLC as $llc |
    ($llc["hybrid LLC"] // {}) as $hybrid |
    ($llc.LOAD.miss[0] // 0) as $load_misses |
    ($llc.RFO.miss[0] // 0) as $rfo_misses |
    [
      $workload,
      $config,
      $instructions,
      $cycles,
      (if $cycles == 0 then 0 else $instructions / $cycles end),
      $load_misses,
      (if $instructions == 0 then 0 else 1000 * $load_misses / $instructions end),
      $rfo_misses,
      ($load_misses + $rfo_misses),
      (if $instructions == 0 then 0 else 1000 * ($load_misses + $rfo_misses) / $instructions end),
      ($llc["miss latency"] // 0),
      ($hybrid["SRAM reads"] // 0),
      ($hybrid["SRAM writes"] // 0),
      ($hybrid["STTRAM reads"] // 0),
      ($hybrid["STTRAM writes"] // 0),
      ($hybrid["PHC SRAM placements"] // 0),
      ($hybrid["PHC STTRAM placements"] // 0),
      ($hybrid["PHC hot training events"] // 0),
      ($hybrid["PHC cold training events"] // 0)
    ] | @csv
  ' "$json" >> "$CSV"
done

print ""
print "All 12 runs completed."
print "Results directory: ${OUTDIR}"
print "Comparison CSV: ${CSV}"
