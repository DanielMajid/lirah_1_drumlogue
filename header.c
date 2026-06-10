/**
 *  @file header.c
 *  @brief drumlogue SDK unit header for Lirah-1 synth
 *
 *  Copyright (c) 2026.
 */

#include "unit.h"  // Note: Include common definitions for all units

// ---- Unit header definition  --------------------------------------------------------------------

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_synth,
    .api = UNIT_API_VERSION,
    .dev_id = 0x4D616A69U,
    .unit_id = 0x00000004U,
    .version = 0x00010001U,
    .name = "Lirah-1Drm",
    .num_presets = 8,
    .num_params = 10,
    .params = {
        {0, 1023, 0, 0, k_unit_param_type_none, 0, 0, 0, {"FM DEPTH"}},
        {0, 1023, 0, 0, k_unit_param_type_none, 0, 0, 0, {"HYPER LFO"}},
        {0, 100, 0, 10, k_unit_param_type_none, 0, 0, 0, {"LFO1 RATE"}},
        {0, 100, 0, 20, k_unit_param_type_none, 0, 0, 0, {"LFO2 RATE"}},
        {0, 100, 0, 0, k_unit_param_type_none, 0, 0, 0, {"FOLD"}},
        {0, 100, 0, 0, k_unit_param_type_none, 0, 0, 0, {"FM TUNE"}},
        {0, 100, 0, 0, k_unit_param_type_none, 0, 0, 0, {"PITCH"}},
        {0, 100, 0, 0, k_unit_param_type_none, 0, 0, 0, {"FEEDBACK"}},
        {0, 8, 0, 0, k_unit_param_type_strings, 0, 0, 0, {"LFO TARGET"}},
        {0, 100, 0, 10, k_unit_param_type_none, 0, 0, 0, {"LFO RATE"}},

        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}},
        {0, 0, 0, 0, k_unit_param_type_none, 0, 0, 0, {""}}}};
