#include "network-scheduler.h"

namespace ns3
{
namespace lorawan
{

NS_LOG_COMPONENT_DEFINE("NetworkScheduler");

NS_OBJECT_ENSURE_REGISTERED(NetworkScheduler);

TypeId
NetworkScheduler::GetTypeId(void)
{
    static TypeId tid =
        TypeId("ns3::NetworkScheduler")
            .SetParent<Object>()
            .AddConstructor<NetworkScheduler>()
            .AddTraceSource("ReceiveWindowOpened",
                            "Trace source that is fired when a receive window opportunity happens.",
                            MakeTraceSourceAccessor(&NetworkScheduler::m_receiveWindowOpened),
                            "ns3::Packet::TracedCallback")
            .SetGroupName("lorawan");
    return tid;
}

NetworkScheduler::NetworkScheduler()
{
}

NetworkScheduler::NetworkScheduler(Ptr<NetworkStatus> status, Ptr<NetworkController> controller)
    : m_status(status),
      m_controller(controller),
      m_bcnCounter(0),
      m_downCounter(0)
{
    m_queue = CreateObject<DropTailQueue<Packet>>();
}

NetworkScheduler::~NetworkScheduler()
{
}

void
NetworkScheduler::OnReceivedPacket(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(packet);

    // Get the current packet's frame counter
    Ptr<Packet> packetCopy = packet->Copy();
    LorawanMacHeader receivedMacHdr;
    packetCopy->RemoveHeader(receivedMacHdr);
    LoraFrameHeader receivedFrameHdr;
    receivedFrameHdr.SetAsUplink();
    packetCopy->RemoveHeader(receivedFrameHdr);

    // Need to decide whether to schedule a receive window
    if (!m_status->GetEndDeviceStatus(packet)->HasReceiveWindowOpportunityScheduled())
    {
        // Extract the address
        LoraDeviceAddress deviceAddress = receivedFrameHdr.GetAddress();

        // Schedule OnReceiveWindowOpportunity event
        m_status->GetEndDeviceStatus(packet)->SetReceiveWindowOpportunity(
            Simulator::Schedule(Seconds(1),
                                &NetworkScheduler::OnReceiveWindowOpportunity,
                                this,
                                deviceAddress,
                                1)); // This will be the first receive window
    }
}

void
NetworkScheduler::OnReceiveWindowOpportunity(LoraDeviceAddress deviceAddress, int window)
{
    NS_LOG_FUNCTION(deviceAddress);

    NS_LOG_DEBUG("Opening receive window number " << window << " for device " << deviceAddress);

    // Check whether we can send a reply to the device, again by using
    // NetworkStatus
    Address gwAddress = m_status->GetBestGatewayForDevice(deviceAddress, window);

    if (gwAddress == Address() && window == 1)
    {
        NS_LOG_DEBUG("No suitable gateway found for first window.");

        // No suitable GW was found, but there's still hope to find one for the
        // second window.
        // Schedule another OnReceiveWindowOpportunity event
        m_status->GetEndDeviceStatus(deviceAddress)
            ->SetReceiveWindowOpportunity(
                Simulator::Schedule(Seconds(1),
                                    &NetworkScheduler::OnReceiveWindowOpportunity,
                                    this,
                                    deviceAddress,
                                    2)); // This will be the second receive window
    }
    else if (gwAddress == Address() && window == 2)
    {
        // No suitable GW was found and this was our last opportunity
        // Simply give up.
        NS_LOG_DEBUG("Giving up on reply: no suitable gateway was found "
                     << "on the second receive window");

        // Reset the reply
        // XXX Should we reset it here or keep it for the next opportunity?
        m_status->GetEndDeviceStatus(deviceAddress)->RemoveReceiveWindowOpportunity();
        m_status->GetEndDeviceStatus(deviceAddress)->InitializeReply();
    }
    else
    {
        // A gateway was found

        NS_LOG_DEBUG("Found available gateway with address: " << gwAddress);

        // This funciton is used to check if the gw needs to send a reply. For example,
        // checking at the mac commands from the uplink message.
        //  we don't need to do anything before sending a reply, so we comment this.
        m_controller->BeforeSendingReply(m_status->GetEndDeviceStatus(deviceAddress));
        // m_status->GetEndDeviceStatus(deviceAddress)->m_reply.needsReply = true;

        // Check whether this device needs a response by querying m_status
        bool needsReply = m_status->NeedsReply(deviceAddress);

        if (needsReply)
        {
            NS_LOG_INFO("A reply is needed");

            // Send the reply through that gateway
            m_status->SendThroughGateway(m_status->GetReplyForDevice(deviceAddress, window),
                                         gwAddress);

            // Reset the reply
            m_status->GetEndDeviceStatus(deviceAddress)->RemoveReceiveWindowOpportunity();
            m_status->GetEndDeviceStatus(deviceAddress)->InitializeReply();
        }
        else {
            NS_LOG_INFO("No reply is needed");
        }
    }
}

void 
NetworkScheduler::SetOffsetData(Ptr<StructTest> data)
{
    m_offsetData = data;
}

Ptr<StructTest>
NetworkScheduler::GetOffsetData()
{
    return m_offsetData;
}

void
NetworkScheduler::OnReceiveWindowOpportunityBcn(LoraDeviceAddress deviceAddress, int window)
{
    NS_LOG_FUNCTION(deviceAddress);
    LoraTag tag;
    // LoraTag tag2;

    NS_LOG_DEBUG("Opening beacon window for device " << deviceAddress);

    if (window != 3)
    {
        NS_LOG_ERROR("Bad window number for beacon Tx.");
    }

    // Check whether we can send a reply to the device, again by using
    // NetworkStatus
    Address gwAddress = m_status->GetBestGatewayForDevice(deviceAddress, window);

    if (gwAddress == Address())
    {
        NS_LOG_DEBUG("No gateway available.");
        return;
    }

    // A gateway was found
    NS_LOG_DEBUG("Found available gateway with address: " << gwAddress);


    // Create a beacon and send it to the end device
    Ptr<Packet> pkt = Create<Packet>(25);
    pkt->RemovePacketTag(tag);
    tag.SetFrequency(m_offsetData->GetBeaconFreq());
    tag.SetDataRate(3);
    pkt->AddPacketTag(tag);

    LoraFrameHeader frameHdr;
    frameHdr.SetAsDownlink();
    // frameHdr.SetAddress();
    frameHdr.SetFCnt(m_bcnCounter++); //add a counter to the num of bcns (there should be an attribute to count the number of beacons, and also the number of data DL packets)
    NS_LOG_DEBUG("aaaaaaaaaaaaaaaaaaaaaaaaaaaaa new m_bcnCounter value: "<< m_bcnCounter);
    // ApplyNecessaryOptions(frameHdr);
    pkt->AddHeader(frameHdr);

    LorawanMacHeader macHdr;
    // ApplyNecessaryOptions(macHdr);
    macHdr.SetMType(LorawanMacHeader::BEACON); // set as bcon
    pkt->AddHeader(macHdr);

    NS_LOG_DEBUG("aaaaaaaaaaaaaaaaaaaaaaaaaaaaa Pkt size (BCN): "<< pkt->GetSize());

    // Send the reply through that gateway
    m_status->SendThroughGateway(pkt, gwAddress);
}

void
NetworkScheduler::OnReceiveWindowOpportunityPingSlot(LoraDeviceAddress deviceAddress, int window)
{
    NS_LOG_FUNCTION(deviceAddress);
    LoraTag tag;
    NS_LOG_DEBUG("Opening receive window in a ping slot for device: "<< deviceAddress);

    if (window != 4)
    {
        NS_LOG_ERROR("Bad window number for PingSlot Tx.");
    }

    // Check whether we can send a reply to the device, again by using
    // NetworkStatus
    Address gwAddress = m_status->GetBestGatewayForDevice(deviceAddress, window);

    if (gwAddress == Address())
    {
        NS_LOG_DEBUG("No gateway available.");

        // No suitable GW was found, but there's still hope to find one for the
        // second window.
        // Schedule another OnReceiveWindowOpportunity event
        // m_status->GetEndDeviceStatus(deviceAddress)
        //     ->SetReceiveWindowOpportunity(
        //         Simulator::Schedule(Seconds(1),
        //                             &NetworkScheduler::OnReceiveWindowOpportunity,
        //                             this,
        //                             deviceAddress,
        //                             2)); // This will be the second receive window
        return;
    }

    // A gateway was found
    NS_LOG_DEBUG("Found available gateway with address: " << gwAddress);

    // Check whether this device needs a response by querying m_status
    bool queue_empty = m_queue->IsEmpty();

    if (!queue_empty)
    {
        Ptr<Packet> pkt_cpy = m_queue->Peek()->Copy();

        // Check if the device who is listening at this moment is the 
        // destination of the packet
        LoraFrameHeader fHdr;
        pkt_cpy->RemoveHeader(fHdr);

        if (deviceAddress == fHdr.GetAddress()) 
        {
            // The packet is for this device
            NS_LOG_INFO("A DL is needed");
            Ptr<Packet> pkt = m_queue->Dequeue();
            pkt->RemovePacketTag(tag);
            // NS_LOG_DEBUG("AAAAAAAAAAAAAAAAAAAAAAAAAAAAA AAAAAAAAAAAm_offsetData->m_endDevicesFreq.find(deviceAddress)->second" << m_offsetData->m_endDevicesFreq.find(deviceAddress)->second);
            tag.SetFrequency(m_offsetData->m_endDevicesFreq.find(deviceAddress)->second);
            tag.SetDataRate(3);
            tag.SetTimeRemovedQueue(Simulator::Now());
            pkt->AddPacketTag(tag);

            fHdr.SetAsDownlink();
            fHdr.SetAddress(deviceAddress); //add address (gw or end device?)
            fHdr.SetFCnt(m_downCounter++); //add a counter to the num of bcns (there should be an attribute to count the number of beacons, and also the number of data DL packets)
            NS_LOG_DEBUG("aaaaaaaaaaaaaaaaaaaaaaaaaaaaa new m_downCounter value: "<< unsigned(m_downCounter));
            pkt->AddHeader(fHdr);

            LorawanMacHeader macHdr;
            macHdr.SetMType(LorawanMacHeader::UNCONFIRMED_DATA_DOWN); // set as data downlink
            pkt->AddHeader(macHdr);

            NS_LOG_DEBUG("aaaaaaaaaaaaaaaaaaaaaaaaaaaaa Pkt size (DL): "<< pkt->GetSize());
            
            // Send the packet through that gateway
            m_status->SendThroughGateway(pkt, gwAddress);

        }
        else 
        {
            // The packet is not for this device
            NS_LOG_INFO("The packet is not for this device");
        }
    }
    else {
        NS_LOG_INFO("No DL is needed");
    }
    
}

void 
NetworkScheduler::Send2Queue(Ptr<Packet> pkt)
{
    m_queue->Enqueue(pkt);
}

uint16_t
NetworkScheduler::GetDownCounter()
{
    return m_downCounter;
}
void
NetworkScheduler::IncrementDownCounter()
{
    m_downCounter++;
}

} // namespace lorawan
} // namespace ns3
