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
 * I2CPlugin.cpp
 * The I2C plugin for ola
 * Copyright (C) 2023 Olivier Baumgartner, 1345 Le Lieu, Switzerland
 * from work of Copyright (C) 2013 Simon Newton
 */

#include <memory>
#include <string>
#include <vector>
#include "ola/Logging.h"
#include "ola/StringUtils.h"
#include "ola/file/Util.h"
#include "ola/rdm/UID.h"
#include "ola/rdm/UIDAllocator.h"
#include "olad/PluginAdaptor.h"
#include "olad/Preferences.h"
#include "plugins/i2c/I2CDevice.h"
#include "plugins/i2c/I2CPlugin.h"
#include "plugins/i2c/I2CPluginDescription.h"


namespace ola {
namespace plugin {
namespace i2c {

using ola::rdm::UID;
using std::auto_ptr;
using std::string;
using std::vector;

const char I2CPlugin::DEFAULT_BASE_UID[] = "7a70:00000100";
const char I2CPlugin::DEFAULT_I2C_DEVICE_PREFIX[] = "i2cdev";
const char I2CPlugin::PLUGIN_NAME[] = "I2C";
const char I2CPlugin::PLUGIN_PREFIX[] = "i2c";
const char I2CPlugin::I2C_BASE_UID_KEY[] = "base_uid";
const char I2CPlugin::I2C_DEVICE_PREFIX_KEY[] = "device_prefix";

/*
 * Start the plugin
 * For now we just have one device.
 */
bool I2CPlugin::StartHook() {
  const string uid_str = m_preferences->GetValue(I2C_BASE_UID_KEY);
  auto_ptr<UID> base_uid(UID::FromString(uid_str));
  if (!base_uid.get()) {
    OLA_WARN << "Invalid UID " << uid_str << ", defaulting to "
             << DEFAULT_BASE_UID;
    base_uid.reset(UID::FromString(DEFAULT_BASE_UID));
    if (!base_uid.get()) {
      OLA_WARN << "Invalid UID " << DEFAULT_BASE_UID;
      return false;
    }
  }

  vector<string> i2c_files;
  vector<string> i2c_prefixes = m_preferences->GetMultipleValue(
      I2C_DEVICE_PREFIX_KEY);
  if (!ola::file::FindMatchingFiles("/dev", i2c_prefixes, &i2c_files)) {
    return false;
  }

  ola::rdm::UIDAllocator uid_allocator(*base_uid);
  vector<string>::const_iterator iter = i2c_files.begin();
  for (; iter != i2c_files.end(); ++iter) {
    I2CDevice *device = new I2CDevice(this, m_preferences, m_plugin_adaptor,
                                      *iter, &uid_allocator);

    if (!device) {
      continue;
    }

    if (!device->Start()) {
      delete device;
      continue;
    }
    m_devices.push_back(device);
    m_plugin_adaptor->RegisterDevice(device);
  }
  return true;
}


/*
 * Stop the plugin
 * @return true on success, false on failure
 */
bool I2CPlugin::StopHook() {
  vector<I2CDevice*>::iterator iter = m_devices.begin();
  bool ok = true;
  for (; iter != m_devices.end(); ++iter) {
    m_plugin_adaptor->UnregisterDevice(*iter);
    ok &= (*iter)->Stop();
    delete *iter;
  }
  return ok;
}


/*
 * Return the description for this plugin
 */
string I2CPlugin::Description() const {
  return plugin_description;
}


/*
 * Load the plugin prefs and default to sensible values
 */
bool I2CPlugin::SetDefaultPreferences() {
  bool save = false;

  if (!m_preferences) {
    return false;
  }

  save |= m_preferences->SetDefaultValue(I2C_DEVICE_PREFIX_KEY,
                                         StringValidator(),
                                         DEFAULT_I2C_DEVICE_PREFIX);
  save |= m_preferences->SetDefaultValue(I2C_BASE_UID_KEY,
                                         StringValidator(),
                                         DEFAULT_BASE_UID);
  if (save) {
    m_preferences->Save();
  }

  if (m_preferences->GetValue(I2C_DEVICE_PREFIX_KEY).empty()) {
    return false;
  }

  return true;
}
}  // namespace i2c
}  // namespace plugin
}  // namespace ola
