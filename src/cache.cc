/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cache.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <fmt/core.h>

#include "bandwidth.h"
#include "champsim.h"
#include "chrono.h"
#include "deadlock.h"
#include "instruction.h"
#include "util/algorithm.h"
#include "util/bits.h"
#include "util/span.h"

CACHE::CACHE(CACHE&& other)
    : operable(other),

      delayed_responses(std::move(other.delayed_responses)),

      upper_levels(std::move(other.upper_levels)), lower_level(std::move(other.lower_level)), lower_translate(std::move(other.lower_translate)),

      cpu(other.cpu), NAME(std::move(other.NAME)), NUM_SET(other.NUM_SET), NUM_WAY(other.NUM_WAY), MSHR_SIZE(other.MSHR_SIZE), PQ_SIZE(other.PQ_SIZE),
      HIT_LATENCY(other.HIT_LATENCY), FILL_LATENCY(other.FILL_LATENCY), OFFSET_BITS(other.OFFSET_BITS), block(std::move(other.block)), MAX_TAG(other.MAX_TAG),
      MAX_FILL(other.MAX_FILL), prefetch_as_load(other.prefetch_as_load), match_offset_bits(other.match_offset_bits), virtual_prefetch(other.virtual_prefetch),
      hybrid_llc(other.hybrid_llc), SRAM_WAYS(other.SRAM_WAYS), HYBRID_POLICY(other.HYBRID_POLICY), PHC_PREDICTOR(other.PHC_PREDICTOR),
      PHC_THRESHOLD(other.PHC_THRESHOLD), PHC_READ_DELTA(other.PHC_READ_DELTA), PHC_WRITE_DELTA(other.PHC_WRITE_DELTA),
      PHC_SAMPLER_PERIOD(other.PHC_SAMPLER_PERIOD), SRAM_READ_LATENCY(other.SRAM_READ_LATENCY),
      SRAM_WRITE_LATENCY(other.SRAM_WRITE_LATENCY), STTRAM_READ_LATENCY(other.STTRAM_READ_LATENCY), STTRAM_WRITE_LATENCY(other.STTRAM_WRITE_LATENCY),
      sram_read_ports(std::move(other.sram_read_ports)), sram_write_ports(std::move(other.sram_write_ports)),
      sttram_read_ports(std::move(other.sttram_read_ports)), sttram_write_ports(std::move(other.sttram_write_ports)),
      pref_activate_mask(std::move(other.pref_activate_mask)), phc_predictor(std::move(other.phc_predictor)),
      phc_sampler(std::move(other.phc_sampler)), phc_sampler_clock(other.phc_sampler_clock), dam(std::move(other.dam)),

      sim_stats(std::move(other.sim_stats)), roi_stats(std::move(other.roi_stats)),

      pref_module_pimpl(std::move(other.pref_module_pimpl)), repl_module_pimpl(std::move(other.repl_module_pimpl))
{
  pref_module_pimpl->bind(this);
  repl_module_pimpl->bind(this);
}

auto CACHE::operator=(CACHE&& other) -> CACHE&
{
  this->clock_period = other.clock_period;
  this->current_time = other.current_time;
  this->warmup = other.warmup;

  this->upper_levels = std::move(other.upper_levels);
  this->lower_level = std::move(other.lower_level);
  this->lower_translate = std::move(other.lower_translate);

  this->cpu = other.cpu;
  this->NAME = std::move(other.NAME);
  this->NUM_SET = other.NUM_SET;
  this->NUM_WAY = other.NUM_WAY;
  ;
  this->MSHR_SIZE = other.MSHR_SIZE;
  ;
  this->PQ_SIZE = other.PQ_SIZE;
  this->HIT_LATENCY = other.HIT_LATENCY;
  this->FILL_LATENCY = other.FILL_LATENCY;
  this->OFFSET_BITS = other.OFFSET_BITS;
  ;
  this->block = std::move(other.block);
  this->MAX_TAG = other.MAX_TAG;
  this->MAX_FILL = other.MAX_FILL;
  this->prefetch_as_load = other.prefetch_as_load;
  this->match_offset_bits = other.match_offset_bits;
  this->virtual_prefetch = other.virtual_prefetch;
  this->hybrid_llc = other.hybrid_llc;
  this->SRAM_WAYS = other.SRAM_WAYS;
  this->HYBRID_POLICY = other.HYBRID_POLICY;
  this->PHC_PREDICTOR = other.PHC_PREDICTOR;
  this->PHC_THRESHOLD = other.PHC_THRESHOLD;
  this->PHC_READ_DELTA = other.PHC_READ_DELTA;
  this->PHC_WRITE_DELTA = other.PHC_WRITE_DELTA;
  this->PHC_SAMPLER_PERIOD = other.PHC_SAMPLER_PERIOD;
  this->SRAM_READ_LATENCY = other.SRAM_READ_LATENCY;
  this->SRAM_WRITE_LATENCY = other.SRAM_WRITE_LATENCY;
  this->STTRAM_READ_LATENCY = other.STTRAM_READ_LATENCY;
  this->STTRAM_WRITE_LATENCY = other.STTRAM_WRITE_LATENCY;
  this->sram_read_ports = std::move(other.sram_read_ports);
  this->sram_write_ports = std::move(other.sram_write_ports);
  this->sttram_read_ports = std::move(other.sttram_read_ports);
  this->sttram_write_ports = std::move(other.sttram_write_ports);
  this->pref_activate_mask = std::move(other.pref_activate_mask);
  this->phc_predictor = std::move(other.phc_predictor);
  this->phc_sampler = std::move(other.phc_sampler);
  this->phc_sampler_clock = other.phc_sampler_clock;
  this->dam = std::move(other.dam);
  this->delayed_responses = std::move(other.delayed_responses);

  this->sim_stats = std::move(other.sim_stats);
  this->roi_stats = std::move(other.roi_stats);

  this->pref_module_pimpl = std::move(other.pref_module_pimpl);
  this->repl_module_pimpl = std::move(other.repl_module_pimpl);

  pref_module_pimpl->bind(this);
  repl_module_pimpl->bind(this);

  return *this;
}

CACHE::tag_lookup_type::tag_lookup_type(const request_type& req, bool local_pref, bool skip)
    : address(req.address), v_address(req.v_address), data(req.data), ip(req.ip), instr_id(req.instr_id), pf_metadata(req.pf_metadata), cpu(req.cpu),
      type(req.type), prefetch_from_this(local_pref), skip_fill(skip), is_translated(req.is_translated), instr_depend_on_me(req.instr_depend_on_me)
{
}

CACHE::fill_type::fill_type(const tag_lookup_type& req, champsim::chrono::clock::time_point _time_enqueued)
    : address(req.address), v_address(req.v_address), ip(req.ip), instr_id(req.instr_id), cpu(req.cpu), type(req.type),
      prefetch_from_this(req.prefetch_from_this), dirty(req.type == access_type::WRITE), time_enqueued(_time_enqueued),
      instr_depend_on_me(req.instr_depend_on_me), to_return(req.to_return)
{
}

