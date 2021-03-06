//
//                           SimuLTE
//
// This file is part of a software released under the license included in file
// "license.pdf". This license can be also found at http://www.ltesimulator.com/
// The above file and the present reference are part of the software itself,
// and cannot be removed from it.
//

#include "stack/rlc/um/entity/UmTxEntity.h"

Define_Module(UmTxEntity);

using namespace omnetpp;

/*
 * Main functions
 */

void UmTxEntity::initialize()
{
    sno_ = 0;
    firstIsFragment_ = false;
    notifyEmptyBuffer_ = false;
    holdingDownstreamInPackets_ = false;

    // store the node id of the owner module
    LteMacBase* mac = check_and_cast<LteMacBase*>(getParentModule()->getParentModule()->getSubmodule("mac"));
    ownerNodeId_ = mac->getMacNodeId();

    // get the reference to the RLC module
    lteRlc_ = check_and_cast<LteRlcUm*>(getParentModule()->getSubmodule("um"));
    queueSize_ = lteRlc_->par("queueSize");
    queueLength_ = 0;
}

bool UmTxEntity::enque(cPacket* pkt)
{
    EV << NOW << " UmTxEntity::enque - bufferize new SDU  " << endl;
    if(queueSize_ == 0 || queueLength_ + pkt->getByteLength() < queueSize_){
        // Buffer the SDU in the TX buffer
        sduQueue_.insert(pkt);
        queueLength_ += pkt->getByteLength();
        // Packet was successfully enqueued
        return true;
    } else {
        // Buffer is full - cannot enqueue packet
        return false;
    }
}

void UmTxEntity::rlcPduMake(int pduLength)
{
    EV << NOW << " UmTxEntity::rlcPduMake - PDU with size " << pduLength << " requested from MAC"<< endl;

    // create the RLC PDU
    LteRlcUmDataPdu* rlcPdu = new LteRlcUmDataPdu("lteRlcFragment");

    // the request from MAC takes into account also the size of the RLC header
    pduLength -= RLC_HEADER_UM;

    int len = 0;

    bool startFrag = firstIsFragment_;
    bool endFrag = false;

    while (!sduQueue_.isEmpty() && pduLength > 0)
    {
        // detach data from the SDU buffer
        cPacket* pkt = sduQueue_.front();
        LteRlcSdu* rlcSdu = check_and_cast<LteRlcSdu*>(pkt);

        unsigned int sduSequenceNumber = rlcSdu->getSnoMainPacket();
        int sduLength = rlcSdu->getByteLength();

        EV << NOW << " UmTxEntity::rlcPduMake - Next data chunk from the queue, sduSno[" << sduSequenceNumber
                << "], length[" << sduLength << "]"<< endl;

        if (pduLength >= sduLength)
        {
            EV << NOW << " UmTxEntity::rlcPduMake - Add " << sduLength << " bytes to the new SDU, sduSno[" << sduSequenceNumber << "]" << endl;

            // add the whole SDU
            pduLength -= sduLength;
            len += sduLength;

            sduQueue_.pop();
            queueLength_ -= sduLength;

            rlcPdu->pushSdu(rlcSdu);

            EV << NOW << " UmTxEntity::rlcPduMake - Pop data chunk from the queue, sduSno[" << sduSequenceNumber << "]" << endl;

            // now, the first SDU in the buffer is not a fragment
            firstIsFragment_ = false;

            EV << NOW << " UmTxEntity::rlcPduMake - The new SDU has length " << len << ", left space is " << pduLength << endl;
        }
        else
        {
            EV << NOW << " UmTxEntity::rlcPduMake - Add " << pduLength << " bytes to the new SDU, sduSno[" << sduSequenceNumber << "]" << endl;

            // add partial SDU

            len += pduLength;

            LteRlcSdu* rlcSduDup = rlcSdu->dup();
            rlcSduDup->setByteLength(pduLength);
            rlcPdu->pushSdu(rlcSduDup);

            endFrag = true;

            // update SDU in the buffer
            int newLength = sduLength - pduLength;
            pkt->setByteLength(newLength);
            queueLength_ -= pduLength;

            EV << NOW << " UmTxEntity::rlcPduMake - Data chunk in the queue is now " << newLength << " bytes, sduSno[" << sduSequenceNumber << "]" << endl;

            pduLength = 0;

            // now, the first SDU in the buffer is a fragment
            firstIsFragment_ = true;

            EV << NOW << " UmTxEntity::rlcPduMake - The new SDU has length " << len << ", left space is " << pduLength << endl;

        }
    }

    if (len == 0)
    {
        // send an empty message to notify the MAC that there is not enough space to send RLC PDU
        rlcPdu->setControlInfo(flowControlInfo_->dup());
        rlcPdu->setByteLength(len);
    }
    else
    {
        // compute FI
        // the meaning of this field is specified in 3GPP TS 36.322
        FramingInfo fi = 0;
        unsigned short int mask;
        if (endFrag)
        {
            mask = 1;   // 01
            fi |= mask;
        }
        if (startFrag)
        {
            mask = 2;   // 10
            fi |= mask;
        }

        rlcPdu->setFramingInfo(fi);
        rlcPdu->setPduSequenceNumber(sno_++);
        rlcPdu->setControlInfo(flowControlInfo_->dup());
        rlcPdu->setByteLength(RLC_HEADER_UM + len);  // add the header size
    }

    // send to MAC layer
    EV << NOW << " UmTxEntity::rlcPduMake - send PDU " << rlcPdu->getPduSequenceNumber() << " with size " << rlcPdu->getByteLength() << " bytes to lower layer" << endl;
    lteRlc_->sendToLowerLayer(rlcPdu);

    // if incoming connection was halted
    if (notifyEmptyBuffer_ && sduQueue_.isEmpty())
    {
        notifyEmptyBuffer_ = false;

        // tell the RLC UM to resume packets for the new mode
        lteRlc_->resumeDownstreamInPackets(flowControlInfo_->getD2dRxPeerId());
    }
}

