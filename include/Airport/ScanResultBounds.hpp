/* Bounded legacy scan-result serialization, KGP 2026. GPL-2.0-or-later. */
#ifndef KGP_SCAN_RESULT_BOUNDS_HPP
#define KGP_SCAN_RESULT_BOUNDS_HPP
#include <stddef.h>
#include <stdint.h>

// Export only complete information elements that fit the destination. A byte
// truncation in the middle of an IE must not be advertised as a complete IE.
inline size_t airportScanIEPrefix(const uint8_t *data,size_t bytes,size_t capacity)
{
    if (!data) return 0;
    const size_t limit=bytes<capacity?bytes:capacity;
    size_t copied=0;
    while (limit-copied>=2) {
        const size_t length=size_t(data[copied+1])+2;
        if (length>limit-copied) break;
        copied+=length;
    }
    return copied;
}
#endif