CACHE::fill_type CACHE::fill_type::merge(fill_type predecessor, fill_type successor)
{
  std::vector<uint64_t> merged_instr{};
  std::vector<std::deque<response_type>*> merged_return{};

  std::set_union(std::begin(predecessor.instr_depend_on_me), std::end(predecessor.instr_depend_on_me), std::begin(successor.instr_depend_on_me),
                 std::end(successor.instr_depend_on_me), std::back_inserter(merged_instr));
  std::set_union(std::begin(predecessor.to_return), std::end(predecessor.to_return), std::begin(successor.to_return), std::end(successor.to_return),
                 std::back_inserter(merged_return));

  fill_type retval{(successor.type == access_type::PREFETCH) ? predecessor : successor};
  // A merged request must retain the placement chosen when the original miss
  // allocated the MSHR. A later demand request must not move an in-flight fill.
  retval.target_sram = predecessor.target_sram;
  // Fix 4: a load merging into an in-flight writeback must not drop its dirty data.
  retval.dirty = predecessor.dirty || successor.dirty;

  // set the time enqueued to the predecessor unless its a demand into prefetch, in which case we use the successor
  retval.time_enqueued =
      ((successor.type != access_type::PREFETCH && predecessor.type == access_type::PREFETCH)) ? successor.time_enqueued : predecessor.time_enqueued;
  retval.instr_depend_on_me = merged_instr;
  retval.to_return = merged_return;
  retval.data_promise = predecessor.data_promise;

  if constexpr (champsim::debug_print) {
    if (successor.type == access_type::PREFETCH) {
      fmt::print("[MSHR] {} address {} type: {} into address {} type: {}\n", __func__, successor.address,
                 access_type_names.at(champsim::to_underlying(successor.type)), predecessor.address,
                 access_type_names.at(champsim::to_underlying(successor.type)));
    } else {
      fmt::print("[MSHR] {} address {} type: {} into address {} type: {}\n", __func__, predecessor.address,
                 access_type_names.at(champsim::to_underlying(predecessor.type)), successor.address,
                 access_type_names.at(champsim::to_underlying(successor.type)));
    }
  }

  return retval;
}

auto CACHE::fill_block(fill_type fill, uint32_t metadata) -> BLOCK
{
  CACHE::BLOCK to_fill;
  to_fill.valid = true;
  to_fill.prefetch = fill.prefetch_from_this;
  to_fill.dirty = fill.dirty || (fill.type == access_type::WRITE);
  to_fill.address = fill.address;
  to_fill.v_address = fill.v_address;
  to_fill.data = fill.data_promise->data;
  to_fill.pf_metadata = metadata;

  return to_fill;
}

auto CACHE::matches_address(champsim::address addr) const
{
  return [match = addr.slice_upper(OFFSET_BITS), shamt = OFFSET_BITS](const auto& entry) {
    return entry.address.slice_upper(shamt) == match;
  };
}

template <typename T>
champsim::address CACHE::module_address(const T& element) const
{
  auto address = virtual_prefetch ? element.v_address : element.address;
  return champsim::address{address.slice_upper(match_offset_bits ? champsim::data::bits{} : OFFSET_BITS)};
}

bool CACHE::handle_fill(const fill_type& fill)
{
  cpu = fill.cpu;

  // find victim
  auto [set_begin, set_end] = get_set_span(fill.address);
  auto [medium_begin_idx, medium_end_idx] = hybrid_way_range(fill.target_sram);
  auto medium_begin = std::next(set_begin, medium_begin_idx);
  auto medium_end = std::next(set_begin, medium_end_idx);
  auto way = std::find_if_not(medium_begin, medium_end, [](auto x) { return x.valid; });
  if (way == medium_end) {
    way = std::next(set_begin,
                    impl_find_victim(fill.cpu, fill.instr_id, get_set_index(fill.address), &*set_begin, fill.ip, fill.address, fill.type, fill.target_sram));
  }
  assert(medium_begin <= way);
  assert(way <= medium_end);
  assert(way != medium_end || fill.type != access_type::WRITE); // Writes may not bypass
  const auto way_idx = std::distance(set_begin, way);        // cast protected by earlier assertion

  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} instr_id: {} address: {} v_address: {} set: {} way: {} type: {} prefetch_metadata: {} cycle_enqueued: {} cycle: {}\n", NAME, __func__,
               fill.instr_id, fill.address, fill.v_address, get_set_index(fill.address), way_idx, access_type_names.at(champsim::to_underlying(fill.type)),
               fill.data_promise->pf_metadata, (fill.time_enqueued.time_since_epoch()) / clock_period, (current_time.time_since_epoch()) / clock_period);
  }

  if (dam.enabled && way != set_end && way->valid)
    dam_note_eviction(get_set_index(fill.address), way_idx);

  if (way != set_end && way->valid && way->dirty) {
    request_type writeback_packet;

    writeback_packet.cpu = fill.cpu;
    writeback_packet.address = way->address;
    writeback_packet.data = way->data;
    writeback_packet.instr_id = fill.instr_id;
    writeback_packet.ip = champsim::address{};
    writeback_packet.type = access_type::WRITE;
    writeback_packet.pf_metadata = way->pf_metadata;
    writeback_packet.response_requested = false;

    if constexpr (champsim::debug_print) {
      fmt::print("[{}] {} evict address: {} v_address: {} prefetch_metadata: {}\n", NAME, __func__, writeback_packet.address, writeback_packet.v_address,
                 fill.data_promise->pf_metadata);
    }

    auto success = lower_level->add_wq(writeback_packet);
    if (!success) {
      return false;
    }
  }

  champsim::address evicting_address{};
  if (way != set_end && way->valid) {
    evicting_address = module_address(*way);
  }

  auto metadata_thru = impl_prefetcher_cache_fill(module_address(fill), get_set_index(fill.address), way_idx, (fill.type == access_type::PREFETCH),
                                                  evicting_address, fill.data_promise->pf_metadata);
  impl_replacement_cache_fill(fill.cpu, get_set_index(fill.address), way_idx, module_address(fill), fill.ip, evicting_address, fill.type);

  if (way != set_end) {
    if (way->valid && way->prefetch) {
      ++sim_stats.pf_useless;
    }

    if (fill.type == access_type::PREFETCH) {
      ++sim_stats.pf_fill;
    }

    *way = fill_block(fill, metadata_thru);
    if (dam.enabled) {
      // DAM: remember the (hashed) PC of the instruction that brought the block in.
      const auto idx = static_cast<std::size_t>(get_set_index(fill.address) * NUM_WAY + way_idx);
      const bool has_pc = (fill.ip != champsim::address{}) && (fill.type != access_type::WRITE);
      dam.block_pc.at(idx) = has_pc ? dam_hash_pc(fill.ip) : uint8_t{0};
      dam.block_has_pc.at(idx) = has_pc ? 1 : 0;
    }
    if (hybrid_llc)
      count_medium_access(is_sram_way(way_idx), true);
  }

  // COLLECT STATS
  if (fill.type != access_type::PREFETCH)
    sim_stats.total_miss_latency_cycles += (current_time - (fill.time_enqueued + clock_period)) / clock_period;
  sim_stats.fill.increment(std::pair{fill.type, fill.cpu});

  response_type response{fill.address, fill.v_address, fill.data_promise->data, metadata_thru, fill.instr_depend_on_me};
  for (auto* ret : fill.to_return) {
    ret->push_back(response);
  }

  return true;
}

