#ifndef THEMIS_LOOP_TAG_H
#define THEMIS_LOOP_TAG_H
#include "ns3/tag.h"

namespace ns3
{
    class ThemisLoopTag : public Tag
    {
    public:
        ThemisLoopTag();
        explicit ThemisLoopTag(int32_t left);

        static TypeId GetTypeId();
        TypeId GetInstanceTypeId() const override;

        uint32_t GetSerializedSize() const override;
        void Serialize(TagBuffer i) const override;
        void Deserialize(TagBuffer i) override;
        void Print(std::ostream &os) const override;

        void SetLeft(int32_t left);
        int32_t GetLeft() const;

    private:
        int32_t m_left; /* -1 means "not initialized", 0 means "done" */
    };
}

#endif /* THEMIS_LOOP_TAG_H */
