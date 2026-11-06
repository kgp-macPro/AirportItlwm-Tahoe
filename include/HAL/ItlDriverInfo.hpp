/*
* Copyright (C) 2020  钟先耀
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#ifndef ItlDriverInfo_h
#define ItlDriverInfo_h

struct ItlHeTxReport {
    uint32_t kbps, raw;
    uint32_t age_ms;
    uint8_t mcs, nss, bw, gi, source;
    bool valid; // coherent last-known tuple for this connection
    bool fresh; // observed within 5s; expiry does not erase Legacy PHY status
};

class ItlDriverInfo {
    
public:
    
    virtual const char *getFirmwareVersion() = 0;
    
    virtual int16_t getBSSNoise() = 0;
    
    virtual bool is5GBandSupport() = 0;
    
    virtual int getTxNSS() = 0;
    // true: this HAL owns HE reporting, including explicit unavailable state and age/freshness.
    virtual bool getHeTxReport(ItlHeTxReport &report) { return false; }
    
    virtual const char *getFirmwareName() = 0;
    
    virtual UInt32 supportedFeatures() = 0;

    virtual const char *getFirmwareCountryCode() = 0;

    virtual uint32_t getTxQueueSize() = 0;
};

#endif /* ItlDriverInfo_h */
