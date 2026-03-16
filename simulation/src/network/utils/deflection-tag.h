#ifndef DEFLECTION_TAG_H
#define DEFLECTION_TAG_H

#include "ns3/tag.h"

namespace ns3
{
    class DeflectionTag : public Tag
    {
    public:
        DeflectionTag();

        static TypeId GetTypeId();
        TypeId GetInstanceTypeId() const override;

        uint32_t GetSerializedSize() const override;
        void Serialize(TagBuffer i) const override;
        void Deserialize(TagBuffer i) override;
        void Print(std::ostream &os) const override;

        void Increment();
        uint16_t GetDeflections() const;

    private:
        uint16_t m_deflections;
    };
}

#endif /* DEFLECTION_TAG_H */
