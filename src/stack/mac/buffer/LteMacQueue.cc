//
//                           SimuLTE
//
// This file is part of a software released under the license included in file
// "license.pdf". This license can be also found at http://www.ltesimulator.com/
// The above file and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include <climits>
#include "stack/mac/buffer/LteMacQueue.h"
#include "stack/rlc/am/packet/LteRlcAmPdu.h"

using namespace omnetpp;

LteMacQueue::LteMacQueue(int queueSize) :
    cPacketQueue("LteMacQueue")
{
    queueSize_ = queueSize;
    lastUnenqueueableMainSno = UINT_MAX;
}

LteMacQueue::LteMacQueue(const LteMacQueue& queue)
{
    operator=(queue);
}

LteMacQueue& LteMacQueue::operator=(const LteMacQueue& queue)
{
    cPacketQueue::operator=(queue);
    queueSize_ = queue.queueSize_;
    return *this;
}

LteMacQueue* LteMacQueue::dup() const
{
    return new LteMacQueue(*this);
}

// ENQUEUE
bool LteMacQueue::pushBack(cPacket *pkt)
{
    if (!isEnqueueablePacket(pkt))
         return false; // packet queue full or we have discarded fragments for this main packet

    cPacketQueue::insert(pkt);
    return true;
}

bool LteMacQueue::pushFront(cPacket *pkt)
{
    if (!isEnqueueablePacket(pkt))
        return false; // packet queue full or we have discarded fragments for this main packet

    cPacketQueue::insertBefore(cPacketQueue::front(), pkt);
    return true;
}

cPacket* LteMacQueue::popFront()
{
    return getQueueLength() > 0 ? cPacketQueue::pop() : NULL;
}

cPacket* LteMacQueue::popBack()
{
    return getQueueLength() > 0 ? cPacketQueue::remove(cPacketQueue::back()) : NULL;
}

simtime_t LteMacQueue::getHolTimestamp() const
{
    return getQueueLength() > 0 ? cPacketQueue::front()->getTimestamp() : 0;
}

int64_t LteMacQueue::getQueueOccupancy() const
{
    return cPacketQueue::getByteLength();
}

int64_t LteMacQueue::getQueueSize() const
{
    return queueSize_;
}

bool LteMacQueue::isEnqueueablePacket(cPacket* pkt){
    if(queueSize_ == 0){
        // unlimited queue size -- nothing to check for
        return true;
    }
    /* Check:
     *
     * For AM: We need to check if all fragments will fit in the queue
     * For UM: The new UM implementation introduced in commit 9ab9b71c5358a70278e2fbd51bf33a9d1d81cb86
     *         by G. Nardini only sends one SDU at a time upon MAC SDU request,
     *         therefore no check needs to be done here
     * For TM: No fragments are to be checked, anyways.
     */
    LteRlcAmPdu_Base* pdu = dynamic_cast<LteRlcAmPdu*>(pkt);

    if(pdu != NULL){ // for AM we need to check if all fragments will fit
        if(pdu->getTotalFragments() > 1) {
            int remainingFrags = (pdu->getLastSn() - pdu->getSnoFragment() + 1);
            bool allFragsWillFit = (remainingFrags*pdu->getByteLength()) + getByteLength() < queueSize_;
            bool enqueable = (pdu->getSnoMainPacket() != lastUnenqueueableMainSno) && allFragsWillFit;
            if(allFragsWillFit && !enqueable){
                EV_DEBUG << "PDU would fit but discarded frags before - rejecting fragment: " << pdu->getSnoMainPacket() << ":" << pdu->getSnoFragment() << std::endl;
            }
            if(!enqueable){
                lastUnenqueueableMainSno = pdu->getSnoMainPacket();
            }
            return enqueable;
        }
    }

    // no fragments or unknown type -- can always be enqueued if there is enough space in the queue
    return (pkt->getByteLength() + getByteLength() < queueSize_);
}

int LteMacQueue::getQueueLength() const
{
    return cPacketQueue::getLength();
}

std::ostream &operator << (std::ostream &stream, const LteMacQueue* queue)
{
    stream << "LteMacQueue-> Length: " << queue->getQueueLength() <<
        " Occupancy: " << queue->getQueueOccupancy() <<
        " HolTimestamp: " << queue->getHolTimestamp() <<
        " Size: " << queue->getQueueSize();
    return stream;
}
