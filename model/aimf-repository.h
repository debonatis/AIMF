/* 
 * File:   aIMFrepository.h
 * Author: debonatis
 *
 * Created on 19 February 2016, 16:54
 */

#ifndef AIMFREPOSITORY_H
#define	AIMFREPOSITORY_H


#include <set>
#include <vector>

#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"

namespace ns3 {
    namespace aimf {

        struct IfaceAssocTuple {
            /// Interface address of a node.
            Ipv4Address ifaceAddr;
            /// Main address of the node.
            Ipv4Address mainAddr;
            /// Time at which this tuple expires and must be removed.
            Time time;
        };

        static inline bool
        operator==(const IfaceAssocTuple &a, const IfaceAssocTuple &b) {
            return (a.ifaceAddr == b.ifaceAddr
                    && a.mainAddr == b.mainAddr);
        }

        static inline std::ostream&
        operator<<(std::ostream &os, const IfaceAssocTuple &tuple) {
            os << "IfaceAssocTuple(ifaceAddr=" << tuple.ifaceAddr
                    << ", mainAddr=" << tuple.mainAddr
                    << ", time=" << tuple.time << ")";
            return os;
        }

        struct NeighborTuple {
            /// Main address of a neighbor node.
            Ipv4Address neighborMainAddr;
            /// Neighbor Type and Link Type at the four less significative digits.
            std::pair<Ipv4Address, Ipv4Address> groupToSource;

            enum Status {
                STATUS_NOT_SYM = 0, // "not symmetric"
                STATUS_SYM = 1, // "symmetric"
            } status;
            /// A value between 0 and 7 specifying the node's willingness to carry traffic on behalf of other nodes.
            uint8_t willingness;
        };

        static inline bool
        operator==(const NeighborTuple &a, const NeighborTuple &b) {
            return (a.neighborMainAddr == b.neighborMainAddr
                    && a.status == b.status
                    && a.willingness == b.willingness);
        }

        static inline std::ostream&
        operator<<(std::ostream &os, const NeighborTuple &tuple) {
            os << "NeighborTuple(neighborMainAddr=" << tuple.neighborMainAddr
                    << ", status=" << (tuple.status == NeighborTuple::STATUS_SYM ? "SYM" : "NOT_SYM")
                    << ", willingness=" << (int) tuple.willingness << ")";
            return os;
        }




        /// Association

        struct Association {
            Ipv4Address group;
            Ipv4Address source;
        };

        static inline bool
        operator==(const Association &a, const Association &b) {
            return (a.group == b.group
                    && a.source == b.source);
        }

        static inline std::ostream&
        operator<<(std::ostream &os, const Association &tuple) {
            os << "Association(networkAddr=" << tuple.group
                    << ", netmask=" << tuple.source
                    << ")";
            return os;
        }

        /// An Association Tuple

        struct AssociationTuple {
            /// Main address of the gateway.
            Ipv4Address advertiser;

            Ipv4Address group;
            /// Network Address of network reachable through gatewayAddr
            Ipv4Address source;
            /// Time at which this tuple expires and must be removed
            Time expirationTime;
        };

        static inline bool
        operator==(const AssociationTuple &a, const AssociationTuple &b) {
            return (a.group == b.group
                    && a.source == b.source
                    && a.advertiser == b.advertiser);
        }

        static inline std::ostream&
        operator<<(std::ostream &os, const AssociationTuple &tuple) {
            os << "AssociationTuple(group=" << tuple.group
                    << ", source=" << tuple.source
                    << ", advertiser=" << tuple.advertiser
                    << ", expirationTime=" << tuple.expirationTime
                    << ")";
            return os;
        }



        typedef std::vector<NeighborTuple> NeighborSet; ///< Neighbor Set type.

        typedef std::vector<IfaceAssocTuple> IfaceAssocSet; ///< Interface Association Set type.
        typedef std::vector<AssociationTuple> AssociationSet; ///< Association Set type.
        typedef std::vector<Association> Associations; ///< Association Set type.


    }
} // namespace ns3, olsr

#endif	/* AIMFREPOSITORY_H */

