/* 
 * File:   aimf-state.h
 * Author: debonatis
 *
 * Created on 24 February 2016, 15:04
 */

#ifndef AIMF_STATE_H
#define	AIMF_STATE_H

#include "aimf-repository.h"

namespace ns3 {
    namespace aimf {

        /// This class encapsulates all data structures needed for maintaining internal state of an AIMF node.

        class AimfState {
        protected:

            NeighborSet m_neighborSet; 
            TimerMap m_timerMap;  
            IfaceAssocSet m_ifaceAssocSet; 
            AssociationSet m_associationSet; 
            Associations m_associations; 
            UniqnessTable m_unikTable;
        public:

            AimfState(){
                
            };
            
            //Partition handling

          
            // Neighbor

            const NeighborSet & GetNeighbors() const {
                return m_neighborSet;
            }

            NeighborSet & GetNeighbors() {
                return m_neighborSet;
            }
            
            UniqnessTable & GetUnikTable() {
                return m_unikTable;
            }
            
            
            uint8_t FindUnik(uint8_t t);
            Time* FindTimer(Ipv4Address const &mainAddr);
            void AddTimer(Ipv4Address const &mainAddr, Time &time);
            void EraseTimer(Ipv4Address const &mainAddr);
            void ClearTimer();
                
            
            NeighborTuple* FindNeighborTuple(const Ipv4Address &mainAddr);
            const NeighborTuple* FindSymNeighborTuple(const Ipv4Address &mainAddr) const;
            NeighborTuple* FindNeighborTuple(const Ipv4Address &mainAddr,
                    uint8_t willingness);
            void EraseNeighborTuple(const NeighborTuple &neighborTuple);
            void EraseNeighborTuple(const Ipv4Address &mainAddr);
            void InsertNeighborTuple(const NeighborTuple &tuple);
            int WillingnessOk(uint8_t const will);
            int
            WillingnessMaxInSystem();
            uint8_t WillingnessNextMaxInSystem();




            // Host Multicast Association

            const AssociationSet & GetAssociationSet() const // Associations known to the node
            {
                return m_associationSet;
            }

            const Associations & GetAssociations() const // Set of associations that the node has
            {
                return m_associations;
            }



            AssociationTuple* FindAssociationTuple(const Ipv4Address &advertiser, \
                                          const Ipv4Address &group, \
                                          const Ipv4Address &source);
            void EraseAssociationTuple(const AssociationTuple &tuple);
            void InsertAssociationTuple(const AssociationTuple &tuple);
            void EraseAssociation(const Association &tuple);
            void InsertAssociation(const Association &tuple);


        };

    }
} // namespace aimf,ns3


#endif	/* AIMF_STATE_H */

