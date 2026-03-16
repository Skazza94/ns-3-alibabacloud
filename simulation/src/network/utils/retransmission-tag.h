#ifndef RETRANSMISSION_TAG_H
#define RETRANSMISSION_TAG_H

#include "ns3/tag.h"

namespace ns3
{
    class RetransmissionTag : public Tag
    {
    public:
        RetransmissionTag();

        static TypeId GetTypeId();
        TypeId GetInstanceTypeId() const override;

        uint32_t GetSerializedSize() const override;
        void Serialize(TagBuffer i) const override;
        void Deserialize(TagBuffer i) override;
        void Print(std::ostream &os) const override;

        void SetRetransmissions(uint32_t n);
        uint32_t GetRetransmissions() const;

    private:
        uint32_t m_rtx;
    };
}

#endif /* RETRANSMISSION_TAG_H */
