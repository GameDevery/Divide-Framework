#pragma once
#ifndef DVD_WORLDPACKET_H
#define DVD_WORLDPACKET_H

#include "OPCodesTpl.h"

#include "Utils.h"
#include "Core/Headers/ByteBuffer.h"

#include <boost/asio/streambuf.hpp>

namespace Divide {

class WorldPacket : public ByteBuffer {
   public:
    WorldPacket() noexcept;

    explicit WorldPacket(const OPCodes::ValueType opCode, const size_t res = 200);
    void Initialize(const U16 opCode, const size_t newres = 200);
    PROPERTY_RW( OPCodes::ValueType, opcode );

    [[nodiscard]] bool loadFromBuffer(boost::asio::streambuf& buf);
    [[nodiscard]] bool saveToBuffer(boost::asio::streambuf& buf) const;


    /*template <typename Archive>
    void load(Archive& ar, [[maybe_unused]] const unsigned int version) {
        size_t storageSize = 0;

        ar& _rpos;
        ar& _wpos;
        ar& storageSize;

        _storage.resize(storageSize);
        for (size_t i = 0; i < storageSize; ++i) {
            ar & _storage[i];
        }
        ar& m_opcode;
    }

    template <typename Archive>
    void save(Archive& ar, [[maybe_unused]] const unsigned int version) const {
        const size_t storageSize = _storage.size();

        ar & _rpos;
        ar & _wpos;
        ar & storageSize;
        for (size_t i = 0; i < storageSize; ++i) {
            ar & _storage[i];
        }
        ar & m_opcode;
    }*/

};

};  // namespace Divide

#endif //DVD_WORLDPACKET_H
