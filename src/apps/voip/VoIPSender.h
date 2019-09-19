//
//                           SimuLTE
//
// This file is part of a software released under the license included in file
// "license.pdf". This license can be also found at http://www.ltesimulator.com/
// The above file and the present reference are part of the software itself,
// and cannot be removed from it.
//

#ifndef _LTE_VOIPSENDER_H_
#define _LTE_VOIPSENDER_H_

#include <string.h>
#include <omnetpp.h>

#include <inet/networklayer/common/L3AddressResolver.h>
#include <inet4_compat/transportlayer/contract/udp/UdpSocket.h>
#include "apps/voip/VoipPacket_m.h"

class VoIPSender : public omnetpp::cSimpleModule
{
    inet::UdpSocket socket;

    //source
    omnetpp::simtime_t durTalk_;
    omnetpp::simtime_t durSil_;
    double scaleTalk_;
    double shapeTalk_;
    double scaleSil_;
    double shapeSil_;
    bool isTalk_;
    omnetpp::cMessage* selfSource_;
    //sender
    int iDtalk_;
    int nframes_;
    int iDframe_;
    int nframesTmp_;
    int size_;
    omnetpp::simtime_t sampling_time;

    bool silences_;


    // omnetpp::simsignal_t voIPGeneratedThroughtput_;
    // ----------------------------

    omnetpp::cMessage *selfSender_;

    omnetpp::cMessage *initTraffic_;

    omnetpp::simtime_t timestamp_;
    int localPort_;
    int destPort_;
    inet::L3Address destAddress_;

    void initTraffic();
    void talkspurt(omnetpp::simtime_t dur);
    void selectPeriodTime();
    void sendVoIPPacket();

  public:
    ~VoIPSender();
    VoIPSender();

  protected:

    virtual int numInitStages() const  override { return inet::NUM_INIT_STAGES; }
    void initialize(int stage) override;
    void handleMessage(omnetpp::cMessage *msg) override;

};

#endif

