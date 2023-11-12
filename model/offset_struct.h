/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2017 University of Padova
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
 * Author: Matteo Perin <matteo.perin.2@studenti.unipd.2>
 */

#ifndef OFFSET_STRUCT_H
#define OFFSET_STRUCT_H

#include "ns3/nstime.h"
#include "ns3/object.h"
#include "lora-device-address.h"

namespace ns3
{
namespace lorawan
{

class StructTest : public Object

    {
        public:
            static TypeId GetTypeId(void);
            StructTest();
            virtual ~StructTest();

            std::map<LoraDeviceAddress, double> m_endDevicesFreq;
            std::map<LoraDeviceAddress, uint8_t> m_endDevicesOffset;
            double GetBeaconFreq();
            void SetBeaconFreq(double freq);
        private:

            double m_beaconFreq;
    };

} // namespace lorawan
} // namespace ns3

#endif
