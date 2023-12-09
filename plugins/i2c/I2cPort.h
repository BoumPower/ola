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
 * I2CPort.h
 * An OLA I2C Port. This simply wraps the I2COutput.
 * Copyright (C) 2023 Olivier Baumgartner, 1345 Le Lieu, Switzerland
 * from work of Copyright (C) 2013 Simon Newton
 */

#ifndef PLUGINS_I2C_I2CPORT_H_
#define PLUGINS_I2C_I2CPORT_H_

#include <string>
#include "ola/DmxBuffer.h"
#include "olad/Port.h"
#include "plugins/i2c/I2CDevice.h"
#include "plugins/i2c/I2COutput.h"

namespace ola {
namespace plugin {
namespace i2c {

class I2COutputPort: public BasicOutputPort {
 public:
  I2COutputPort(I2CDevice *parent, class I2CBackendInterface *backend,
                const ola::rdm::UID &uid, const I2COutput::Options &options);
  ~I2COutputPort() {}

  std::string GetDeviceLabel() const;
  bool SetDeviceLabel(const std::string &device_label);
  uint8_t GetPersonality() const;
  bool SetPersonality(uint16_t personality);
  uint16_t GetStartAddress() const;
  bool SetStartAddress(uint16_t start_address);
  unsigned int PixelCount() const;

  std::string Description() const;
  bool WriteDMX(const DmxBuffer &buffer, uint8_t priority);

  void RunFullDiscovery(ola::rdm::RDMDiscoveryCallback *callback);
  void RunIncrementalDiscovery(ola::rdm::RDMDiscoveryCallback *callback);
  void SendRDMRequest(ola::rdm::RDMRequest *request,
                      ola::rdm::RDMCallback *callback);

 private:
  I2COutput m_i2c_output;
};
}  // namespace i2c
}  // namespace plugin
}  // namespace ola
#endif  // PLUGINS_I2C_I2CPORT_H_
