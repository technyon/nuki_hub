// Copyright 2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

/** Major version number (X.x.x) */
#define PSYCHIC_VERSION_MAJOR 2
/** Minor version number (x.X.x) */
#define PSYCHIC_VERSION_MINOR 0
/** Patch version number (x.x.X) */
#define PSYCHIC_VERSION_PATCH 0

/**
 * Macro to convert PsychicHttp version number into an integer
 *
 * To be used in comparisons, such as PSYCHIC_VERSION >= PSYCHIC_VERSION_VAL(2, 0, 0)
 */
#define PSYCHIC_VERSION_VAL(major, minor, patch) ((major << 16) | (minor << 8) | (patch))

/**
 * Current PsychicHttp version, as an integer
 *
 * To be used in comparisons, such as PSYCHIC_VERSION >= PSYCHIC_VERSION_VAL(2, 0, 0)
 */
#define PSYCHIC_VERSION PSYCHIC_VERSION_VAL(PSYCHIC_VERSION_MAJOR, PSYCHIC_VERSION_MINOR, PSYCHIC_VERSION_PATCH)

/**
 * Current PsychicHttp version, as string
 */
#ifndef PSYCHIC_df2xstr
#define PSYCHIC_df2xstr(s)          #s
#endif
#ifndef PSYCHIC_df2str
#define PSYCHIC_df2str(s)           PSYCHIC_df2xstr(s)
#endif
#define PSYCHIC_VERSION_STR PSYCHIC_df2str(PSYCHIC_VERSION_MAJOR) "." PSYCHIC_df2str(PSYCHIC_VERSION_MINOR) "." PSYCHIC_df2str(PSYCHIC_VERSION_PATCH)
