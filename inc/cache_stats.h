#ifndef CACHE_STATS_H
#define CACHE_STATS_H

#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

#include "channel.h"
#include "event_counter.h"

struct cache_stats {
  std::string name;
  // prefetch stats
  uint64_t pf_requested = 0;
  uint64_t pf_issued = 0;
  uint64_t pf_useful = 0;
  uint64_t pf_useless = 0;
  uint64_t pf_fill = 0;

  uint64_t sram_reads = 0;
  uint64_t sram_writes = 0;
  uint64_t sttram_reads = 0;
  uint64_t sttram_writes = 0;
  uint64_t phc_sram_placements = 0;
  uint64_t phc_sttram_placements = 0;
  uint64_t phc_hot_training_events = 0;
  uint64_t phc_cold_training_events = 0;
  // DAM dead-block predictor / DVS (Stage 2)
  uint64_t dam_dead_victims = 0;         // evictions where DVS chose a predicted-dead block
  uint64_t dam_lru_victims = 0;          // evictions with no dead block in the target portion (LRU fallback)
  uint64_t dam_dead_in_other_portion = 0; // ...of those, a dead block existed in the other portion (Stage 3 opportunity)
  uint64_t dam_reuse_training = 0;       // reuse-state table increments (sampler hits)
  uint64_t dam_dead_training = 0;        // reuse-state table decrements (sampler evictions)
  uint64_t dam_phases = 0;               // set-dueling phases completed
  uint64_t dam_phases_b = 0;             // phases after which followers used threshold B
  // DAM dead-block-based migration (Stage 3)
  uint64_t dam_migrations_to_sram = 0;   // STT-RAM block moved into a dead SRAM slot (incoming block -> STT-RAM)
  uint64_t dam_migrations_to_sttram = 0; // SRAM block moved into a dead STT-RAM slot (incoming block -> SRAM)

  champsim::stats::event_counter<std::pair<access_type, std::remove_cv_t<decltype(NUM_CPUS)>>> hits = {};
  champsim::stats::event_counter<std::pair<access_type, std::remove_cv_t<decltype(NUM_CPUS)>>> misses = {};
  champsim::stats::event_counter<std::pair<access_type, std::remove_cv_t<decltype(NUM_CPUS)>>> miss_merge = {};
  champsim::stats::event_counter<std::pair<access_type, std::remove_cv_t<decltype(NUM_CPUS)>>> fill = {};

  long total_miss_latency_cycles{};
};

cache_stats operator-(cache_stats lhs, cache_stats rhs);

#endif