bool CACHE::try_hit(const tag_lookup_type& handle_pkt)
{
  cpu = handle_pkt.cpu;

  // access cache
  auto [set_begin, set_end] = get_set_span(handle_pkt.address);
  auto way = std::find_if(set_begin, set_end, [matcher = matches_address(handle_pkt.address)](const auto& x) { return x.valid && matcher(x); });
  const auto hit = (way != set_end);
  const auto useful_prefetch = (hit && way->prefetch && !handle_pkt.prefetch_from_this);

  // Fix 2: a miss that cannot be accepted this cycle re-enters try_hit() on
  // every retry. Train the sampler only when the access actually completes:
  // here for hits, and in handle_miss()/handle_write() for accepted misses.
  if (hit)
    phc_sampler_access(handle_pkt);

  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} instr_id: {} address: {} v_address: {} data: {} set: {} way: {} ({}) type: {} cycle: {}\n", NAME, __func__, handle_pkt.instr_id,
               handle_pkt.address, handle_pkt.v_address, handle_pkt.data, get_set_index(handle_pkt.address), std::distance(set_begin, way),
               hit ? "HIT" : "MISS", access_type_names.at(champsim::to_underlying(handle_pkt.type)), current_time.time_since_epoch() / clock_period);
  }

  auto metadata_thru = handle_pkt.pf_metadata;
  if (should_activate_prefetcher(handle_pkt)) {
    metadata_thru = impl_prefetcher_cache_operate(module_address(handle_pkt), handle_pkt.ip, hit, useful_prefetch, handle_pkt.type, metadata_thru);
  }

  // update replacement policy
  const auto way_idx = std::distance(set_begin, way);
  impl_update_replacement_state(handle_pkt.cpu, get_set_index(handle_pkt.address), way_idx, module_address(handle_pkt), handle_pkt.ip, {}, handle_pkt.type,
                                hit);

  if (hit) {
    sim_stats.hits.increment(std::pair{handle_pkt.type, handle_pkt.cpu});

    response_type response{handle_pkt.address, handle_pkt.v_address, way->data, metadata_thru, handle_pkt.instr_depend_on_me};
    if (hybrid_llc && !warmup) {
      const auto sram = is_sram_way(way_idx);
      const auto write = handle_pkt.type == access_type::WRITE;
      const auto latency = medium_latency(sram, write);
      const auto start = reserve_medium_port(sram, write, latency);
      delayed_responses.push_back({start + latency, std::move(response), handle_pkt.to_return});
      count_medium_access(sram, write);
    } else {
      for (auto* ret : handle_pkt.to_return) {
        ret->push_back(response);
      }
    }

    way->dirty |= (handle_pkt.type == access_type::WRITE);

    // update prefetch stats and reset prefetch bit
    if (useful_prefetch) {
      ++sim_stats.pf_useful;
      way->prefetch = false;
    }
  }

  return hit;
}

auto CACHE::mshr_and_forward_packet(const tag_lookup_type& handle_pkt) -> std::pair<fill_type, request_type>
{
  fill_type to_allocate{handle_pkt, current_time};

  request_type fwd_pkt;

  fwd_pkt.asid[0] = handle_pkt.asid[0];
  fwd_pkt.asid[1] = handle_pkt.asid[1];
  fwd_pkt.type = (handle_pkt.type == access_type::WRITE) ? access_type::RFO : handle_pkt.type;
  fwd_pkt.pf_metadata = handle_pkt.pf_metadata;
  fwd_pkt.cpu = handle_pkt.cpu;

  fwd_pkt.address = handle_pkt.address;
  fwd_pkt.v_address = handle_pkt.v_address;
  fwd_pkt.data = handle_pkt.data;
  fwd_pkt.instr_id = handle_pkt.instr_id;
  fwd_pkt.ip = handle_pkt.ip;

  fwd_pkt.instr_depend_on_me = handle_pkt.instr_depend_on_me;
  fwd_pkt.response_requested = (!handle_pkt.prefetch_from_this || !handle_pkt.skip_fill);

  return std::pair{std::move(to_allocate), std::move(fwd_pkt)};
}

bool CACHE::handle_miss(const tag_lookup_type& handle_pkt)
{
  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} instr_id: {} address: {} v_address: {} type: {} local_prefetch: {} cycle: {}\n", NAME, __func__, handle_pkt.instr_id,
               handle_pkt.address, handle_pkt.v_address, access_type_names.at(champsim::to_underlying(handle_pkt.type)), handle_pkt.prefetch_from_this,
               current_time.time_since_epoch() / clock_period);
  }

  fill_type to_allocate{handle_pkt, current_time};

  cpu = handle_pkt.cpu;

  auto mshr_pkt = mshr_and_forward_packet(handle_pkt);

  // check mshr
  auto fill_entry = std::find_if(std::begin(MSHR), std::end(MSHR), matches_address(handle_pkt.address));
  bool mshr_full = (MSHR.size() == MSHR_SIZE);

  // check inflight fills
  if (fill_entry == MSHR.end()) {
    fill_entry = std::find_if(inflight_fills.begin(), inflight_fills.end(), matches_address(handle_pkt.address));
  }

  if (fill_entry != inflight_fills.end()) // miss or fill already inflight
  {
    if (fill_entry->type == access_type::PREFETCH && handle_pkt.type != access_type::PREFETCH) {
      // Mark the prefetch as useful
      if (fill_entry->prefetch_from_this) {
        ++sim_stats.pf_useful;
      }
    }

    // COLLECT STATS
    sim_stats.miss_merge.increment(std::pair{to_allocate.type, to_allocate.cpu});

    *fill_entry = fill_type::merge(*fill_entry, to_allocate);
  } else {
    if (mshr_full) { // not enough MSHR resource
      return false;  // TODO should we allow prefetches anyway if they will not be filled to this level?
    }

    const bool send_to_rq = (prefetch_as_load || handle_pkt.type != access_type::PREFETCH);
    bool success = send_to_rq ? lower_level->add_rq(mshr_pkt.second) : lower_level->add_pq(mshr_pkt.second);

    if (!success) {
      return false;
    }

    // Allocate an MSHR. Fix 2: predict placement exactly once, only for a
    // newly allocated miss (not for retries or for requests that merge).
    if (mshr_pkt.second.response_requested) {
      mshr_pkt.first.target_sram = choose_fill_target(handle_pkt);
      MSHR.emplace_back(std::move(mshr_pkt.first));
    }
  }

  phc_sampler_access(handle_pkt);
  if (dam.enabled && handle_pkt.type != access_type::PREFETCH)
    dam_record_demand_miss(get_set_index(handle_pkt.address));
  sim_stats.misses.increment(std::pair{handle_pkt.type, handle_pkt.cpu});

  return true;
}

bool CACHE::handle_write(const tag_lookup_type& handle_pkt)
{
  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} instr_id: {} address: {} v_address: {} type: {} local_prefetch: {} cycle: {}\n", NAME, __func__, handle_pkt.instr_id,
               handle_pkt.address, handle_pkt.v_address, access_type_names.at(champsim::to_underlying(handle_pkt.type)), handle_pkt.prefetch_from_this,
               current_time.time_since_epoch() / clock_period);
  }

  fill_type to_allocate{handle_pkt, current_time};
  to_allocate.target_sram = choose_fill_target(handle_pkt);
  if (hybrid_llc && !warmup) {
    // A writeback miss allocates a line directly in this cache, so it does not
    // pass through finish_packet(). Fix 1: the array write occupies a port of
    // the destination medium for its full latency, but the line is
    // installed after the normal fill latency (nothing waits on its data).
    const auto sram = to_allocate.target_sram;
    reserve_medium_port(sram, true, medium_latency(sram, true));
  }
  to_allocate.data_promise.ready_at(current_time + (warmup ? champsim::chrono::clock::duration{} : FILL_LATENCY));
  inflight_fills.push_back(to_allocate);

  phc_sampler_access(handle_pkt);
  sim_stats.misses.increment(std::pair{handle_pkt.type, handle_pkt.cpu});

  return true;
}

