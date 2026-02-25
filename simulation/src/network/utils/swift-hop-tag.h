#ifndef SWIFT_HOP_TAG
#define SWIFT_HOP_TAG
#include "ns3/tag.h"

namespace ns3
{
    class SwiftHopTag : public Tag
    {
    public:
        SwiftHopTag();

        static TypeId GetTypeId();
        TypeId GetInstanceTypeId() const override;

        uint32_t GetSerializedSize() const override;
        void Serialize(TagBuffer i) const override;
        void Deserialize(TagBuffer i) override;
        void Print(std::ostream &os) const override;

        void SetHops(uint32_t hops);
        void IncHops();
        uint32_t GetHops() const;

    private:
        uint32_t m_hops;
    };
}

#endif /* SWIFT_HOP_TAG */
