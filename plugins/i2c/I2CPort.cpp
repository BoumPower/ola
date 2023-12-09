/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * I2CPort.cpp
 * The I2C plugin for ola
 * Copyright (C) 2023 Olivier Baumgartner, 1345 Le Lieu, Switzerland
 * from work of Copyright (C) 2013 Simon Newton
 */

#include <string>
#include "ola/Constants.h"
#include "ola/rdm/RDMCommand.h"
#include "ola/rdm/UID.h"

#include "plugins/i2c/I2CBackend.h"
#include "plugins/i2c/I2CPort.h"

namespace ola {
namespace plugin {
namespace i2c {

using ola::rdm::RDMCallback;
using ola::rdm::RDMDiscoveryCallback;
using ola::rdm::RDMRequest;
using ola::rdm::UID;
using std::string;

I2COutputPort::I2COutputPort(I2CDevice *parent, I2CBackendInterface *backend,
                             const UID &uid,
                             const I2COutput::Options &options)
    : BasicOutputPort(parent, options.output_number, true),
      m_i2c_output(uid, backend, options) {
}


string I2COutputPort::GetDeviceLabel() const {
  return m_i2c_output.GetDeviceLabel();
}

bool I2COutputPort::SetDeviceLabel(const string &device_label) {
  return m_i2c_output.SetDeviceLabel(device_label);
}

uint8_t I2COutputPort::GetPersonality() const {
  return m_i2c_output.GetPersonality();
}

bool I2COutputPort::SetPersonality(uint16_t personality) {
  return m_i2c_output.SetPersonality(personality);
}

uint16_t I2COutputPort::GetStartAddress() const {
  return m_i2c_output.GetStartAddress();
}

bool I2COutputPort::SetStartAddress(uint16_t address) {
  return m_i2c_output.SetStartAddress(address);
}

unsigned int I2COutputPort::PixelCount() const {
  return m_i2c_output.PixelCount();
}

string I2COutputPort::Description() const {
  return m_i2c_output.Description();
}

bool I2COutputPort::WriteDMX(const DmxBuffer &buffer, uint8_t) {
  return m_i2c_output.WriteDMX(buffer);
}

void I2COutputPort::RunFullDiscovery(RDMDiscoveryCallback *callback) {
  return m_i2c_output.RunFullDiscovery(callback);
}

void I2COutputPort::RunIncrementalDiscovery(RDMDiscoveryCallback *callback) {
  return m_i2c_output.RunIncrementalDiscovery(callback);
}

void I2COutputPort::SendRDMRequest(ola::rdm::RDMRequest *request,
                                   ola::rdm::RDMCallback *callback) {
  return m_i2c_output.SendRDMRequest(request, callback);
}
}  // namespace i2c
}  // namespace plugin
}  // namespace ola