template <bool UpdateRequest>
auto CACHE::initiate_tag_check(champsim::channel* ul)
{
  return [time = current_time + (warmup ? champsim::chrono::clock::duration{} : HIT_LATENCY), ul](const auto& entry) {
    CACHE::tag_lookup_type retval{entry};
    retval.event_cycle = time;

    if constexpr (UpdateRequest) {
      if (entry.response_requested) {
        retval.to_return = {&ul->returned};
      }
    } else {
      (void)ul; // supress warning about ul being unused
    }

    if constexpr (champsim::debug_print) {
      fmt::print("[TAG] initiate_tag_check instr_id: {} address: {} v_address: {} type: {} response_requested: {}\n", retval.instr_id, retval.address,
                 retval.v_address, access_type_names.at(champsim::to_underlying(retval.type)), !std::empty(retval.to_return));
    }

    return retval;
  };
}

long CACHE::operate()
{
  long progress{0};

  auto response_end = std::partition(std::begin(delayed_responses), std::end(delayed_responses), [time = current_time](const auto& response) {
    return response.ready_at <= time;
  });
  for (auto response_it = std::begin(delayed_responses); response_it != response_end; ++response_it) {
    for (auto* destination : response_it->destinations)
      destination->push_back(response_it->response);
  }
  progress += std::distance(std::begin(delayed_responses), response_end);
  delayed_responses.erase(std::begin(delayed_responses), response_end);

  auto is_ready = [time = current_time](const auto& entry) {
    return entry.event_cycle <= time;
  };
  auto is_translated = [](const auto& entry) {
    return entry.is_translated;
  };

  // Finish returns
  std::for_each(std::cbegin(lower_level->returned), std::cend(lower_level->returned), [this](const auto& pkt) { this->finish_packet(pkt); });
  progress += std::distance(std::cbegin(lower_level->returned), std::cend(lower_level->returned));
  lower_level->returned.clear();

  // Finish translations
  if (lower_translate != nullptr) {
    std::for_each(std::cbegin(lower_translate->returned), std::cend(lower_translate->returned), [this](const auto& pkt) { this->finish_translation(pkt); });
    progress += std::distance(std::cbegin(lower_translate->returned), std::cend(lower_translate->returned));
    lower_translate->returned.clear();
  }

  // Perform fills
  champsim::bandwidth fill_bw{MAX_FILL};
  auto [fill_begin, fill_end] = champsim::get_span_p(std::cbegin(inflight_fills), std::cend(inflight_fills), fill_bw,
                                                     [time = current_time](const auto& x) { return x.data_promise.is_ready_at(time); });
  auto complete_end = std::find_if_not(fill_begin, fill_end, [this](const auto& x) { return this->handle_fill(x); });
  fill_bw.consume(std::distance(fill_begin, complete_end));
  inflight_fills.erase(fill_begin, complete_end);

  // Initiate tag checks
  const champsim::bandwidth::maximum_type bandwidth_from_tag_checks{champsim::to_underlying(MAX_TAG) * (long)(HIT_LATENCY / clock_period)
                                                                    - (long)std::size(inflight_tag_check)};
  champsim::bandwidth initiate_tag_bw{std::clamp(bandwidth_from_tag_checks, champsim::bandwidth::maximum_type{0}, MAX_TAG)};
  auto can_translate = [avail = (std::size(translation_stash) < static_cast<std::size_t>(MSHR_SIZE))](const auto& entry) {
    return avail || entry.is_translated;
  };
  auto stash_bandwidth_consumed =
      champsim::transform_while_n(translation_stash, std::back_inserter(inflight_tag_check), initiate_tag_bw, is_translated, initiate_tag_check<false>());
  initiate_tag_bw.consume(stash_bandwidth_consumed);
  std::vector<long long> channels_bandwidth_consumed{};

  if (std::size(upper_levels) > 1) {
    std::rotate(upper_levels.begin(), upper_levels.begin() + 1, upper_levels.end());
  }

  // upper levels get an equal portion of the remaining bandwidth
  champsim::bandwidth::maximum_type per_upper_bandwidth =
      std::size(upper_levels) >= 1
          ? (champsim::bandwidth::maximum_type)std::max((size_t)initiate_tag_bw.amount_remaining() / std::size(upper_levels), size_t{1})
          : champsim::bandwidth::maximum_type{};

  for (auto* ul : upper_levels) {
    for (auto q : {std::ref(ul->WQ), std::ref(ul->RQ), std::ref(ul->PQ)}) {
      // this needs to be in this loop, we need to ensure that for cases where bandwidth doesn't divide nicely across upstreams,
      // we don't accidentally consume more bandwidth than expected
      champsim::bandwidth per_upper_tag_bw{std::min(per_upper_bandwidth, champsim::bandwidth::maximum_type{initiate_tag_bw.amount_remaining()})};
      auto bandwidth_consumed =
          champsim::transform_while_n(q.get(), std::back_inserter(inflight_tag_check), per_upper_tag_bw, can_translate, initiate_tag_check<true>(ul));
      channels_bandwidth_consumed.push_back(bandwidth_consumed);
      initiate_tag_bw.consume(bandwidth_consumed);
    }
  }

  auto pq_bandwidth_consumed =
      champsim::transform_while_n(internal_PQ, std::back_inserter(inflight_tag_check), initiate_tag_bw, can_translate, initiate_tag_check<false>());
  initiate_tag_bw.consume(pq_bandwidth_consumed);

  // Issue translations
  std::for_each(std::begin(inflight_tag_check), std::end(inflight_tag_check), [this](auto& x) { this->issue_translation(x); });
  std::for_each(std::begin(translation_stash), std::end(translation_stash), [this](auto& x) { this->issue_translation(x); });

  // Find entries that would be ready except that they have not finished translation, move them to the stash
  auto [last_not_missed, stash_end] = champsim::extract_if(std::begin(inflight_tag_check), std::end(inflight_tag_check), std::back_inserter(translation_stash),
                                                           [is_ready, is_translated](const auto& x) { return is_ready(x) && !is_translated(x); });
  progress += std::distance(last_not_missed, std::end(inflight_tag_check));
  inflight_tag_check.erase(last_not_missed, std::end(inflight_tag_check));

  // Perform tag checks
  auto do_handle_miss = [this](const auto& pkt) {
    if (pkt.type == access_type::WRITE && !this->match_offset_bits) {
      return this->handle_write(pkt); // Treat writes (that is, writebacks) like fills
    }
    return this->handle_miss(pkt); // Treat writes (that is, stores) like reads
  };
  champsim::bandwidth tag_check_bw{MAX_TAG};
  auto [tag_check_ready_begin, tag_check_ready_end] =
      champsim::get_span_p(std::begin(inflight_tag_check), std::end(inflight_tag_check), tag_check_bw,
                           [is_ready, is_translated](const auto& pkt) { return is_ready(pkt) && is_translated(pkt); });
  auto hits_end = std::stable_partition(tag_check_ready_begin, tag_check_ready_end, [this](const auto& pkt) { return this->try_hit(pkt); });
  auto finish_tag_check_end = std::stable_partition(hits_end, tag_check_ready_end, do_handle_miss);
  tag_check_bw.consume(std::distance(tag_check_ready_begin, finish_tag_check_end));
  inflight_tag_check.erase(tag_check_ready_begin, finish_tag_check_end);

  impl_prefetcher_cycle_operate();

  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} cycle completed: {} tags checked: {} remaining: {} stash consumed: {} remaining: {} channel consumed: {} pq consumed {} unused consume "
               "bw {}\n",
               NAME, __func__, current_time.time_since_epoch() / clock_period, tag_check_bw.amount_consumed(), std::size(inflight_tag_check),
               stash_bandwidth_consumed, std::size(translation_stash), channels_bandwidth_consumed, pq_bandwidth_consumed, initiate_tag_bw.amount_remaining());
  }

  return progress + fill_bw.amount_consumed() + initiate_tag_bw.amount_consumed() + tag_check_bw.amount_consumed();
}

