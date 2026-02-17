#include "sponge-header.h"
#include "ns3/buffer.h"

namespace ns3
{
    void
    SpongeHeader::Serialize(Buffer::Iterator start) const
    {
        start.WriteU32(m_osip);
        start.WriteU32(m_odip);
        start.WriteU32(m_osport);
        start.WriteU32(m_odport);
    }

    uint32_t
    SpongeHeader::Deserialize(Buffer::Iterator start)
    {
        m_osip = start.ReadU32();
        m_odip = start.ReadU32();
        m_osport = start.ReadU32();
        m_odport = start.ReadU32();
        return GetStaticSize();
    }

    uint32_t SpongeHeader::GetStaticSize()
    {
        return sizeof(m_osip) + sizeof(m_odip) + sizeof(m_osport) + sizeof(m_odport);
    }
}