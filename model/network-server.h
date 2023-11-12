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

#ifndef NETWORK_SERVER_H
#define NETWORK_SERVER_H

#include "class-a-end-device-lorawan-mac.h"
#include "gateway-status.h"
#include "lora-device-address.h"
#include "network-controller.h"
#include "network-scheduler.h"
#include "network-status.h"
#include "offset_struct.h"

#include "ns3/application.h"
#include "ns3/log.h"
#include "ns3/net-device.h"
#include "ns3/node-container.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/point-to-point-net-device.h"

namespace ns3
{
namespace lorawan
{

/**
 * The NetworkServer is an application standing on top of a node equipped with
 * links that connect it with the gateways.
 *
 * This version of the NetworkServer attempts to closely mimic an actual
 * Network Server, by providing as much functionality as possible.
 */
class NetworkServer : public Application
{
  public:
    static TypeId GetTypeId(void);

    NetworkServer();
    virtual ~NetworkServer();

    /**
     * Start the NS application.
     */
    void StartApplication(void);

    /**
     * Stop the NS application.
     */
    void StopApplication(void);

    /**
     * Inform the NetworkServer that these nodes are connected to the network.
     *
     * This method will create a DeviceStatus object for each new node, and add
     * it to the list.
     */
    void AddNodes(NodeContainer nodes);

    /**
     * Inform the NetworkServer that this node is connected to the network.
     * This method will create a DeviceStatus object for the new node (if it
     * doesn't already exist).
     */
    void AddNode(Ptr<Node> node);

    /**
     * Add this gateway to the list of gateways connected to this NS.
     * Each GW is identified by its Address in the NS-GWs network.
     */
    void AddGateway(Ptr<Node> gateway, Ptr<NetDevice> netDevice);

    /**
     * A NetworkControllerComponent to this NetworkServer instance.
     */
    void AddComponent(Ptr<NetworkControllerComponent> component);

    /**
     * Receive a packet from a gateway.
     * \param packet the received packet
     */
    bool Receive(Ptr<NetDevice> device,
                 Ptr<const Packet> packet,
                 uint16_t protocol,
                 const Address& address);

    Ptr<NetworkStatus> GetNetworkStatus(void);
    
    /**
     *
     */
    void OffsetCalculation(void);

    void BeaconWindowOpen(void);

    void PingSlotOpen(LoraDeviceAddress devAddress);

    void TrafficEnqueue(Ptr<ExponentialRandomVariable> ev);
    
    void TrafficGeneratorManager(void);



  protected:
    Ptr<NetworkStatus> m_status;
    Ptr<NetworkController> m_controller;
    Ptr<NetworkScheduler> m_scheduler;

    TracedCallback<Ptr<const Packet>> m_receivedPacket;

    /**
     * Duration of a slot from the beacon period architecture. By standard,
     * it has a value of 30ms.
     */
    Time m_slotDuration;
    
    /**
     * K parameter value from [0,7]. 2**k represents the number of ping slot the
     * node will open within one beacon period
     */
    uint8_t m_kParameter;

    // /**
    //  * Reference to the last event created inside the beacon period loop
    //  */
    // EventId m_lastBeaconRelatedEVent;

    /**
     * Moment in time in which the beacon period started
     */
    Time m_startBeaconPeriod;

    /**
     * Duration of the beacon period
     */
    Time m_beaconPeriod;

    /**
     * Duration of the beacon reserved period
     */
    Time m_beaconReserved;

    /**
     * Duration of the pseudo random offset at the beginning of the first slot ping window
     */
    Time m_offset;

    /**
     * inverse of lambda paramter: packets/s generated at the NS is lambda, so this parameter
     * should be 1/lambda = [s/packet]
     */    
    double m_lambda_inv;

    Ptr<UniformRandomVariable> m_uniform_rv;


};

} // namespace lorawan

} // namespace ns3
#endif /* NETWORK_SERVER_H */
