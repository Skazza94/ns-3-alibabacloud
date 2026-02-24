#ifndef SPONGE_HEADER_H
#define SPONGE_HEADER_H

#include <stdint.h>
#include "ns3/header.h"

namespace ns3
{
    class SpongeHeader
    {
    public:
        uint8_t m_enabled = 0;
        uint32_t m_osip = 0;
        uint32_t m_odip = 0;
        uint32_t m_osport = 0;
        uint32_t m_odport = 0;
        uint16_t m_opg = 0;
        
        void Serialize(ns3::Buffer::Iterator start) const;
        uint32_t Deserialize(ns3::Buffer::Iterator start);
        static uint32_t GetStaticSize();
    };
}

#endif