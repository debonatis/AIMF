#include "aimf-state.h"
#include "ns3/aimf-state.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"


namespace ns3 {
    namespace aimf {

        void
        AimfState::InsertAssociationTuple(const AssociationTuple &tuple) {
            m_associationSet.push_back(tuple);
        }

        Time* AimfState::FindTimer(Ipv4Address const &mainAddr) {
            for (TimerMap::iterator it = m_timerMap.begin(); it != m_timerMap.end(); it++) {
                if (it->first == mainAddr) {
                    return &(it->second);
                }
            }
            return NULL;
        }

        void AimfState::ClearTimer() {
            m_timerMap.clear();
        }

        void AimfState::AddTimer(Ipv4Address const &mainAddr, Time &time) {
            m_timerMap.insert(std::make_pair<Ipv4Address, Time>(mainAddr, time));
        }

        void AimfState::EraseTimer(Ipv4Address const &mainAddr) {
            m_timerMap.erase(mainAddr);
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
                if (it->neighborMainAddr == mainAddr)
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
        AimfState::WillingnessOk(uint8_t const will) {
            uint8_t k = 0;

            for (NeighborSet::const_iterator it = m_neighborSet.begin();
                    it != m_neighborSet.end(); it++) {

                if (it->willingness > k)
                    k = it->willingness;
            }
            return (will >= k);
        }

        int
        AimfState::WillingnessMaxInSystem() {
            uint8_t k = 0;

            for (NeighborSet::iterator it = m_neighborSet.begin();
                    it != m_neighborSet.end(); it++) {

                if (it->willingness > k)
                    k = it->willingness;
            }
            return k;
        }

        uint8_t AimfState::WillingnessNextMaxInSystem() {
            uint8_t max, secmax;
            if(m_neighborSet.size()<2){
                return ((uint8_t)WillingnessMaxInSystem());
            }

            if (m_neighborSet.at(0).willingness > m_neighborSet.at(1).willingness) {
                max = m_neighborSet.at(0).willingness;
                secmax = m_neighborSet.at(1).willingness;
            } else {
                secmax = m_neighborSet.at(0).willingness;
                max = m_neighborSet.at(1).willingness;
            }
            if(m_neighborSet.size()<3){
                return (secmax);
            }

            int i = 0;
            for (NeighborSet::iterator it = m_neighborSet.begin();
                    it != m_neighborSet.end(); it++) {

                if (i < 2) {
                    i++;
                    continue;
                }
                if (it->willingness > max) {
                    secmax = max;
                    max = it->willingness;
                } else if (it->willingness > secmax && it->willingness != max)
                    secmax = it->willingness;
            }
            return secmax;
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

    }
} // namespace aimf, ns3

