#include <cmath>

#include "ns3/assert.h"
#include "ns3/log.h"

#include "aimf-header.h"

#define IPV4_ADDRESS_SIZE 4
#define AIMF_MSG_HEADER_SIZE 11
#define AIMF_PKT_HEADER_SIZE 4

namespace ns3 {

    NS_LOG_COMPONENT_DEFINE("AimfHeader");

    namespace aimf {


        
#define AIMF_C 0.0625

        ///
        /// \brief Converts a decimal number of seconds to the mantissa/exponent format.
        ///
        /// \param seconds decimal number of seconds we want to convert.
        /// \return the number of seconds in mantissa/exponent format.
        ///

        uint8_t
        SecondsToEmf(double seconds) {
            int a, b = 0;

            // find the largest integer 'b' such that: T/C >= 2^b
            for (b = 0; (seconds / AIMF_C) >= (1 << b); ++b)
                ;
            NS_ASSERT((seconds / AIMF_C) < (1 << b));
            b--;
            NS_ASSERT((seconds / AIMF_C) >= (1 << b));

            // compute the expression 16*(T/(C*(2^b))-1), which may not be a integer
            double tmp = 16 * (seconds / (AIMF_C * (1 << b)) - 1);

            // round it up.  This results in the value for 'a'
            a = (int) std::ceil(tmp);

            // if 'a' is equal to 16: increment 'b' by one, and set 'a' to 0
            if (a == 16) {
                b += 1;
                a = 0;
            }

            // now, 'a' and 'b' should be integers between 0 and 15,
            NS_ASSERT(a >= 0 && a < 16);
            NS_ASSERT(b >= 0 && b < 16);

            // the field will be a byte holding the value a*16+b
            return (uint8_t) ((a << 4) | b);
        }

        ///
        /// \brief Converts a number of seconds in the mantissa/exponent format to a decimal number.
        ///
        /// \param aimf_format number of seconds in mantissa/exponent format.
        /// \return the decimal number of seconds.
        ///

        double
        EmfToSeconds(uint8_t aimfFormat) {
            int a = (aimfFormat >> 4);
            int b = (aimfFormat & 0xf);
            // value = C*(1+a/16)*2^b [in seconds]
            return AIMF_C * (1 + a / 16.0) * (1 << b);
        }



        // ---------------- AIMF Packet -------------------------------

        NS_OBJECT_ENSURE_REGISTERED(PacketHeader);

        PacketHeader::PacketHeader() {
        }

        PacketHeader::~PacketHeader() {
        }

        TypeId
        PacketHeader::GetTypeId(void) {
            static TypeId tid = TypeId("ns3::aimf::PacketHeader")
                    .SetParent<Header> ()
                    .SetGroupName("Aimf")
                    .AddConstructor<PacketHeader> ()
                    ;
            return tid;
        }

        TypeId
        PacketHeader::GetInstanceTypeId(void) const {
            return GetTypeId();
        }

        uint32_t
        PacketHeader::GetSerializedSize(void) const {
            return AIMF_PKT_HEADER_SIZE;
        }

        void
        PacketHeader::Print(std::ostream &os) const {
            /// \todo
        }

        void
        PacketHeader::Serialize(Buffer::Iterator start) const {
            Buffer::Iterator i = start;
            i.WriteHtonU16(m_packetLength);
            i.WriteHtonU16(m_packetSequenceNumber);
        }

        uint32_t
        PacketHeader::Deserialize(Buffer::Iterator start) {
            Buffer::Iterator i = start;
            m_packetLength = i.ReadNtohU16();
            m_packetSequenceNumber = i.ReadNtohU16();
            return GetSerializedSize();
        }


        // ---------------- AIMF Message -------------------------------

        NS_OBJECT_ENSURE_REGISTERED(MessageHeader);

        MessageHeader::MessageHeader()
        : m_messageType(MessageHeader::MessageType(0)) {
        }

        MessageHeader::~MessageHeader() {
        }

        TypeId
        MessageHeader::GetTypeId(void) {
            static TypeId tid = TypeId("ns3::aimf::MessageHeader")
                    .SetParent<Header> ()
                    .SetGroupName("Aimf")
                    .AddConstructor<MessageHeader> ()
                    ;
            return tid;
        }

        TypeId
        MessageHeader::GetInstanceTypeId(void) const {
            return GetTypeId();
        }

        uint32_t
        MessageHeader::GetSerializedSize(void) const {
            uint32_t size = AIMF_MSG_HEADER_SIZE;

            NS_LOG_DEBUG("Hello Message Size: " << size << " + " << m_message.hello.GetSerializedSize());
            size += m_message.hello.GetSerializedSize();

            return size;
        }

        void
        MessageHeader::Print(std::ostream &os) const {
            /// \todo
        }

        void
        MessageHeader::Serialize(Buffer::Iterator start) const {
            Buffer::Iterator i = start;
            i.WriteU8(m_messageType);
            i.WriteU8(m_vTime);
            i.WriteHtonU16(this->GetSerializedSize());
            i.WriteHtonU32(m_originatorAddress.Get());
            i.WriteU8(m_timeToLive);
            i.WriteHtonU16(m_messageSequenceNumber);
            m_message.hello.Serialize(i);


        }

        uint32_t
        MessageHeader::Deserialize(Buffer::Iterator start) {
            uint32_t size;
            Buffer::Iterator i = start;
            m_messageType = (MessageType) i.ReadU8();
            NS_ASSERT(m_messageType == HELLO_MESSAGE);
            m_vTime = i.ReadU8();
            m_messageSize = i.ReadNtohU16();
            m_originatorAddress = Ipv4Address(i.ReadNtohU32());
            m_timeToLive = i.ReadU8();            
            m_messageSequenceNumber = i.ReadNtohU16();
            size = AIMF_MSG_HEADER_SIZE;

            size += m_message.hello.Deserialize(i, m_messageSize - AIMF_MSG_HEADER_SIZE);

            return size;
        }

        // ---------------- AIMF HELLO Message -------------------------------

        uint32_t
        MessageHeader::Hello::GetSerializedSize(void) const {
            uint32_t size = 2;
            size += 2 * this->associations.size() * IPV4_ADDRESS_SIZE;
            return size;
        }

        void
        MessageHeader::Hello::Print(std::ostream &os) const {
            /// \todo
        }

        void
        MessageHeader::Hello::Serialize(Buffer::Iterator start) const {
            Buffer::Iterator i = start;


            i.WriteU8(this->hTime);
            i.WriteU8(this->willingness);
            

            for (size_t n = 0; n < this->associations.size(); ++n) {
                i.WriteHtonU32(this->associations[n].group.Get());
                i.WriteHtonU32(this->associations[n].source.Get());
            }
        }

        uint32_t
        MessageHeader::Hello::Deserialize(Buffer::Iterator start, uint32_t messageSize) {
            Buffer::Iterator i = start;


            
            this->hTime = i.ReadU8();
            this->willingness = i.ReadU8();
            
        

            NS_ASSERT((messageSize-2) % (IPV4_ADDRESS_SIZE * 2) == 0);
            int numAddresses = (messageSize-2) / IPV4_ADDRESS_SIZE / 2;
            this->associations.clear();
            for (int n = 0; n < numAddresses; ++n) {
                Ipv4Address group(i.ReadNtohU32());
                Ipv4Address source(i.ReadNtohU32());

                this->associations.push_back((Association) {
                    group, source
                });
            }


            return messageSize;
        }



        

    }
} // namespace aimf, ns3

