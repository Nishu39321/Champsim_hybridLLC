#include "hybrid_lru.h"

#include <algorithm>
#include <cassert>

hybrid_lru::hybrid_lru(CACHE* cache_) : replacement(cache_), cache(cache_), NUM_WAY(cache_->NUM_WAY),
                                        last_used_cycles(static_cast<std::size_t>(cache_->NUM_SET * cache_->NUM_WAY), 0)
{
}

long hybrid_lru::lru_dead_way(long set, long first, long last) const
{
  long dead_way = -1;
  for (long way = first; way < last; ++way) {
    if (cache->dam_block_dead(set, way) && (dead_way < 0 || stamp(set, way) < stamp(set, dead_way)))
      dead_way = way;
  }
  return dead_way;
}

long hybrid_lru::find_victim(uint32_t, uint64_t, long set, const champsim::cache_block*, champsim::address, champsim::address, access_type,
                             bool target_sram)
{
  const auto [first_way, last_way] = cache->hybrid_way_range(target_sram);
  assert(first_way < last_way);
  pending_set = -1;

  if (cache->dam_dvs_enabled()) {
    // DAM DVS (Stage 2): prefer the least recently used predicted-dead block in the target portion.
    if (const long dead_way = lru_dead_way(set, first_way, last_way); dead_way >= 0)
      return dead_way;

    // DAM DM (Stage 3): no dead block in the target portion, but one in the other portion.
    // Move a block of the target portion into the dead slot and give its way to the incoming block:
    //  - incoming block -> SRAM:    move the most read-intensive SRAM block (lowest cost; ties: least recently used)
    //  - incoming block -> STT-RAM: move the most write-intensive STT-RAM block (highest cost; ties: most recently used)
    if (cache->dam_dm_enabled()) {
      const auto [other_first, other_last] = cache->hybrid_way_range(!target_sram);
      if (const long dead_other = lru_dead_way(set, other_first, other_last); dead_other >= 0) {
        long migrant = first_way;
        for (long way = first_way + 1; way < last_way; ++way) {
          const int cost = cache->dam_block_cost(set, way);
          const int best = cache->dam_block_cost(set, migrant);
          if (target_sram ? (cost < best || (cost == best && stamp(set, way) < stamp(set, migrant)))
                          : (cost > best || (cost == best && stamp(set, way) > stamp(set, migrant))))
            migrant = way;
        }
        pending_set = set;
        pending_from = migrant;
        pending_to = dead_other;
        cache->dam_set_pending_migration(set, migrant, dead_other);
        return migrant;
      }
    }
  }

  auto begin = std::next(std::begin(last_used_cycles), set * NUM_WAY + first_way);
  auto end = std::next(std::begin(last_used_cycles), set * NUM_WAY + last_way);
  auto victim = std::min_element(begin, end);
  return first_way + std::distance(begin, victim);
}

void hybrid_lru::replacement_cache_fill(uint32_t, long set, long way, champsim::address, champsim::address, champsim::address, access_type)
{
  if (pending_set == set && pending_from == way && cache->dam.pending.valid && cache->dam.pending.set == set && cache->dam.pending.from == way) {
    // DM: the migrated block keeps its recency in its new way.
    last_used_cycles.at(static_cast<std::size_t>(set * NUM_WAY + pending_to)) = stamp(set, pending_from);
  }
  pending_set = -1;
  last_used_cycles.at(static_cast<std::size_t>(set * NUM_WAY + way)) = cycle++;
}

void hybrid_lru::update_replacement_state(uint32_t, long set, long way, champsim::address, champsim::address, champsim::address, access_type type, uint8_t hit)
{
  if (hit && type != access_type::WRITE)
    last_used_cycles.at(static_cast<std::size_t>(set * NUM_WAY + way)) = cycle++;
}
