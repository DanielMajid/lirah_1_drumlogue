/**
 *  @file header.c
 *  @brief drumlogue SDK unit header for Lirah-4 synth
 *
 *  Copyright (c) 2026.
 */

#include "unit.h"  // Drumlogue unit header types and constants.

// ---- Unit header definition  --------------------------------------------------------------------

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_synth,
    .api = UNIT_API_VERSION,
    .dev_id = 0x4D616A69U,
    .unit_id = 0x00000005U,
    // Version format: 0x00MMmmpp (major/minor/patch)
    .version = 0x00000102U,
    .name = "Lirah-4",
    .num_presets = 8,
    .num_params = 19,
    .params = {
        // ---- Core oscillator parameters ------------------------------------
        {0, 1023, 0,  0, k_unit_param_type_none,    0, 0, 0, {"FM DEPTH"}},
        {0, 1023, 0,  0, k_unit_param_type_none,    0, 0, 0, {"HYPER LFO"}},
        {0,  100, 0, 10, k_unit_param_type_none,    0, 0, 0, {"LFO1 RATE"}},
        {0,  100, 0, 20, k_unit_param_type_none,    0, 0, 0, {"LFO2 RATE"}},
        {0,  100, 0,  0, k_unit_param_type_none,    0, 0, 0, {"FOLD"}},
        {0,  100, 0,  0, k_unit_param_type_none,    0, 0, 0, {"FM TUNE"}},
        {0,  100, 0,  0, k_unit_param_type_none,    0, 0, 0, {"PITCH"}},
        {0,  100, 0,  0, k_unit_param_type_none,    0, 0, 0, {"FEEDBACK"}},
        {0,    8, 0,  0, k_unit_param_type_strings, 0, 0, 0, {"LFO TARGET"}},
        {0,  100, 0, 10, k_unit_param_type_none,    0, 0, 0, {"LFO RATE"}},
        {0,  100, 0,  0, k_unit_param_type_none,    0, 0, 0, {"LFO DEPTH"}},
        // ---- Voice spread parameters (params 11-14) ------------------------
        // Each spreads the named parameter across the 4 voices symmetrically.
        // At 0 all voices are identical; increasing the value widens the spread.
        {0,  100, 0,  0, k_unit_param_type_none,    0, 0, 0, {"FM SPREAD"}},
        {0,  100, 0,  0, k_unit_param_type_none,    0, 0, 0, {"FOLD SPRD"}},
        {0,  100, 0,  0, k_unit_param_type_none,    0, 0, 0, {"FDBK SPRD"}},
        {0,  100, 0,  0, k_unit_param_type_none,    0, 0, 0, {"TUN SPREAD"}},
        // ---- Envelope parameters (params 15-18) ---------------------------
        // ENV TYPE options: 0=AR, 1=ADSR, 2=AHR, 3=LOOP, 4=OPEN
        {0,    4, 0,  0, k_unit_param_type_strings, 0, 0, 0, {"ENV TYPE"}},
        {0,    2, 0,  1, k_unit_param_type_strings, 0, 0, 0, {"ENV RANGE"}},
        {0,  100, 0, 20, k_unit_param_type_none,    0, 0, 0, {"ATTACK"}},
        {0,  100, 0, 35, k_unit_param_type_none,    0, 0, 0, {"RELEASE"}},
        // ---- Unused slots (SDK requires 24 entries total) -------------------
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}}};
