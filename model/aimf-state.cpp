#include "aimf-state.h"
#include "ns3/aimf-state.h"


namespace ns3 {
    namespace aimf {

        void
        AimfState::InsertAssociationTuple(const AssociationTuple &tuple) {
            m_associationSet.push_back(tuple);
        }

        /********** Neighbor Set Manipulation **********/

        NeighborTuple*
        AimfState::FindNeighborTuple(Ipv4Address const &mainAddr) {
            for (NeighborSet::iterator it = m_neighborSet.begin();
                    it != m_neighborSet.end(); it++) {
                if (it->neighborMainAddr == mainAddr)
                    return &(*it);
            }
            return NULL;
        }

        const NeighborTuple*
        AimfState::FindSymNeighborTuple(Ipv4Address const &mainAddr) const {
            for (NeighborSet::const_iterator it = m_neighborSet.begin();
                    it != m_neighborSet.end(); it++) {
                if (it->neighborMainAddr == mainAddr && it->status == NeighborTuple::STATUS_SYM)
                    return &(*it);
            }
            return NULL;
        }

        NeighborTuple*
        AimfState::FindNeighborTuple(Ipv4Address const &mainAddr, uint8_t willingness) {
            for (NeighborSet::iterator it = m_neighborSet.begin();
                    it != m_neighborSet.end(); it++) {
                if (it->neighborMainAddr == mainAddr && it->willingness == willingness)
                    return &(*it);
            }
            return NULL;
        }

        bool
        AimfState::WillingnessOk(uint8_t const &will) {
            uint8_t k = 0;
            for (NeighborSet::iterator it = m_neighborSet.begin();
                    it != m_neighborSet.end(); it++) {
                if (it->willingness > k)
                    k = it->willingness;
            }
            return (will > k);
        }

        void
        AimfState::EraseNeighborTuple(const NeighborTuple &tuple) {
            for (NeighborSet::iterator it = m_neighborSet.begin();
                    it != m_neighborSet.end(); it++) {
                if (*it == tuple) {
                    m_neighborSet.erase(it);
                    break;
                }
            }
        }

        void
        AimfState::EraseNeighborTuple(const Ipv4Address &mainAddr) {
            for (NeighborSet::iterator it = m_neighborSet.begin();
                    it != m_neighborSet.end(); it++) {
                if (it->neighborMainAddr == mainAddr) {
                    it = m_neighborSet.erase(it);
                    break;
                }
            }
        }

        void
        AimfState::InsertNeighborTuple(NeighborTuple const &tuple) {
            for (NeighborSet::iterator it = m_neighborSet.begin();
                    it != m_neighborSet.end(); it++) {
                if (it->neighborMainAddr == tuple.neighborMainAddr) {
                    // Update it
                    *it = tuple;
                    return;
                }
            }
            m_neighborSet.push_back(tuple);
        }

        /********** Host-Multicast Association Set Manipulation **********/

        IfaceAssocTuple* AimfState::FindIfaceAssocTuple(const Ipv4Address& ifaceAddr) {
            for (IfaceAssocSet::iterator it = m_ifaceAssocSet.begin();
                    it != m_ifaceAssocSet.end(); it++) {
                if (it->ifaceAddr == ifaceAddr)
                    return &(*it);
            }
            return NULL;
        }

        const IfaceAssocTuple*
        AimfState::FindIfaceAssocTuple(const Ipv4Address& ifaceAddr) const {
            for (IfaceAssocSet::const_iterator it = m_ifaceAssocSet.begin();
                    it != m_ifaceAssocSet.end(); it++) {
                if (it->ifaceAddr == ifaceAddr)
                    return &(*it);
            }
            return NULL;
        }

        AssociationTuple*
        AimfState::FindAssociationTuple(const Ipv4Address &advertiser, const Ipv4Address &group, const Ipv4Address &source) {
            for (AssociationSet::iterator it = m_associationSet.begin();
                    it != m_associationSet.end(); it++) {
                if (it->advertiser == advertiser and it->group == group and it->source == source) {
                    return &(*it);
                }
            }
            return NULL;
        }

        void
        AimfState::EraseAssociationTuple(const AssociationTuple &tuple) {
            for (AssociationSet::iterator it = m_associationSet.begin();
                    it != m_associationSet.end(); it++) {
                if (*it == tuple) {
                    m_associationSet.erase(it);
                    break;
                }
            }
        }

        void
        AimfState::EraseAssociation(const Association &tuple) {
            for (Associations::iterator it = m_associations.begin();
                    it != m_associations.end(); it++) {
                if (*it == tuple) {
                    m_associations.erase(it);
                    break;
                }
            }
        }

        void
        AimfState::InsertAssociation(const Association &tuple) {
            m_associations.push_back(tuple);
        }

        void
        AimfState::InsertIfaceAssocTuple(const IfaceAssocTuple &tuple) {
            m_ifaceAssocSet.push_back(tuple);
        }

    }
} // namespace aimf, ns3

