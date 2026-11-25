// shot_summary.h - turns the per-block detector output into one result per shot.
//
// Same rules as ShotTracker in tools/showcase.py:
//  * a shot starts at the first block with FLAG_TRACKING and ends after SHOT_END_GAP blocks without it,
//    when the device disarms, or when a new bullet starts;
//  * the reported speed is that of the strongest block (the dart is nearest the middle of the beam);
//  * tracks slower than the minimum speed are dropped.
// No hardware access, so it is unit-tested on a PC.
#pragma once
#include <stdint.h>

struct ShotResult {
  float    speed_mps;
  float    freq_hz;
  float    mag;
  uint32_t blocks;      // blocks the shot was tracked in
};

static constexpr uint32_t SHOT_END_GAP = 9;     // = DET_LOST_BLOCKS + 1: the detector keeps a lost peak for 8 blocks

void shot_init(float min_speed_mps);

// Feed every processed block. Returns true (and fills *out) in the block that completes a shot.
bool shot_update(uint32_t flags, float speed_mps, float mag, float freq_hz, bool armed, ShotResult *out);

bool shot_active();     // a track is in progress (the display should keep out of the way)
