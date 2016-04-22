/* 
 * File:   aimf-header.h
 * Author: debonatis
 *
 * Created on 24 February 2016, 14:56
 */

#ifndef AIMF_HEADER_H
#define	AIMF_HEADER_H

#include <stdint.h>
#include <vector>
#include "ns3/header.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"

namespace ns3 {
    namespace aimf {
        double EmfToSeconds(uint8_t emf);
        uint8_t SecondsToEmf(double seconds);

        // 3.3.  Packet Format
        //
        //    The basic layout of any packet in AIMF is as follows (omitting IP and
        //    UDP headers):
        //
        //        0                   1                   2                   3
        //        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
        //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //       |         Packet Length         |    Packet Sequence Number     |
        //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //       |  Message Type |     Vtime     |         Message Size          |
        //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //       |                      Originator Address                       |
        //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //       |  Time To Live |    Message Sequence Number    |               |
        //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //       |                                                               |
        //       :                            MESSAGE                            :
        //       |                                                               |
      
      

        class PacketHeader : public Header {
        public:
            PacketHeader();
            virtual ~PacketHeader();

            void SetPacketLength(uint16_t length) {
                m_packetLength = length;
            }

            uint16_t GetPacketLength() const {
                return m_packetLength;
            }

            void SetPacketSequenceNumber(uint16_t seqnum) {
                m_packetSequenceNumber = seqnum;
            }

            uint16_t GetPacketSequenceNumber() const {
                return m_packetSequenceNumber;
            }

        private:
            uint16_t m_packetLength;
            uint16_t m_packetSequenceNumber;

        public:
            static TypeId GetTypeId(void);
            virtual TypeId GetInstanceTypeId(void) const;
            virtual void Print(std::ostream &os) const;
            virtual uint32_t GetSerializedSize(void) const;
            virtual void Serialize(Buffer::Iterator start) const;
            virtual uint32_t Deserialize(Buffer::Iterator start);
        };


        //        0                   1                   2                   3
        //        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
        //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //       |  Message Type |     Vtime     |         Message Size          |
        //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //       |                      Originator Address                       |
        //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        //       |  Time To Live |    Message Sequence Number    |
        //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

        class MessageHeader : public Header {
        public:

            enum MessageType {
                HELLO_MESSAGE = 1,
            };

            MessageHeader();
            virtual ~MessageHeader();

            void SetMessageType(MessageType messageType) {
                m_messageType = messageType;
            }

            MessageType GetMessageType() const {
                return m_messageType;
            }

            void SetVTime(Time time) {
                m_vTime = SecondsToEmf(time.GetSeconds());
            }

            Time GetVTime() const {
                return Seconds(EmfToSeconds(m_vTime));
            }

            void SetOriginatorAddress(Ipv4Address originatorAddress) {
                m_originatorAddress = originatorAddress;
            }

            Ipv4Address GetOriginatorAddress() const {
                return m_originatorAddress;
            }

            void SetTimeToLive(uint8_t timeToLive) {
                m_timeToLive = timeToLive;
            }

            uint8_t GetTimeToLive() const {
                return m_timeToLive;
            }

            void SetMessageSequenceNumber(uint16_t messageSequenceNumber) {
                m_messageSequenceNumber = messageSequenceNumber;
            }

            uint16_t GetMessageSequenceNumber() const {
                return m_messageSequenceNumber;
            }

          
        private:
            MessageType m_messageType;
            uint8_t m_vTime;
            Ipv4Address m_originatorAddress;
            uint8_t m_timeToLive;           
            uint16_t m_messageSequenceNumber;
            uint16_t m_messageSize;

        public:
            static TypeId GetTypeId(void);
            virtual TypeId GetInstanceTypeId(void) const;
            virtual void Print(std::ostream &os) const;
            virtual uint32_t GetSerializedSize(void) const;
            virtual void Serialize(Buffer::Iterator start) const;
            virtual uint32_t Deserialize(Buffer::Iterator start);

           

            // 6.1.  HELLO Message Format
            //
            //        0                   1                   2                   3                   
            //        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
            //
            //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            //       |     Htime     |  Willingness  | 
            //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            //       |                              HMA1                             |                                                           |
            //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            //       |                              HMA1                             |                                                           |
            //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            //       |      HMA1     |
            //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+       
            //       |                              HMA1                             |                                                           |
            //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            //       |                              HMA2                             |
            //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            //       |       HMA2    |
            //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            //       |                              HMA1                             |                                                           |
            //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            //       |                              HMA3                             |
            //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            //       |       HMA3    |
            //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            //       :                             .  .  .                           :
            //       :                                                               :
            //      
            //       
            //       
            //       
            //       
            //       
            //       
            //                                                                      :
            //                                             :
            //    (etc.)

            struct Hello {

                struct Association {
                    Ipv4Address group;
                    Ipv4Address source;
                    uint8_t willGroupSSM;
                };

                uint8_t hTime;

                void SetHTime(Time time) {
                    this->hTime = SecondsToEmf(time.GetSeconds());
                }

                Time GetHTime() const {
                    return Seconds(EmfToSeconds(this->hTime));
                }

                uint8_t willingness;
                std::vector<Association> associations;

                void Print(std::ostream &os) const;
                uint32_t GetSerializedSize(void) const;
                void Serialize(Buffer::Iterator start) const;
                uint32_t Deserialize(Buffer::Iterator start, uint32_t messageSize);
            };





            // 12.1.  HMA Message Format
            //
            //    The proposed format of an HMA-message is:
            //
            //        0                   1                   2                   3
            //        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
            //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            //       |                         Group Address                         |
            //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            //       |                         Source Address                        |
            //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            //       |       will     |
            //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            //       |                         Group Address                         |
            //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            //       |                         Source Address                        |
            //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            //       |       will     |
            //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            //       |                              ...                              |
            //       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

            // Note: HMA stands for Host multicast Association



        private:

            struct {
                Hello hello;

            } m_message; // union not allowed

        public:

            Hello& GetHello() {
                if (m_messageType == 0) {
                    m_messageType = HELLO_MESSAGE;
                } else {
                    NS_ASSERT(m_messageType == HELLO_MESSAGE);
                }
                return m_message.hello;
            }

            const Hello& GetHello() const {
                NS_ASSERT(m_messageType == HELLO_MESSAGE);
                return m_message.hello;
            }




        };

        static inline std::ostream& operator<<(std::ostream& os, const PacketHeader & packet) {
            packet.Print(os);
            return os;
        }

        static inline std::ostream& operator<<(std::ostream& os, const MessageHeader & message) {
            message.Print(os);
            return os;
        }

        typedef std::vector<MessageHeader> MessageList;
       
        static inline std::ostream& operator<<(std::ostream& os, const MessageList & messages) {
            os << "[";
            for (std::vector<MessageHeader>::const_iterator messageIter = messages.begin();
                    messageIter != messages.end(); messageIter++) {
                messageIter->Print(os);
                if (messageIter + 1 != messages.end())
                    os << ", ";
    }
            os << "]";
            return os;
}
        
    }
}
#endif	/* AIMF_HEADER_H */

