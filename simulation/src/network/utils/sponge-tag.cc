#include "sponge-tag.h"

namespace ns3
{
    SpongeTag::SpongeTag() : m_osip(0), m_odip(0), m_osport(0), m_odport(0), m_opg(0) {}

    TypeId SpongeTag::GetTypeId()
    {
        static TypeId tid = TypeId("ns3::SpongeTag")
                                .SetParent<Tag>()
                                .AddConstructor<SpongeTag>();
        return tid;
    }

    TypeId SpongeTag::GetInstanceTypeId() const
    {
        return GetTypeId();
    }

    uint32_t SpongeTag::GetSerializedSize() const
    {
        return sizeof(m_osip) + sizeof(m_odip) + sizeof(m_osport) + sizeof(m_odport) + sizeof(m_opg);
    }

    void SpongeTag::Serialize(TagBuffer i) const
    {
        i.WriteU32(m_osip);
        i.WriteU32(m_odip);
        i.WriteU16(m_osport);
        i.WriteU16(m_odport);
        i.WriteU16(m_opg);
    }

    void SpongeTag::Deserialize(TagBuffer i)
    {
        m_osip = i.ReadU32();
        m_odip = i.ReadU32();
        m_osport = i.ReadU16();
        m_odport = i.ReadU16();
        m_opg = i.ReadU16();
    }

    void SpongeTag::Print(std::ostream &os) const
    {
        os << "sip=" << m_osip << " dip=" << m_odip << " sport=" << m_osport << " dport=" << m_odport << " pg=" << m_opg;
    }

    void SpongeTag::SetFlowInfo(uint32_t osip, uint32_t odip, uint16_t osport, uint16_t odport, uint16_t opg)
    {
        m_osip = osip;
        m_odip = odip;
        m_osport = osport;
        m_odport = odport;
        m_opg = opg;
    }

    uint32_t SpongeTag::GetOriginalSrcIp() const { return m_osip; }

    uint32_t SpongeTag::GetOriginalDstIp() const { return m_odip; }

    uint16_t SpongeTag::GetOriginalSrcPort() const { return m_osport; }

    uint16_t SpongeTag::GetOriginalDstPort() const { return m_odport; }

    uint16_t SpongeTag::GetOriginalPG() const { return m_opg; }
}