// LCOV_EXCL_START exclude deprecated function
uint64_t CACHE::get_set(uint64_t address) const { return static_cast<uint64_t>(get_set_index(champsim::address{address})); }
// LCOV_EXCL_STOP

long CACHE::get_set_index(champsim::address address) const { return address.slice(champsim::dynamic_extent{OFFSET_BITS, champsim::lg2(NUM_SET)}).to<long>(); }

template <typename It>
std::pair<It, It> get_span(It anchor, typename std::iterator_traits<It>::difference_type set_idx, typename std::iterator_traits<It>::difference_type num_way)
{
  auto begin = std::next(anchor, set_idx * num_way);
  return {std::move(begin), std::next(begin, num_way)};
}

bool CACHE::is_sram_way(long way) const { return way < static_cast<long>(SRAM_WAYS); }

bool CACHE::target_is_sram(access_type type) const
{
  // Keep technology-only baselines valid regardless of the selected policy.
  if (SRAM_WAYS == 0)
    return false;
  if (SRAM_WAYS == NUM_WAY)
    return true;

  switch (HYBRID_POLICY) {
  case champsim::hybrid_llc_policy::read_stt_write_sram:
    return type == access_type::RFO || type == access_type::WRITE;
  case champsim::hybrid_llc_policy::read_sram_writeback_stt:
    return type == access_type::LOAD || type == access_type::RFO;
  case champsim::hybrid_llc_policy::demand_sram_prefetch_stt:
    return type != access_type::PREFETCH;
  }
  throw std::logic_error{"Unknown hybrid LLC placement policy"};
}

std::pair<long, long> CACHE::hybrid_way_range(access_type type) const
{
  return hybrid_way_range(target_is_sram(type));
}

std::pair<long, long> CACHE::hybrid_way_range(bool target_sram) const
{
  if (!hybrid_llc)
    return {0, static_cast<long>(NUM_WAY)};

  if (target_sram)
    return {0, static_cast<long>(SRAM_WAYS)};
  return {static_cast<long>(SRAM_WAYS), static_cast<long>(NUM_WAY)};
}

bool CACHE::choose_fill_target(const tag_lookup_type& request)
{
  if (!hybrid_llc)
    return false;
  if (SRAM_WAYS == 0)
    return false;
  if (SRAM_WAYS == NUM_WAY)
    return true;

  if (!PHC_PREDICTOR)
    return target_is_sram(request.type);

  // Writebacks have no useful trigger PC in ChampSim. Keep them in SRAM, and
  // keep prefetches in STT-RAM, as defined for this first PHC baseline.
  if (request.type == access_type::WRITE)
    return true;
  if (request.type == access_type::PREFETCH)
    return false;

  const auto predictor_index = static_cast<std::size_t>(request.ip.to<uint64_t>() & 0xfffULL);
  const bool sram = phc_predictor.at(predictor_index) >= 2;
  if (sram)
    ++sim_stats.phc_sram_placements;
  else
    ++sim_stats.phc_sttram_placements;
  return sram;
}

void CACHE::phc_train(uint16_t trigger_index, int16_t cost)
{
  auto& counter = phc_predictor.at(trigger_index);
  if (cost >= PHC_THRESHOLD) {
    counter = std::min<uint8_t>(3, static_cast<uint8_t>(counter + 1));
    ++sim_stats.phc_hot_training_events;
  } else {
    counter = (counter == 0) ? 0 : static_cast<uint8_t>(counter - 1);
    ++sim_stats.phc_cold_training_events;
  }
}

void CACHE::phc_sampler_access(const tag_lookup_type& request)
{
  if (!PHC_PREDICTOR)
    return;

  const auto set = static_cast<uint32_t>(get_set_index(request.address));
  if (set % PHC_SAMPLER_PERIOD != 0)
    return;

  const auto tag_shift = champsim::to_underlying(OFFSET_BITS) + champsim::lg2(NUM_SET);
  const auto partial_tag = static_cast<uint16_t>((request.address.to<uint64_t>() >> tag_shift) & 0xffffULL);
  const auto sampler_set = set / PHC_SAMPLER_PERIOD;
  const auto begin_idx = static_cast<std::size_t>(sampler_set) * NUM_WAY;
  auto begin = std::next(std::begin(phc_sampler), static_cast<long>(begin_idx));
  auto end = std::next(begin, NUM_WAY);
  auto entry = std::find_if(begin, end, [partial_tag](const auto& x) { return x.valid && x.partial_tag == partial_tag; });

  const auto access_delta = (request.type == access_type::WRITE) ? PHC_WRITE_DELTA : PHC_READ_DELTA;
  if (entry == end) {
    // A writeback that misses the sampler has no trigger PC to train or store.
    if (request.type == access_type::WRITE)
      return;

    entry = std::find_if(begin, end, [](const auto& x) { return !x.valid; });
    if (entry == end) {
      entry = std::min_element(begin, end, [](const auto& lhs, const auto& rhs) { return lhs.last_used < rhs.last_used; });
      phc_train(entry->trigger_index, entry->cost);
      if (dam.enabled) {
        // DAM: a block leaving the (LRU) sampler lowers its PC's reuse state.
        auto& state = dam.rst.at(entry->dam_pc);
        state = (state == 0) ? uint8_t{0} : static_cast<uint8_t>(state - 1);
        ++sim_stats.dam_dead_training;
      }
    }

    // Fix 3: PHC Fig. 3 -- the access that brings a block in starts its cost
    // at zero; only later reads/writes to the block change the cost.
    entry->valid = true;
    entry->partial_tag = partial_tag;
    entry->trigger_index = static_cast<uint16_t>(request.ip.to<uint64_t>() & 0xfffULL);
    entry->cost = 0;
    entry->dam_pc = dam_hash_pc(request.ip);
    entry->last_used = phc_sampler_clock++;
    return;
  }

  if (dam.enabled && request.type != access_type::WRITE) {
    // DAM: a demand hit to a sampled block raises its PC's reuse state.
    auto& state = dam.rst.at(entry->dam_pc);
    state = (state == 255) ? uint8_t{255} : static_cast<uint8_t>(state + 1);
    ++sim_stats.dam_reuse_training;
  }

  const auto updated_cost = static_cast<int32_t>(entry->cost) + access_delta;
  entry->cost = static_cast<int16_t>(std::clamp(updated_cost, -128, 127));
  entry->last_used = phc_sampler_clock++;
}

uint8_t CACHE::dam_hash_pc(champsim::address ip)
{
  // 8-bit XOR fold of the full PC.
  auto pc = ip.to<uint64_t>();
  uint64_t hash = 0;
  for (int i = 0; i < 8; ++i) {
    hash ^= pc & 0xffULL;
    pc >>= 8;
  }
  return static_cast<uint8_t>(hash);
}

