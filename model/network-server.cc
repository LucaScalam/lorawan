/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2018 University of Padova
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Davide Magrin <magrinda@dei.unipd.it>
 *          Martina Capuzzo <capuzzom@dei.unipd.it>
 */

#include "network-server.h"

#include "class-a-end-device-lorawan-mac.h"
#include "lora-device-address.h"
#include "lora-frame-header.h"
#include "lorawan-mac-header.h"
#include "mac-command.h"
#include "network-status.h"

#include "ns3/net-device.h"
#include "ns3/node-container.h"
#include "ns3/packet.h"
#include "ns3/point-to-point-net-device.h"

namespace ns3
{
namespace lorawan
{

NS_LOG_COMPONENT_DEFINE("NetworkServer");

NS_OBJECT_ENSURE_REGISTERED(NetworkServer);

TypeId
NetworkServer::GetTypeId(void)
{
    static TypeId tid =
        TypeId("ns3::NetworkServer")
            .SetParent<Application>()
            .AddConstructor<NetworkServer>()
            .AddTraceSource(
                "ReceivedPacket",
                "Trace source that is fired when a packet arrives at the Network Server",
                MakeTraceSourceAccessor(&NetworkServer::m_receivedPacket),
                "ns3::Packet::TracedCallback")
            .SetGroupName("lorawan");
    return tid;
}

NetworkServer::NetworkServer()
    : m_status(Create<NetworkStatus>()),
      m_controller(Create<NetworkController>(m_status)),
      m_scheduler(Create<NetworkScheduler>(m_status, m_controller)),
      m_slotDuration(Seconds(.03)),
      m_kParameter(4),
      m_beaconPeriod(Seconds(128)),
      m_beaconReserved(Seconds(2.12)),
      m_lambda_inv(32)
{
    NS_LOG_FUNCTION_NOARGS();
    m_scheduler->SetOffsetData(Create<StructTest>());
    m_scheduler->GetOffsetData()->SetBeaconFreq(869.525);
    m_uniform_rv = CreateObject<UniformRandomVariable>();
}

NetworkServer::~NetworkServer()
{
    NS_LOG_FUNCTION_NOARGS();
}

void
NetworkServer::TrafficGeneratorManager(void)
{
    NS_LOG_FUNCTION_NOARGS();

    Ptr<ExponentialRandomVariable> ev = CreateObject<ExponentialRandomVariable>();
    // ev->SetAttribute("Mean", DoubleValue(1/m_lambda));
    ev->SetAttribute("Mean", DoubleValue(m_lambda_inv));
    ev->SetAttribute("Bound", DoubleValue(600));

    Simulator::Schedule(Seconds(ev->GetValue()), &NetworkServer::TrafficEnqueue, this, ev);

}

void
NetworkServer::TrafficEnqueue(Ptr<ExponentialRandomVariable> ev)
{
    NS_LOG_FUNCTION(this);
    // NS_LOG_FUNCTION_NOARGS();
    LoraTag tag;
    Ptr<Packet> pkt = Create<Packet>(55);
    uint16_t index_end_device = m_uniform_rv->GetValue(0, m_scheduler->GetOffsetData()->m_endDevicesFreq.size() - 1);

    auto it = m_scheduler->GetOffsetData()->m_endDevicesFreq.begin();
    std::advance(it, index_end_device);
    
    pkt->RemovePacketTag(tag);
    tag.SetPriority(0);
    tag.SetTimeAddedQueue(Simulator::Now());
    pkt->AddPacketTag(tag);

    LoraFrameHeader frameHdr;
    // frameHdr.SetAsDownlink();
    frameHdr.SetAddress(it->first); //add address (gw or end device?)
    // frameHdr.SetFCnt(m_scheduler->GetDownCounter());
    // m_scheduler->IncrementDownCounter();
    NS_LOG_DEBUG("aaaaaaaaaaaaaaaaaaaaaaaaaaaaa index_end_device: "<< index_end_device);
    NS_LOG_DEBUG("aaaaaaaaaaaaaaaaaaaaaaaaaaaaa DevAddresss: "<< it->first);
    // NS_LOG_DEBUG("aaaaaaaaaaaaaaaaaaaaaaaaaaaaa new m_downCounter value: "<< unsigned(m_scheduler->GetDownCounter()));
    pkt->AddHeader(frameHdr);

    m_scheduler->Send2Queue(pkt);
    Simulator::Schedule(Seconds(ev->GetValue()), &NetworkServer::TrafficEnqueue, this, ev);

}

void
NetworkServer::OffsetCalculation(void)
{
    // NS_LOG_FUNCTION();
    NS_LOG_FUNCTION_NOARGS();

    // m_offsetData


    // m_offset = Seconds(0.06);
}

void
NetworkServer::BeaconWindowOpen(void)
{
    // NS_LOG_FUNCTION(this);
    NS_LOG_FUNCTION_NOARGS();
    m_startBeaconPeriod = Simulator::Now();

    // Create bcn structure and send it to the children

    // Then, we could change this and send the beacon through all the gws conneted to the NS (no need
    // to send the first device)
    Simulator::Schedule(Seconds(0), &NetworkScheduler::OnReceiveWindowOpportunityBcn, m_scheduler, 
                        m_status->m_endDeviceStatuses.begin()->first, 3);
    
    for ( auto it = m_scheduler->GetOffsetData()->m_endDevicesOffset.begin(); it != m_scheduler->GetOffsetData()->m_endDevicesOffset.end(); ++it )
    {
        m_status->GetEndDeviceStatus(it->first)->SetCounterPingSlot(1 << m_kParameter);
        it->second = m_uniform_rv->GetInteger(0, (1 << (12 - m_kParameter)) - 1);
        NS_LOG_DEBUG("[key: devAdd, value: pingOffset] -> " << it->first << " , " << unsigned(it->second));
        Simulator::Schedule((it->second * m_slotDuration) + m_beaconReserved, &NetworkServer::PingSlotOpen, this, it->first);

    }

    Simulator::Schedule(m_beaconPeriod,
                        &NetworkServer::BeaconWindowOpen,
                        this); 
}

void
NetworkServer::PingSlotOpen(LoraDeviceAddress devAddress)
{
    // NS_LOG_FUNCTION(this);
    NS_LOG_FUNCTION_NOARGS();

    Simulator::Schedule(Seconds(0), &NetworkScheduler::OnReceiveWindowOpportunityPingSlot, m_scheduler, 
                        devAddress, 4);

    uint8_t counterPingSlot = m_status->GetEndDeviceStatus(devAddress)->GetCounterPingSlot();
    m_status->GetEndDeviceStatus(devAddress)->SetCounterPingSlot(counterPingSlot - 1);
    NS_LOG_DEBUG("For device -> "<< devAddress << ", counterPingSlot left -> " << unsigned(m_status->GetEndDeviceStatus(devAddress)->GetCounterPingSlot()));

    if (m_status->GetEndDeviceStatus(devAddress)->GetCounterPingSlot() != 0) {
        Simulator::Schedule(m_slotDuration * (1 << (12 - m_kParameter)),
                            &NetworkServer::PingSlotOpen,
                            this, devAddress); 
    }
}

void
NetworkServer::StartApplication(void)
{
    NS_LOG_FUNCTION(this);
   
    for ( auto it = m_scheduler->GetOffsetData()->m_endDevicesFreq.begin(); it != m_scheduler->GetOffsetData()->m_endDevicesFreq.end(); ++it )
    {
        // Assign DL Freq to every end device
        uint8_t index = m_uniform_rv->GetInteger(0, 7);
        NS_LOG_DEBUG("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA freq index -> " << unsigned(index));
        it->second = 867.1 + ((double)index * .2);
        m_status->GetEndDeviceStatus(it->first)->SetPingSlotFreq(867.1 + ((double)index * .2));
        // m_status->GetEndDeviceStatus(it->first)->GetMac() SetPingSlotFreq(867.1 + ((double)index * .2));

        // Assign Beacon Freq to every device
        m_status->GetEndDeviceStatus(it->first)->SetBeaconFreq(m_scheduler->GetOffsetData()->GetBeaconFreq());
    }

    Simulator::Schedule(Seconds(0), &NetworkServer::BeaconWindowOpen, this);
    Simulator::Schedule(Seconds(0), &NetworkServer::TrafficGeneratorManager, this);
    
}

void
NetworkServer::StopApplication(void)
{
    NS_LOG_FUNCTION_NOARGS();
}

void
NetworkServer::AddGateway(Ptr<Node> gateway, Ptr<NetDevice> netDevice)
{
    NS_LOG_FUNCTION(this << gateway);

    // Get the PointToPointNetDevice
    Ptr<PointToPointNetDevice> p2pNetDevice;
    for (uint32_t i = 0; i < gateway->GetNDevices(); i++)
    {
        p2pNetDevice = gateway->GetDevice(i)->GetObject<PointToPointNetDevice>();
        if (p2pNetDevice)
        {
            // We found a p2pNetDevice on the gateway
            break;
        }
    }

    // Get the gateway's LoRa MAC layer (assumes gateway's MAC is configured as first device)
    Ptr<GatewayLorawanMac> gwMac =
        gateway->GetDevice(0)->GetObject<LoraNetDevice>()->GetMac()->GetObject<GatewayLorawanMac>();
    NS_ASSERT(gwMac);

    // Get the Address
    Address gatewayAddress = p2pNetDevice->GetAddress();

    // Create new gatewayStatus
    Ptr<GatewayStatus> gwStatus = Create<GatewayStatus>(gatewayAddress, netDevice, gwMac);

    m_status->AddGateway(gatewayAddress, gwStatus);
}

void
NetworkServer::AddNodes(NodeContainer nodes)
{
    NS_LOG_FUNCTION_NOARGS();

    // For each node in the container, call the function to add that single node
    NodeContainer::Iterator it;
    for (it = nodes.Begin(); it != nodes.End(); it++)
    {
        AddNode(*it);
    }
}

void
NetworkServer::AddNode(Ptr<Node> node)
{
    NS_LOG_FUNCTION(this << node);

    // Get the LoraNetDevice
    Ptr<LoraNetDevice> loraNetDevice;
    for (uint32_t i = 0; i < node->GetNDevices(); i++)
    {
        loraNetDevice = node->GetDevice(i)->GetObject<LoraNetDevice>();
        if (loraNetDevice)
        {
            // We found a LoraNetDevice on the node
            break;
        }
    }

    // Get the MAC
    Ptr<ClassAEndDeviceLorawanMac> edLorawanMac =
        loraNetDevice->GetMac()->GetObject<ClassAEndDeviceLorawanMac>();

    // Add device to the offset data
    m_scheduler->GetOffsetData()->m_endDevicesOffset.insert(std::pair<LoraDeviceAddress, uint8_t>(edLorawanMac->GetDeviceAddress(), 0));
    m_scheduler->GetOffsetData()->m_endDevicesFreq.insert(std::pair<LoraDeviceAddress, double>(edLorawanMac->GetDeviceAddress(), 0));
    // m_status->m_endDeviceStatuses.begin()->first
    // LoraDeviceAddress edAddress = edMac->GetDeviceAddress();

    edLorawanMac->SetOffsetData(m_scheduler->GetOffsetData());

    // Update the NetworkStatus about the existence of this node
    m_status->AddNode(edLorawanMac);
}

bool
NetworkServer::Receive(Ptr<NetDevice> device,
                       Ptr<const Packet> packet,
                       uint16_t protocol,
                       const Address& address)
{
    NS_LOG_FUNCTION(this << packet << protocol << address);

    // Create a copy of the packet
    Ptr<Packet> myPacket = packet->Copy();

    // Fire the trace source
    m_receivedPacket(packet);

    // Inform the scheduler of the newly arrived packet
    m_scheduler->OnReceivedPacket(packet);

    // Inform the status of the newly arrived packet
    m_status->OnReceivedPacket(packet, address);

    // Inform the controller of the newly arrived packet
    m_controller->OnNewPacket(packet);

    return true;
}

void
NetworkServer::AddComponent(Ptr<NetworkControllerComponent> component)
{
    NS_LOG_FUNCTION(this << component);

    m_controller->Install(component);
}

Ptr<NetworkStatus>
NetworkServer::GetNetworkStatus(void)
{
    NS_LOG_FUNCTION(this);
    return m_status;
}

} // namespace lorawan
} // namespace ns3
