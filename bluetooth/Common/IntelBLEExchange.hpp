/* SPDX-License-Identifier: MIT
 * Bounded receipt for the loader-owned capability exchange, not a native hook.
 */
#pragma once
#include <stdint.h>
namespace IntelBLE {
struct Exchange {
 uint32_t schema,bytes,opcode,submitted,events,unrelated;
 uint32_t firstBytes,firstSaved,transportStatus,terminal,budgetMs,ownership;
 uint32_t elapsedMs,reserved;
 uint8_t first[72];
};
static_assert(sizeof(Exchange)==128,"capability exchange receipt extent");
}