int CACHE::dam_leader(long set) const
{
  // 32 leader sets per group: one A leader and one B leader in every block of NUM_SET/32 sets.
  // Offsets 1 and stride/2+1 avoid the PHC sampler sets (set % 32 == 0).
  const long stride = static_cast<long>(NUM_SET) / 32;
  const long offset = set % stride;
  if (offset == 1)
    return 1;
  if (offset == stride / 2 + 1)
    return 2;
  return 0;
}

uint32_t CACHE::dam_threshold_for_set(long set) const
{
  switch (dam_leader(set)) {
  case 1:
    return dam.threshold_a;
  case 2:
    return dam.threshold_b;
  default:
    return dam.followers_use_b ? dam.threshold_b : dam.threshold_a;
  }
}

bool CACHE::dam_block_dead(long set, long way) const
{
  if (!dam.enabled)
    return false;
  const auto idx = static_cast<std::size_t>(set * NUM_WAY + way);
  if (!block.at(idx).valid || dam.block_has_pc.at(idx) == 0)
    return false;
  return dam.rst.at(dam.block_pc.at(idx)) < dam_threshold_for_set(set);
}

void CACHE::dam_record_demand_miss(long set)
{
  switch (dam_leader(set)) {
  case 1:
    ++dam.leader_misses_a;
    break;
  case 2:
    ++dam.leader_misses_b;
    break;
  default:
    break;
  }
  if (++dam.phase_misses >= dam.phase_length) {
    // The leader group with fewer misses sets the followers' threshold for the next phase (ties keep the current choice).
    if (dam.leader_misses_a != dam.leader_misses_b)
      dam.followers_use_b = dam.leader_misses_b < dam.leader_misses_a;
    ++sim_stats.dam_phases;
    if (dam.followers_use_b)
      ++sim_stats.dam_phases_b;
    dam.leader_misses_a = dam.leader_misses_b = dam.phase_misses = 0;
  }
}

void CACHE::dam_note_eviction(long set, long way)
{
  if (dam_block_dead(set, way)) {
    ++sim_stats.dam_dead_victims;
    return;
  }
  ++sim_stats.dam_lru_victims;
  // Stage 3 opportunity: was a dead block available in the other technology portion?
  const auto [first, last] = hybrid_way_range(!is_sram_way(way));
  for (long w = first; w < last; ++w) {
    if (dam_block_dead(set, w)) {
      ++sim_stats.dam_dead_in_other_portion;
      break;
    }
  }
}

champsim::chrono::clock::duration CACHE::medium_latency(bool sram, bool write) const
{
  if (sram)
    return write ? SRAM_WRITE_LATENCY : SRAM_READ_LATENCY;
  return write ? STTRAM_WRITE_LATENCY : STTRAM_READ_LATENCY;
}

champsim::chrono::clock::time_point CACHE::reserve_medium_port(bool sram, bool write, champsim::chrono::clock::duration latency)
{
  // Reads and writes of one medium share the same array ports (banks): a bank
  // busy with a 120-cycle STT-RAM write cannot serve a read. The *_read_ports
  // value sets the number of shared ports; *_write_ports is no longer used.
  (void)write;
  auto& ports = sram ? sram_read_ports : sttram_read_ports;
  auto next_port = std::min_element(std::begin(ports), std::end(ports));
  const auto start = std::max(current_time, *next_port);
  *next_port = start + latency;
  return start;
}

void CACHE::count_medium_access(bool sram, bool write)
{
  auto& stats = sim_stats;
  if (sram) {
    if (write)
      ++stats.sram_writes;
    else
      ++stats.sram_reads;
  } else {
    if (write)
      ++stats.sttram_writes;
    else
      ++stats.sttram_reads;
  }
}

auto CACHE::get_set_span(champsim::address address) -> std::pair<set_type::iterator, set_type::iterator>
{
  const auto set_idx = get_set_index(address);
  assert(set_idx < NUM_SET);
  return get_span(std::begin(block), static_cast<set_type::difference_type>(set_idx), NUM_WAY); // safe cast because of prior assert
}

auto CACHE::get_set_span(champsim::address address) const -> std::pair<set_type::const_iterator, set_type::const_iterator>
{
  const auto set_idx = get_set_index(address);
  assert(set_idx < NUM_SET);
  return get_span(std::cbegin(block), static_cast<set_type::difference_type>(set_idx), NUM_WAY); // safe cast because of prior assert
}

// LCOV_EXCL_START exclude deprecated function
uint64_t CACHE::get_way(uint64_t address, uint64_t /*unused set index*/) const
{
  champsim::address intern_addr{address};
  auto [begin, end] = get_set_span(intern_addr);
  return static_cast<uint64_t>(std::distance(begin, std::find_if(begin, end, matches_address(champsim::address{address}))));
}
// LCOV_EXCL_STOP

long CACHE::invalidate_entry(champsim::address inval_addr)
{
  auto [begin, end] = get_set_span(inval_addr);
  auto inv_way = std::find_if(begin, end, matches_address(inval_addr));

  if (inv_way != end) {
    inv_way->valid = false;
  }

  return std::distance(begin, inv_way);
}

bool CACHE::prefetch_line(champsim::address pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  ++sim_stats.pf_requested;

  if (std::size(internal_PQ) >= PQ_SIZE) {
    return false;
  }

  request_type pf_packet;
  pf_packet.type = access_type::PREFETCH;
  pf_packet.pf_metadata = prefetch_metadata;
  pf_packet.cpu = cpu;
  pf_packet.address = pf_addr;
  pf_packet.v_address = virtual_prefetch ? pf_addr : champsim::address{};
  pf_packet.is_translated = !virtual_prefetch;

  internal_PQ.emplace_back(pf_packet, true, !fill_this_level);
  ++sim_stats.pf_issued;

  return true;
}

// LCOV_EXCL_START exclude deprecated function
bool CACHE::prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  return prefetch_line(champsim::address{pf_addr}, fill_this_level, prefetch_metadata);
}

bool CACHE::prefetch_line(uint64_t /*deprecated*/, uint64_t /*deprecated*/, uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  return prefetch_line(champsim::address{pf_addr}, fill_this_level, prefetch_metadata);
}
// LCOV_EXCL_STOP

void CACHE::finish_packet(const response_type& packet)
{
  // check MSHR information
  auto mshr_entry = std::find_if(std::begin(MSHR), std::end(MSHR), matches_address(packet.address));

  // sanity check
  if (mshr_entry == MSHR.end()) {
    fmt::print(stderr, "[{}_MSHR] {} cannot find a matching entry! address: {} v_address: {}\n", NAME, __func__, packet.address, packet.v_address);
    assert(0);
  }

  // MSHR holds the most updated information about this request
  fill_type::returned_value finished_value{packet.data, packet.pf_metadata};
  if (hybrid_llc && !warmup) {
    // Fix 1: forward the returned data to the upper level after the normal
    // fill latency. The array write happens in the background: it holds a
    // port of the destination medium for the full write latency (so it delays
    // later reads and writes to that medium) but is not on the miss's critical path.
    const auto sram = mshr_entry->target_sram;
    reserve_medium_port(sram, true, medium_latency(sram, true));
  }
  mshr_entry->data_promise = champsim::waitable{finished_value, current_time + (warmup ? champsim::chrono::clock::duration{} : FILL_LATENCY)};
  if constexpr (champsim::debug_print) {
    fmt::print("[{}_MSHR] finish_packet instr_id: {} address: {} data: {} type: {} current: {}\n", this->NAME, mshr_entry->instr_id, mshr_entry->address,
               mshr_entry->data_promise->data, access_type_names.at(champsim::to_underlying(mshr_entry->type)), current_time.time_since_epoch() / clock_period);
  }

  std::iter_swap(mshr_entry, std::begin(MSHR));
  inflight_fills.push_back(MSHR.front());
  MSHR.pop_front();
}

