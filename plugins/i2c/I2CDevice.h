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
 * I2CDevice.h
 * The I2C Device class
 * Copyright (C) 2023 Olivier Baumgartner, 1345 Le Lieu, Switzerland
 * from work of Copyright (C) 2013 Simon Newton
 */

#ifndef PLUGINS_I2C_I2CDEVICE_H_
#define PLUGINS_I2C_I2CDEVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "olad/Device.h"
#include "ola/io/SelectServer.h"
#include "ola/rdm/UIDAllocator.h"
#include "ola/rdm/UID.h"
#include "plugins/i2c/I2CBackend.h"
#include "plugins/i2c/I2CWriter.h"

namespace ola {
namespace plugin {
namespace i2c {


class I2CDevice: public ola::Device {
 public:
  I2CDevice(class I2CPlugin *owner,
            class Preferences *preferences,
            class PluginAdaptor *plugin_adaptor,
            const std::string &i2c_device,
            ola::rdm::UIDAllocator *uid_allocator);

  std::string DeviceId() const;

  bool AllowMultiPortPatching() const { return true; }

 protected:
  bool StartHook();
  void PrePortStop();

 private:
  typedef std::vector<class I2COutputPort*> I2CPorts;

  std::auto_ptr<I2CWriterInterface> m_writer;
  std::auto_ptr<I2CBackendInterface> m_backend;
  class Preferences *m_preferences;
  class PluginAdaptor *m_plugin_adaptor;
  I2CPorts m_i2c_ports;
  std::string m_i2c_device_name;

  // Per device options
  std::string I2CBackendKey() const;
  std::string I2CSpeedKey() const;
  std::string I2CCEKey() const;
  std::string PortCountKey() const;
  std::string SyncPortKey() const;
  std::string GPIOPinKey() const;

  // Per port options
  std::string DeviceLabelKey(uint8_t port) const;
  std::string PersonalityKey(uint8_t port) const;
  std::string PixelCountKey(uint8_t port) const;
  std::string StartAddressKey(uint8_t port) const;
  std::string GetPortKey(const std::string &suffix, uint8_t port) const;

  void SetDefaults();
  void PopulateHardwareBackendOptions(HardwareBackend::Options *options);
  void PopulateSoftwareBackendOptions(SoftwareBackend::Options *options);
  void PopulateWriterOptions(I2CWriter::Options *options);

  static const char I2C_DEVICE_NAME[];
  static const char HARDWARE_BACKEND[];
  static const char SOFTWARE_BACKEND[];
  static const uint16_t MAX_GPIO_PIN = 1023;
  static const uint32_t MAX_I2C_SPEED = 32000000;
  static const uint16_t MAX_PORT_COUNT = 32;
};
}  // namespace i2c
}  // namespace plugin
}  // namespace ola
#endif  // PLUGINS_I2C_I2CDEVICE_H_
