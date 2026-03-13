#ifndef SPONGE_TAG_H
#define SPONGE_TAG_H

#include "ns3/tag.h"

namespace ns3
{
    class SpongeTag : public Tag
    {
    public:
        SpongeTag();

        static TypeId GetTypeId();
        TypeId GetInstanceTypeId() const override;

        uint32_t GetSerializedSize() const override;
        void Serialize(TagBuffer i) const override;
        void Deserialize(TagBuffer i) override;
        void Print(std::ostream &os) const override;

        void SetFlowInfo(uint32_t osip, uint32_t odip, uint16_t osport, uint16_t odport, uint16_t opg);

        uint32_t GetOriginalSrcIp() const;
        uint32_t GetOriginalDstIp() const;
        uint16_t GetOriginalSrcPort() const;
        uint16_t GetOriginalDstPort() const;
        uint16_t GetOriginalPG() const;

    private:
        uint32_t m_osip;
        uint32_t m_odip;
        uint16_t m_osport;
        uint16_t m_odport;
        uint16_t m_opg;
    };
}

#endif