void CACHE::finish_translation(const response_type& packet)
{
  auto matches_vpage = [page_num = champsim::page_number{packet.v_address}](const auto& entry) {
    return (champsim::page_number{entry.v_address} == page_num) && !entry.is_translated;
  };
  auto mark_translated = [p_page = champsim::page_number{packet.data}, this](auto& entry) {
    [[maybe_unused]] auto old_address = entry.address;
    entry.address = champsim::address{champsim::splice(p_page, champsim::page_offset{entry.v_address})}; // translated address
    entry.is_translated = true;                                                                          // This entry is now translated

    if constexpr (champsim::debug_print) {
      fmt::print("[{}_TRANSLATE] finish_translation old: {} paddr: {} vaddr: {} type: {} cycle: {}\n", this->NAME, old_address, entry.address, entry.v_address,
                 access_type_names.at(champsim::to_underlying(entry.type)), this->current_time.time_since_epoch() / this->clock_period);
    }
  };

  // Restart stashed translations
  auto finish_begin = std::find_if_not(std::begin(translation_stash), std::end(translation_stash), [](const auto& x) { return x.is_translated; });
  auto finish_end = std::stable_partition(finish_begin, std::end(translation_stash), matches_vpage);
  std::for_each(finish_begin, finish_end, mark_translated);

  // Find all packets that match the page of the returned packet
  for (auto& entry : inflight_tag_check) {
    if (matches_vpage(entry)) {
      mark_translated(entry);
    }
  }
}

void CACHE::issue_translation(tag_lookup_type& q_entry) const
{
  if (!q_entry.translate_issued && !q_entry.is_translated) {
    request_type fwd_pkt;
    fwd_pkt.asid[0] = q_entry.asid[0];
    fwd_pkt.asid[1] = q_entry.asid[1];
    fwd_pkt.type = access_type::LOAD;
    fwd_pkt.cpu = q_entry.cpu;

    fwd_pkt.address = q_entry.address;
    fwd_pkt.v_address = q_entry.v_address;
    fwd_pkt.data = q_entry.data;
    fwd_pkt.instr_id = q_entry.instr_id;
    fwd_pkt.ip = q_entry.ip;

    fwd_pkt.instr_depend_on_me = q_entry.instr_depend_on_me;
    fwd_pkt.is_translated = true;

    q_entry.translate_issued = lower_translate->add_rq(fwd_pkt);
    if constexpr (champsim::debug_print) {
      if (q_entry.translate_issued) {
        fmt::print("[TRANSLATE] do_issue_translation instr_id: {} paddr: {} vaddr: {} type: {}\n", q_entry.instr_id, q_entry.address, q_entry.v_address,
                   access_type_names.at(champsim::to_underlying(q_entry.type)));
      }
    }
  }
}

std::size_t CACHE::get_mshr_occupancy() const { return std::size(MSHR); }

std::vector<std::size_t> CACHE::get_rq_occupancy() const
{
  std::vector<std::size_t> retval;
  std::transform(std::begin(upper_levels), std::end(upper_levels), std::back_inserter(retval), [](auto ulptr) { return ulptr->rq_occupancy(); });
  return retval;
}

std::vector<std::size_t> CACHE::get_wq_occupancy() const
{
  std::vector<std::size_t> retval;
  std::transform(std::begin(upper_levels), std::end(upper_levels), std::back_inserter(retval), [](auto ulptr) { return ulptr->wq_occupancy(); });
  return retval;
}

std::vector<std::size_t> CACHE::get_pq_occupancy() const
{
  std::vector<std::size_t> retval;
  std::transform(std::begin(upper_levels), std::end(upper_levels), std::back_inserter(retval), [](auto ulptr) { return ulptr->pq_occupancy(); });
  retval.push_back(std::size(internal_PQ));
  return retval;
}

// LCOV_EXCL_START exclude deprecated function
std::size_t CACHE::get_occupancy(uint8_t queue_type, uint64_t /*deprecated*/) const
{
  if (queue_type == 0) {
    return get_mshr_occupancy();
  }
  return 0;
}

std::size_t CACHE::get_occupancy(uint8_t queue_type, champsim::address /*deprecated*/) const
{
  if (queue_type == 0) {
    return get_mshr_occupancy();
  }
  return 0;
}
// LCOV_EXCL_STOP

std::size_t CACHE::get_mshr_size() const { return MSHR_SIZE; }
std::vector<std::size_t> CACHE::get_rq_size() const
{
  std::vector<std::size_t> retval;
  std::transform(std::begin(upper_levels), std::end(upper_levels), std::back_inserter(retval), [](auto ulptr) { return ulptr->rq_size(); });
  return retval;
}

std::vector<std::size_t> CACHE::get_wq_size() const
{
  std::vector<std::size_t> retval;
  std::transform(std::begin(upper_levels), std::end(upper_levels), std::back_inserter(retval), [](auto ulptr) { return ulptr->wq_size(); });
  return retval;
}

std::vector<std::size_t> CACHE::get_pq_size() const
{
  std::vector<std::size_t> retval;
  std::transform(std::begin(upper_levels), std::end(upper_levels), std::back_inserter(retval), [](auto ulptr) { return ulptr->pq_size(); });
  retval.push_back(PQ_SIZE);
  return retval;
}

// LCOV_EXCL_START exclude deprecated function
std::size_t CACHE::get_size(uint8_t queue_type, champsim::address /*deprecated*/) const
{
  if (queue_type == 0) {
    return get_mshr_size();
  }
  return 0;
}

std::size_t CACHE::get_size(uint8_t queue_type, uint64_t /*deprecated*/) const
{
  if (queue_type == 0) {
    return get_mshr_size();
  }
  return 0;
}
// LCOV_EXCL_STOP

namespace
{
double occupancy_ratio(std::size_t occ, std::size_t sz) { return std::ceil(occ) / std::ceil(sz); }

std::vector<double> occupancy_ratio_vec(std::vector<std::size_t> occ, std::vector<std::size_t> sz)
{
  std::vector<double> retval;
  std::transform(std::begin(occ), std::end(occ), std::begin(sz), std::back_inserter(retval), occupancy_ratio);
  return retval;
}
} // namespace

double CACHE::get_mshr_occupancy_ratio() const { return ::occupancy_ratio(get_mshr_occupancy(), get_mshr_size()); }

std::vector<double> CACHE::get_rq_occupancy_ratio() const { return ::occupancy_ratio_vec(get_rq_occupancy(), get_rq_size()); }

std::vector<double> CACHE::get_wq_occupancy_ratio() const { return ::occupancy_ratio_vec(get_wq_occupancy(), get_wq_size()); }

std::vector<double> CACHE::get_pq_occupancy_ratio() const { return ::occupancy_ratio_vec(get_pq_occupancy(), get_pq_size()); }

void CACHE::impl_prefetcher_initialize() const { pref_module_pimpl->impl_prefetcher_initialize(); }

uint32_t CACHE::impl_prefetcher_cache_operate(champsim::address addr, champsim::address ip, bool cache_hit, bool useful_prefetch, access_type type,
                                              uint32_t metadata_in) const
{
  return pref_module_pimpl->impl_prefetcher_cache_operate(addr, ip, cache_hit, useful_prefetch, type, metadata_in);
}