void UmTxEntity::removeDataFromQueue()
{
    EV << NOW << " UmTxEntity::removeDataFromQueue - removed SDU " << endl;

    // get the last packet...
    cPacket* pkt = sduQueue_.back();

    // ...and remove it
    cPacket* retPkt = sduQueue_.remove(pkt);
    queueLength_ -= retPkt->getByteLength();
    delete retPkt;
}

void UmTxEntity::clearQueue()
{
    // empty buffer
    while (!sduQueue_.isEmpty())
        delete sduQueue_.pop();

    queueLength_ = 0;

    // reset variables except for sequence number
    firstIsFragment_ = false;
}

bool UmTxEntity::isHoldingDownstreamInPackets()
{
    return holdingDownstreamInPackets_;
}

void UmTxEntity::enqueHoldingPackets(cPacket* pkt)
{
    EV << NOW << " UmTxEntity::enqueHoldingPackets - storing new SDU into the holding buffer " << endl;
    sduHoldingQueue_.insert(pkt);
}


void UmTxEntity::resumeDownstreamInPackets()
{
    EV << NOW << " UmTxEntity::resumeDownstreamInPackets - resume buffering incoming downstream packets of the RLC entity associated to the new mode" << endl;

    holdingDownstreamInPackets_ = false;

    // move all SDUs in the holding buffer to the TX buffer
    while (!sduHoldingQueue_.isEmpty())
    {
        LteRlcSdu* rlcPkt = check_and_cast<LteRlcSdu*>(sduHoldingQueue_.front());
        sduHoldingQueue_.pop();

        // store the SDU in the TX buffer
        if(enque(rlcPkt)){
            // create a message so as to notify the MAC layer that the queue contains new data
            LteRlcPdu* newDataPkt = new LteRlcPdu("newDataPkt");
            // make a copy of the RLC SDU
            LteRlcSdu* rlcPktDup = rlcPkt->dup();
            FlowControlInfo* lteInfo = check_and_cast<FlowControlInfo*>(rlcPkt->getControlInfo());
            // the MAC will only be interested in the size of this packet
            newDataPkt->encapsulate(rlcPktDup);
            newDataPkt->setControlInfo(lteInfo->dup());

            lteRlc_->sendToLowerLayer(newDataPkt);
        } else {
            // Queue is full - drop SDU
            EV << "UmTxEntity::resumeDownstreamInPackets - cannot buffer SDU (queue is full), dropping" << std::endl;
            lteRlc_->dropBufferOverflow(rlcPkt);
        }
    }
}

void UmTxEntity::rlcHandleD2DModeSwitch(bool oldConnection, bool clearBuffer)
{
    if (oldConnection)
    {
        if (getNodeTypeById(ownerNodeId_) == ENODEB)
        {
            EV << NOW << " UmRxEntity::rlcHandleD2DModeSwitch - nothing to do on DL leg of IM flow" << endl;
            return;
        }

        if (clearBuffer)
        {
            EV << NOW << " UmTxEntity::rlcHandleD2DModeSwitch - clear TX buffer of the RLC entity associated to the old mode" << endl;
            clearQueue();
        }
        else
        {
            if (!sduQueue_.isEmpty())
            {
                EV << NOW << " UmTxEntity::rlcHandleD2DModeSwitch - check when TX buffer the RLC entity associated to the old mode becomes empty - queue length[" << sduQueue_.getLength() << "]" << endl;
                notifyEmptyBuffer_ = true;
            }
            else
            {
                EV << NOW << " UmTxEntity::rlcHandleD2DModeSwitch - TX buffer of the RLC entity associated to the old mode is already empty" << endl;
            }
        }
    }
    else
    {
        EV << " UmTxEntity::rlcHandleD2DModeSwitch - reset numbering of the RLC TX entity corresponding to the new mode" << endl;
        sno_ = 0;

        if (!clearBuffer)
        {
            if (lteRlc_->isEmptyingTxBuffer(flowControlInfo_->getD2dRxPeerId()))
            {
                // stop incoming connections, until
                EV << NOW << " UmTxEntity::rlcHandleD2DModeSwitch - halt incoming downstream connections of the RLC entity associated to the new mode" << endl;
                startHoldingDownstreamInPackets();
            }
        }
    }
}
