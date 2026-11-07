#include "hybrid_lru.h"

#include <algorithm>
#include <cassert>

hybrid_lru::hybrid_lru(CACHE* cache_) : replacement(cache_), cache(cache_), NUM_WAY(cache_->NUM_WAY),
                                        last_used_cycles(static_cast<std::size_t>(cache_->NUM_SET * cache_->NUM_WAY), 0)
{
}

long hybrid_lru::find_victim(uint32_t, uint64_t, long set, const champsim::cache_block*, champsim::address, champsim::address, access_type,
                             bool target_sram)
{
  const auto [first_way, last_way] = cache->hybrid_way_range(target_sram);
  assert(first_way < last_way);
  auto begin = std::next(std::begin(last_used_cycles), set * NUM_WAY + first_way);
  auto end = std::next(std::begin(last_used_cycles), set * NUM_WAY + last_way);

  // DAM DVS (Stage 2): prefer a predicted-dead block in the target portion
  // (the least recently used one if there are several); otherwise plain LRU.
  if (cache->dam_dvs_enabled()) {
    long dead_way = -1;
    for (long way = first_way; way < last_way; ++way) {
      if (cache->dam_block_dead(set, way)
          && (dead_way < 0 || last_used_cycles.at(static_cast<std::size_t>(set * NUM_WAY + way)) < last_used_cycles.at(static_cast<std::size_t>(set * NUM_WAY + dead_way))))
        dead_way = way;
    }
    if (dead_way >= 0)
      return dead_way;
  }

  auto victim = std::min_element(begin, end);
  return first_way + std::distance(begin, victim);
}

void hybrid_lru::replacement_cache_fill(uint32_t, long set, long way, champsim::address, champsim::address, champsim::address, access_type)
{
  last_used_cycles.at(static_cast<std::size_t>(set * NUM_WAY + way)) = cycle++;
}

void hybrid_lru::update_replacement_state(uint32_t, long set, long way, champsim::address, champsim::address, champsim::address, access_type type, uint8_t hit)
{
  if (hit && type != access_type::WRITE)
    last_used_cycles.at(static_cast<std::size_t>(set * NUM_WAY + way)) = cycle++;
}