uint32_t CACHE::impl_prefetcher_cache_fill(champsim::address addr, long set, long way, bool prefetch, champsim::address evicted_addr,
                                           uint32_t metadata_in) const
{
  return pref_module_pimpl->impl_prefetcher_cache_fill(addr, set, way, prefetch, evicted_addr, metadata_in);
}

void CACHE::impl_prefetcher_cycle_operate() const { pref_module_pimpl->impl_prefetcher_cycle_operate(); }

void CACHE::impl_prefetcher_final_stats() const { pref_module_pimpl->impl_prefetcher_final_stats(); }

void CACHE::impl_prefetcher_branch_operate(champsim::address ip, uint8_t branch_type, champsim::address branch_target) const
{
  pref_module_pimpl->impl_prefetcher_branch_operate(ip, branch_type, branch_target);
}

void CACHE::impl_initialize_replacement() const { repl_module_pimpl->impl_initialize_replacement(); }

long CACHE::impl_find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const BLOCK* current_set, champsim::address ip, champsim::address full_addr,
                             access_type type, bool target_sram) const
{
  return repl_module_pimpl->impl_find_victim(triggering_cpu, instr_id, set, current_set, ip, full_addr, type, target_sram);
}

void CACHE::impl_update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                          champsim::address victim_addr, access_type type, bool hit) const
{
  repl_module_pimpl->impl_update_replacement_state(triggering_cpu, set, way, full_addr, ip, victim_addr, type, hit);
}

void CACHE::impl_replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                        champsim::address victim_addr, access_type type) const
{
  repl_module_pimpl->impl_replacement_cache_fill(triggering_cpu, set, way, full_addr, ip, victim_addr, type);
}

void CACHE::impl_replacement_final_stats() const { repl_module_pimpl->impl_replacement_final_stats(); }

void CACHE::initialize()
{
  impl_prefetcher_initialize();
  impl_initialize_replacement();
}

void CACHE::begin_phase()
{
  stats_type new_roi_stats;
  stats_type new_sim_stats;

  new_roi_stats.name = NAME;
  new_sim_stats.name = NAME;

  roi_stats = new_roi_stats;
  sim_stats = new_sim_stats;

  for (auto* ul : upper_levels) {
    channel_type::stats_type ul_new_roi_stats;
    channel_type::stats_type ul_new_sim_stats;
    ul->roi_stats = ul_new_roi_stats;
    ul->sim_stats = ul_new_sim_stats;
  }
}

void CACHE::end_phase(unsigned finished_cpu)
{
  finished_cpu = finished_cpu;
  roi_stats.total_miss_latency_cycles = sim_stats.total_miss_latency_cycles;

  roi_stats.hits = sim_stats.hits;
  roi_stats.misses = sim_stats.misses;
  roi_stats.miss_merge = sim_stats.miss_merge;
  roi_stats.fill = sim_stats.fill;

  roi_stats.pf_requested = sim_stats.pf_requested;
  roi_stats.pf_issued = sim_stats.pf_issued;
  roi_stats.pf_useful = sim_stats.pf_useful;
  roi_stats.pf_useless = sim_stats.pf_useless;
  roi_stats.pf_fill = sim_stats.pf_fill;
  roi_stats.sram_reads = sim_stats.sram_reads;
  roi_stats.sram_writes = sim_stats.sram_writes;
  roi_stats.sttram_reads = sim_stats.sttram_reads;
  roi_stats.sttram_writes = sim_stats.sttram_writes;
  roi_stats.phc_sram_placements = sim_stats.phc_sram_placements;
  roi_stats.phc_sttram_placements = sim_stats.phc_sttram_placements;
  roi_stats.phc_hot_training_events = sim_stats.phc_hot_training_events;
  roi_stats.phc_cold_training_events = sim_stats.phc_cold_training_events;
  roi_stats.dam_dead_victims = sim_stats.dam_dead_victims;
  roi_stats.dam_lru_victims = sim_stats.dam_lru_victims;
  roi_stats.dam_dead_in_other_portion = sim_stats.dam_dead_in_other_portion;
  roi_stats.dam_reuse_training = sim_stats.dam_reuse_training;
  roi_stats.dam_dead_training = sim_stats.dam_dead_training;
  roi_stats.dam_phases = sim_stats.dam_phases;
  roi_stats.dam_phases_b = sim_stats.dam_phases_b;

  for (auto* ul : upper_levels) {
    ul->roi_stats.RQ_ACCESS = ul->sim_stats.RQ_ACCESS;
    ul->roi_stats.RQ_FULL = ul->sim_stats.RQ_FULL;
    ul->roi_stats.RQ_TO_CACHE = ul->sim_stats.RQ_TO_CACHE;

    ul->roi_stats.PQ_ACCESS = ul->sim_stats.PQ_ACCESS;
    ul->roi_stats.PQ_FULL = ul->sim_stats.PQ_FULL;
    ul->roi_stats.PQ_TO_CACHE = ul->sim_stats.PQ_TO_CACHE;

    ul->roi_stats.WQ_ACCESS = ul->sim_stats.WQ_ACCESS;
    ul->roi_stats.WQ_FULL = ul->sim_stats.WQ_FULL;
    ul->roi_stats.WQ_TO_CACHE = ul->sim_stats.WQ_TO_CACHE;
  }
}

template <typename T>
bool CACHE::should_activate_prefetcher(const T& pkt) const
{
  return !pkt.prefetch_from_this && std::count(std::begin(pref_activate_mask), std::end(pref_activate_mask), pkt.type) > 0;
}

// LCOV_EXCL_START Exclude the following function from LCOV
void CACHE::print_deadlock()
{
  std::string_view mshr_write{"instr_id: {} address: {} v_addr: {} type: {} ready: {}"};
  auto mshr_pack = [time = current_time](const auto& entry) {
    return std::tuple{entry.instr_id, entry.address, entry.v_address, access_type_names.at(champsim::to_underlying(entry.type)),
                      entry.data_promise.is_ready_at(time)};
  };

  std::string_view tag_check_write{"instr_id: {} address: {} v_addr: {} is_translated: {} translate_issued: {} event_cycle: {}"};
  auto tag_check_pack = [period = clock_period](const auto& entry) {
    return std::tuple{entry.instr_id,      entry.address,          entry.v_address,
                      entry.is_translated, entry.translate_issued, entry.event_cycle.time_since_epoch() / period};
  };

  champsim::range_print_deadlock(MSHR, NAME + "_MSHR", mshr_write, mshr_pack);
  champsim::range_print_deadlock(inflight_tag_check, NAME + "_tags", tag_check_write, tag_check_pack);
  champsim::range_print_deadlock(translation_stash, NAME + "_translation", tag_check_write, tag_check_pack);

  std::string_view q_writer{"instr_id: {} address: {} v_addr: {} type: {} translated: {}"};
  auto q_entry_pack = [](const auto& entry) {
    return std::tuple{entry.instr_id, entry.address, entry.v_address, access_type_names.at(champsim::to_underlying(entry.type)), entry.is_translated};
  };

  for (auto* ul : upper_levels) {
    champsim::range_print_deadlock(ul->RQ, NAME + "_RQ", q_writer, q_entry_pack);
    champsim::range_print_deadlock(ul->WQ, NAME + "_WQ", q_writer, q_entry_pack);
    champsim::range_print_deadlock(ul->PQ, NAME + "_PQ", q_writer, q_entry_pack);
  }
}
// LCOV_EXCL_STOP
