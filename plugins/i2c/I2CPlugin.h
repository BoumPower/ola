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
 * i2cPlugin.h
 * An i2c plugin.
 * Copyright (C) 2023 Olivier Baumgartner, 1345 Le Lieu, Switzerland
 * from work of Copyright (C) 2013 Simon Newton
 */

#ifndef PLUGINS_I2C_I2CPLUGIN_H_
#define PLUGINS_I2C_I2CPLUGIN_H_

#include <string>
#include <vector>
#include "olad/Plugin.h"
#include "ola/plugin_id.h"

namespace ola {
namespace plugin {
namespace i2c {

class I2CPlugin: public ola::Plugin {
 public:
  explicit I2CPlugin(class ola::PluginAdaptor *plugin_adaptor)
      : Plugin(plugin_adaptor) {}

  std::string Name() const { return PLUGIN_NAME; }
  std::string Description() const;
  ola_plugin_id Id() const { return OLA_PLUGIN_I2C; }
  std::string PluginPrefix() const { return PLUGIN_PREFIX; }

 private:
  std::vector<class I2CDevice*> m_devices;

  bool StartHook();
  bool StopHook();
  bool SetDefaultPreferences();

  static const char DEFAULT_BASE_UID[];
  static const char DEFAULT_I2C_DEVICE_PREFIX[];
  static const char PLUGIN_NAME[];
  static const char PLUGIN_PREFIX[];
  static const char I2C_BASE_UID_KEY[];
  static const char I2C_DEVICE_PREFIX_KEY[];
};
}  // namespace i2c
}  // namespace plugin
}  // namespace ola
#endif  // PLUGINS_I2C_I2CPLUGIN_H_
