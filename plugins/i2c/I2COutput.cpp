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
 * I2COutput.cpp
 * An RDM-controllable I2C device. Takes up to one universe of DMX.
 * Copyright (C) 2023 Olivier Baumgartner, 1345 Le Lieu, Switzerland
 * from work of Copyright (C) 2013 Simon Newton 
 *
 * The LPD8806 code was based on
 * https://github.com/adafruit/LPD8806/blob/master/LPD8806.cpp
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif  // HAVE_CONFIG_H

#include <string.h>
#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "ola/base/Array.h"
#include "ola/Constants.h"
#include "ola/Logging.h"
#include "ola/file/Util.h"
#include "ola/network/NetworkUtils.h"
#include "ola/rdm/OpenLightingEnums.h"
#include "ola/rdm/RDMCommand.h"
#include "ola/rdm/RDMEnums.h"
#include "ola/rdm/ResponderHelper.h"
#include "ola/rdm/ResponderLoadSensor.h"
#include "ola/rdm/ResponderSensor.h"
#include "ola/rdm/UID.h"
#include "ola/rdm/UIDSet.h"
#include "ola/stl/STLUtils.h"

#include "plugins/i2c/I2CBackend.h"
#include "plugins/i2c/I2COutput.h"

namespace ola {
namespace plugin {
namespace i2c {

using ola::file::FilenameFromPathOrPath;
using ola::network::HostToNetwork;
using ola::network::NetworkToHost;
using ola::rdm::LoadSensor;
using ola::rdm::NR_DATA_OUT_OF_RANGE;
using ola::rdm::NR_FORMAT_ERROR;
using ola::rdm::Personality;
using ola::rdm::PersonalityCollection;
using ola::rdm::PersonalityManager;
using ola::rdm::RDMCallback;
using ola::rdm::RDMCommand;
using ola::rdm::RDMRequest;
using ola::rdm::RDMResponse;
using ola::rdm::ResponderHelper;
using ola::rdm::UID;
using ola::rdm::UIDSet;
using std::min;
using std::string;
using std::vector;

const uint16_t I2COutput::I2C_DELAY = 0;
const uint8_t I2COutput::I2C_BITS_PER_WORD = 8;
const uint8_t I2COutput::I2C_MODE = 0;

/**
 * These constants are used to determine the number of DMX Slots per pixel
 * The p9813 uses another byte preceding each of the three bytes as a kind
 * of header.
 */
const uint16_t I2COutput::WS2801_SLOTS_PER_PIXEL = 3;
const uint16_t I2COutput::LPD8806_SLOTS_PER_PIXEL = 3;
const uint16_t I2COutput::P9813_SLOTS_PER_PIXEL = 3;
const uint16_t I2COutput::APA102_SLOTS_PER_PIXEL = 3;
const uint16_t I2COutput::APA102_PB_SLOTS_PER_PIXEL = 4;

// Number of bytes that each pixel uses on the I2C wires
// (if it differs from 1:1 with colors)
const uint16_t I2COutput::P9813_I2C_BYTES_PER_PIXEL = 4;
const uint16_t I2COutput::APA102_I2C_BYTES_PER_PIXEL = 4;

const uint16_t I2COutput::APA102_START_FRAME_BYTES = 4;
const uint8_t I2COutput::APA102_LEDFRAME_START_MARK = 0xE0;

I2COutput::RDMOps *I2COutput::RDMOps::instance = NULL;

const ola::rdm::ResponderOps<I2COutput>::ParamHandler
    I2COutput::PARAM_HANDLERS[] = {
  { ola::rdm::PID_DEVICE_INFO,
    &I2COutput::GetDeviceInfo,
    NULL},
  { ola::rdm::PID_PRODUCT_DETAIL_ID_LIST,
    &I2COutput::GetProductDetailList,
    NULL},
  { ola::rdm::PID_DEVICE_MODEL_DESCRIPTION,
    &I2COutput::GetDeviceModelDescription,
    NULL},
  { ola::rdm::PID_MANUFACTURER_LABEL,
    &I2COutput::GetManufacturerLabel,
    NULL},
  { ola::rdm::PID_DEVICE_LABEL,
    &I2COutput::GetDeviceLabel,
    &I2COutput::SetDeviceLabel},
  { ola::rdm::PID_SOFTWARE_VERSION_LABEL,
    &I2COutput::GetSoftwareVersionLabel,
    NULL},
  { ola::rdm::PID_DMX_PERSONALITY,
    &I2COutput::GetDmxPersonality,
    &I2COutput::SetDmxPersonality},
  { ola::rdm::PID_DMX_PERSONALITY_DESCRIPTION,
    &I2COutput::GetPersonalityDescription,
    NULL},
  { ola::rdm::PID_SLOT_INFO,
    &I2COutput::GetSlotInfo,
    NULL},
  { ola::rdm::PID_DMX_START_ADDRESS,
    &I2COutput::GetDmxStartAddress,
    &I2COutput::SetDmxStartAddress},
  { ola::rdm::PID_IDENTIFY_DEVICE,
    &I2COutput::GetIdentify,
    &I2COutput::SetIdentify},
#ifdef HAVE_GETLOADAVG
  { ola::rdm::PID_SENSOR_DEFINITION,
    &I2COutput::GetSensorDefinition,
    NULL},
  { ola::rdm::PID_SENSOR_VALUE,
    &I2COutput::GetSensorValue,
    &I2COutput::SetSensorValue},
  { ola::rdm::PID_RECORD_SENSORS,
    NULL,
    &I2COutput::RecordSensor},
#endif  // HAVE_GETLOADAVG
  { ola::rdm::PID_LIST_INTERFACES,
    &I2COutput::GetListInterfaces,
    NULL},
  { ola::rdm::PID_INTERFACE_LABEL,
    &I2COutput::GetInterfaceLabel,
    NULL},
  { ola::rdm::PID_INTERFACE_HARDWARE_ADDRESS_TYPE1,
    &I2COutput::GetInterfaceHardwareAddressType1,
    NULL},
  { ola::rdm::PID_IPV4_CURRENT_ADDRESS,
    &I2COutput::GetIPV4CurrentAddress,
    NULL},
  { ola::rdm::PID_IPV4_DEFAULT_ROUTE,
    &I2COutput::GetIPV4DefaultRoute,
    NULL},
  { ola::rdm::PID_DNS_HOSTNAME,
    &I2COutput::GetDNSHostname,
    NULL},
  { ola::rdm::PID_DNS_DOMAIN_NAME,
    &I2COutput::GetDNSDomainName,
    NULL},
  { ola::rdm::PID_DNS_NAME_SERVER,
    &I2COutput::GetDNSNameServer,
    NULL},
  { 0, NULL, NULL},
};


I2COutput::I2COutput(const UID &uid, I2CBackendInterface *backend,
                     const Options &options)
    : m_backend(backend),
      m_output_number(options.output_number),
      m_uid(uid),
      m_pixel_count(options.pixel_count),
      m_device_label(options.device_label),
      m_start_address(1),
      m_identify_mode(false) {
  m_i2c_device_name = FilenameFromPathOrPath(m_backend->DevicePath());

  PersonalityCollection::PersonalityList personalities;
  // personality description is max 32 characters

  ola::rdm::SlotDataCollection::SlotDataList sd_rgb_combined;
  sd_rgb_combined.push_back(
      ola::rdm::SlotData::PrimarySlot(ola::rdm::SD_COLOR_ADD_RED, 0));
  sd_rgb_combined.push_back(
      ola::rdm::SlotData::PrimarySlot(ola::rdm::SD_COLOR_ADD_GREEN, 0));
  sd_rgb_combined.push_back(
      ola::rdm::SlotData::PrimarySlot(ola::rdm::SD_COLOR_ADD_BLUE, 0));
  ola::rdm::SlotDataCollection sdc_rgb_combined;
  sdc_rgb_combined = ola::rdm::SlotDataCollection(sd_rgb_combined);

  ola::rdm::SlotDataCollection::SlotDataList sd_irgb_combined;
  sd_irgb_combined.push_back(
      ola::rdm::SlotData::PrimarySlot(ola::rdm::SD_INTENSITY,
                                      DMX_MAX_SLOT_VALUE));
  sd_irgb_combined.push_back(
      ola::rdm::SlotData::PrimarySlot(ola::rdm::SD_COLOR_ADD_RED, 0));
  sd_irgb_combined.push_back(
      ola::rdm::SlotData::PrimarySlot(ola::rdm::SD_COLOR_ADD_GREEN, 0));
  sd_irgb_combined.push_back(
      ola::rdm::SlotData::PrimarySlot(ola::rdm::SD_COLOR_ADD_BLUE, 0));
  ola::rdm::SlotDataCollection sdc_irgb_combined;
  sdc_irgb_combined = ola::rdm::SlotDataCollection(sd_irgb_combined);

  personalities.insert(
      personalities.begin() + PERS_WS2801_INDIVIDUAL - 1,
      Personality(m_pixel_count * WS2801_SLOTS_PER_PIXEL,
                  "WS2801 Individual Control"));
  personalities.insert(
      personalities.begin() + PERS_WS2801_COMBINED - 1,
      Personality(WS2801_SLOTS_PER_PIXEL,
                  "WS2801 Combined Control",
                  sdc_rgb_combined));

  personalities.insert(
      personalities.begin() + PERS_LDP8806_INDIVIDUAL - 1,
      Personality(m_pixel_count * LPD8806_SLOTS_PER_PIXEL,
                  "LPD8806 Individual Control"));
  personalities.insert(
      personalities.begin() + PERS_LDP8806_COMBINED - 1,
      Personality(LPD8806_SLOTS_PER_PIXEL,
                  "LPD8806 Combined Control",
                  sdc_rgb_combined));

  personalities.insert(
      personalities.begin() + PERS_P9813_INDIVIDUAL - 1,
      Personality(m_pixel_count * P9813_SLOTS_PER_PIXEL,
                  "P9813 Individual Control"));
  personalities.insert(
      personalities.begin() + PERS_P9813_COMBINED - 1,
      Personality(P9813_SLOTS_PER_PIXEL,
                  "P9813 Combined Control",
                  sdc_rgb_combined));

  personalities.insert(
      personalities.begin() + PERS_APA102_INDIVIDUAL - 1,
      Personality(m_pixel_count * APA102_SLOTS_PER_PIXEL,
                  "APA102 Individual Control"));

  personalities.insert(
      personalities.begin() + PERS_APA102_COMBINED - 1,
      Personality(APA102_SLOTS_PER_PIXEL,
                  "APA102 Combined Control",
                  sdc_rgb_combined));

  personalities.insert(
      personalities.begin() + PERS_APA102_PB_INDIVIDUAL - 1,
      Personality(m_pixel_count * APA102_PB_SLOTS_PER_PIXEL,
                  "APA102 Pixel Brightness Individ."));

  personalities.insert(
      personalities.begin() + PERS_APA102_PB_COMBINED - 1,
      Personality(APA102_PB_SLOTS_PER_PIXEL,
                  "APA102 Pixel Brightness Combined",
                  sdc_irgb_combined));

  m_personality_collection.reset(new PersonalityCollection(personalities));
  m_personality_manager.reset(new PersonalityManager(
      m_personality_collection.get()));
  m_personality_manager->SetActivePersonality(PERS_WS2801_INDIVIDUAL);

#ifdef HAVE_GETLOADAVG
  m_sensors.push_back(new LoadSensor(ola::system::LOAD_AVERAGE_1_MIN,
                                     "Load Average 1 minute"));
  m_sensors.push_back(new LoadSensor(ola::system::LOAD_AVERAGE_5_MINS,
                                     "Load Average 5 minutes"));
  m_sensors.push_back(new LoadSensor(ola::system::LOAD_AVERAGE_15_MINS,
                                     "Load Average 15 minutes"));
#endif  // HAVE_GETLOADAVG

  m_network_manager.reset(new ola::rdm::NetworkManager());
}

I2COutput::~I2COutput() {
  STLDeleteElements(&m_sensors);
}


string I2COutput::GetDeviceLabel() const {
  return m_device_label;
}

bool I2COutput::SetDeviceLabel(const string &device_label) {
  m_device_label = device_label;
  return true;
}

uint8_t I2COutput::GetPersonality() const {
  return m_personality_manager->ActivePersonalityNumber();
}

bool I2COutput::SetPersonality(uint16_t personality) {
  return m_personality_manager->SetActivePersonality(personality);
}

uint16_t I2COutput::GetStartAddress() const {
  return m_start_address;
}

bool I2COutput::SetStartAddress(uint16_t address) {
  uint16_t footprint = m_personality_manager->ActivePersonalityFootprint();
  uint16_t end_address = DMX_UNIVERSE_SIZE - footprint + 1;
  if (address == 0 || address > end_address || footprint == 0) {
    return false;
  }
  m_start_address = address;
  return true;
}

string I2COutput::Description() const {
  std::ostringstream str;
  str << "Output " << static_cast<int>(m_output_number) << ", "
      << m_personality_manager->ActivePersonalityDescription() << ", "
      << m_personality_manager->ActivePersonalityFootprint()
      << " slots @ " << m_start_address << ". (" << m_uid << ")";
  return str.str();
}

/*
 * Send DMX data over I2C.
 */
bool I2COutput::WriteDMX(const DmxBuffer &buffer) {
  if (m_identify_mode) {
    return true;
  }
  return InternalWriteDMX(buffer);
}


void I2COutput::RunFullDiscovery(ola::rdm::RDMDiscoveryCallback *callback) {
  UIDSet uids;
  uids.AddUID(m_uid);
  callback->Run(uids);
}


void I2COutput::RunIncrementalDiscovery(
    ola::rdm::RDMDiscoveryCallback *callback) {
  UIDSet uids;
  uids.AddUID(m_uid);
  callback->Run(uids);
}


void I2COutput::SendRDMRequest(RDMRequest *request,
                               RDMCallback *callback) {
  RDMOps::Instance()->HandleRDMRequest(this, m_uid, ola::rdm::ROOT_RDM_DEVICE,
                                       request, callback);
}

bool I2COutput::InternalWriteDMX(const DmxBuffer &buffer) {
  switch (m_personality_manager->ActivePersonalityNumber()) {
    case PERS_WS2801_INDIVIDUAL:
      IndividualWS2801Control(buffer);
      break;
    case PERS_WS2801_COMBINED:
      CombinedWS2801Control(buffer);
      break;
    case PERS_LDP8806_INDIVIDUAL:
      IndividualLPD8806Control(buffer);
      break;
    case PERS_LDP8806_COMBINED:
      CombinedLPD8806Control(buffer);
      break;
    case PERS_P9813_INDIVIDUAL:
      IndividualP9813Control(buffer);
      break;
    case PERS_P9813_COMBINED:
      CombinedP9813Control(buffer);
      break;
    case PERS_APA102_INDIVIDUAL:
      IndividualAPA102Control(buffer);
      break;
    case PERS_APA102_COMBINED:
      CombinedAPA102Control(buffer);
      break;
    case PERS_APA102_PB_INDIVIDUAL:
      IndividualAPA102ControlPixelBrightness(buffer);
      break;
    case PERS_APA102_PB_COMBINED:
      CombinedAPA102ControlPixelBrightness(buffer);
      break;
    default:
      break;
  }
  return true;
}


void I2COutput::IndividualWS2801Control(const DmxBuffer &buffer) {
  // We always check out the entire string length, even if we only have data
  // for part of it
  const unsigned int output_length = m_pixel_count * WS2801_SLOTS_PER_PIXEL;
  uint8_t *output = m_backend->Checkout(m_output_number, output_length);
  if (!output) {
    return;
  }

  unsigned int new_length = output_length;
  buffer.GetRange(m_start_address - 1, output, &new_length);
  m_backend->Commit(m_output_number);
}

void I2COutput::CombinedWS2801Control(const DmxBuffer &buffer) {
  unsigned int pixel_data_length = WS2801_SLOTS_PER_PIXEL;
  uint8_t pixel_data[WS2801_SLOTS_PER_PIXEL];
  buffer.GetRange(m_start_address - 1, pixel_data, &pixel_data_length);
  if (pixel_data_length != WS2801_SLOTS_PER_PIXEL) {
    OLA_INFO << "Insufficient DMX data, required " << WS2801_SLOTS_PER_PIXEL
             << ", got " << pixel_data_length;
    return;
  }

  const unsigned int length = m_pixel_count * WS2801_SLOTS_PER_PIXEL;
  uint8_t *output = m_backend->Checkout(m_output_number, length);
  if (!output) {
    return;
  }

  for (unsigned int i = 0; i < m_pixel_count; i++) {
    memcpy(output + (i * WS2801_SLOTS_PER_PIXEL), pixel_data,
           pixel_data_length);
  }
  m_backend->Commit(m_output_number);
}

void I2COutput::IndividualLPD8806Control(const DmxBuffer &buffer) {
  const uint8_t latch_bytes = (m_pixel_count + 31) / 32;
  const unsigned int first_slot = m_start_address - 1;  // 0 offset
  if (buffer.Size() - first_slot < LPD8806_SLOTS_PER_PIXEL) {
    // not even 3 bytes of data, don't bother updating
    return;
  }

  // We always check out the entire string length, even if we only have data
  // for part of it
  const unsigned int output_length = m_pixel_count * LPD8806_SLOTS_PER_PIXEL;
  uint8_t *output = m_backend->Checkout(m_output_number, output_length,
                                        latch_bytes);
  if (!output)
    return;

  const unsigned int length = min(m_pixel_count * LPD8806_SLOTS_PER_PIXEL,
                                       buffer.Size() - first_slot);

  for (unsigned int i = 0; i < length / LPD8806_SLOTS_PER_PIXEL; i++) {
    // Convert RGB to GRB
    unsigned int offset = first_slot + i * LPD8806_SLOTS_PER_PIXEL;
    uint8_t r = buffer.Get(offset);
    uint8_t g = buffer.Get(offset + 1);
    uint8_t b = buffer.Get(offset + 2);
    output[i * LPD8806_SLOTS_PER_PIXEL] = 0x80 | (g >> 1);
    output[i * LPD8806_SLOTS_PER_PIXEL + 1] = 0x80 | (r >> 1);
    output[i * LPD8806_SLOTS_PER_PIXEL + 2] = 0x80 | (b >> 1);
  }
  m_backend->Commit(m_output_number);
}

void I2COutput::CombinedLPD8806Control(const DmxBuffer &buffer) {
  const uint8_t latch_bytes = (m_pixel_count + 31) / 32;
  unsigned int pixel_data_length = LPD8806_SLOTS_PER_PIXEL;

  uint8_t pixel_data[LPD8806_SLOTS_PER_PIXEL];
  buffer.GetRange(m_start_address - 1, pixel_data, &pixel_data_length);
  if (pixel_data_length != LPD8806_SLOTS_PER_PIXEL) {
    OLA_INFO << "Insufficient DMX data, required " << LPD8806_SLOTS_PER_PIXEL
             << ", got " << pixel_data_length;
    return;
  }

  // The leds are GRB format so convert here
  uint8_t temp = pixel_data[1];
  pixel_data[1] = pixel_data[0];
  pixel_data[0] = temp;

  const unsigned int length = m_pixel_count * LPD8806_SLOTS_PER_PIXEL;
  uint8_t *output = m_backend->Checkout(m_output_number, length, latch_bytes);
  if (!output)
    return;

  for (unsigned int i = 0; i < m_pixel_count; i++) {
    for (unsigned int j = 0; j < LPD8806_SLOTS_PER_PIXEL; j++) {
      output[i * LPD8806_SLOTS_PER_PIXEL + j] = 0x80 | (pixel_data[j] >> 1);
    }
  }
  m_backend->Commit(m_output_number);
}

void I2COutput::IndividualP9813Control(const DmxBuffer &buffer) {
  // We need 4 bytes of zeros in the beginning and 8 bytes at
  // the end
  const uint8_t latch_bytes = 3 * P9813_I2C_BYTES_PER_PIXEL;
  const unsigned int first_slot = m_start_address - 1;  // 0 offset
  if (buffer.Size() - first_slot < P9813_SLOTS_PER_PIXEL) {
    // not even 3 bytes of data, don't bother updating
    return;
  }

  // We always check out the entire string length, even if we only have data
  // for part of it
  const unsigned int output_length = m_pixel_count * P9813_I2C_BYTES_PER_PIXEL;
  uint8_t *output = m_backend->Checkout(m_output_number, output_length,
                                        latch_bytes);

  if (!output) {
    return;
  }

  for (unsigned int i = 0; i < m_pixel_count; i++) {
    // Convert RGB to P9813 Pixel
    unsigned int offset = first_slot + i * P9813_SLOTS_PER_PIXEL;
    // We need to avoid the first 4 bytes of the buffer since that acts as a
    // start of frame delimiter
    unsigned int i2c_offset = (i + 1) * P9813_I2C_BYTES_PER_PIXEL;
    uint8_t r = 0;
    uint8_t b = 0;
    uint8_t g = 0;
    if (buffer.Size() - offset >= P9813_SLOTS_PER_PIXEL) {
      r = buffer.Get(offset);
      g = buffer.Get(offset + 1);
      b = buffer.Get(offset + 2);
    }
    output[i2c_offset] = P9813CreateFlag(r, g, b);
    output[i2c_offset + 1] = b;
    output[i2c_offset + 2] = g;
    output[i2c_offset + 3] = r;
  }
  m_backend->Commit(m_output_number);
}

void I2COutput::CombinedP9813Control(const DmxBuffer &buffer) {
  const uint8_t latch_bytes = 3 * P9813_I2C_BYTES_PER_PIXEL;
  const unsigned int first_slot = m_start_address - 1;  // 0 offset

  if (buffer.Size() - first_slot < P9813_SLOTS_PER_PIXEL) {
    OLA_INFO << "Insufficient DMX data, required " << P9813_SLOTS_PER_PIXEL
             << ", got " << buffer.Size() - first_slot;
    return;
  }

  uint8_t pixel_data[P9813_I2C_BYTES_PER_PIXEL];
  pixel_data[3] = buffer.Get(first_slot);  // Get Red
  pixel_data[2] = buffer.Get(first_slot + 1);  // Get Green
  pixel_data[1] = buffer.Get(first_slot + 2);  // Get Blue
  pixel_data[0] = P9813CreateFlag(pixel_data[3], pixel_data[2],
                                  pixel_data[1]);

  const unsigned int length = m_pixel_count * P9813_I2C_BYTES_PER_PIXEL;
  uint8_t *output = m_backend->Checkout(m_output_number, length, latch_bytes);
  if (!output) {
    return;
  }

  for (unsigned int i = 0; i < m_pixel_count; i++) {
    memcpy(&output[(i + 1) * P9813_I2C_BYTES_PER_PIXEL], pixel_data,
           P9813_I2C_BYTES_PER_PIXEL);
  }
  m_backend->Commit(m_output_number);
}

/**
 * For more information please visit:
 * https://github.com/CoolNeon/elinux-tcl/blob/master/README.txt
 */
uint8_t I2COutput::P9813CreateFlag(uint8_t red, uint8_t green, uint8_t blue) {
  uint8_t flag = 0;
  flag =  (red & 0xc0) >> 6;
  flag |= (green & 0xc0) >> 4;
  flag |= (blue & 0xc0) >> 2;
  return ~flag;
}


void I2COutput::IndividualAPA102Control(const DmxBuffer &buffer) {
  // some detailed information on the protocol:
  // https://cpldcpu.wordpress.com/2014/11/30/understanding-the-apa102-superled/
  // Data-Struct
  // StartFrame: 4 bytes = 32 bits zeros (APA102_START_FRAME_BYTES)
  // LEDFrame: 1 byte FF ; 3 bytes color info (Blue, Green, Red)
  // EndFrame: (n/2)bits; n = pixel_count

  // calculate DMX-start-address
  const unsigned int first_slot = m_start_address - 1;  // 0 offset

  // only do something if at least 1 pixel can be updated..
  if ((buffer.Size() - first_slot) < APA102_SLOTS_PER_PIXEL) {
    OLA_INFO << "Insufficient DMX data, required " << APA102_SLOTS_PER_PIXEL
             << ", got " << buffer.Size() - first_slot;
    return;
  }

  // We always check out the entire string length, even if we only have data
  // for part of it
  uint16_t output_length = (m_pixel_count * APA102_I2C_BYTES_PER_PIXEL);
  // only add the APA102_START_FRAME_BYTES on the first port!!
  if (m_output_number == 0) {
    output_length += APA102_START_FRAME_BYTES;
  }
  uint8_t *output = m_backend->Checkout(
      m_output_number,
      output_length,
      CalculateAPA102LatchBytes(m_pixel_count));

  // only update I2C data if possible
  if (!output) {
    return;
  }

  // only write to APA102_START_FRAME_BYTES on the first port!!
  if (m_output_number == 0) {
    // set APA102_START_FRAME_BYTES to zero
    memset(output, 0, APA102_START_FRAME_BYTES);
  }

  for (uint16_t i = 0; i < m_pixel_count; i++) {
    // Convert RGB to APA102 Pixel
    uint16_t offset = first_slot + (i * APA102_SLOTS_PER_PIXEL);


    uint16_t i2c_offset = (i * APA102_I2C_BYTES_PER_PIXEL);
    // only skip APA102_START_FRAME_BYTES on the first port!!
    if (m_output_number == 0) {
      // We need to avoid the first 4 bytes of the buffer since that acts as a
      // start of frame delimiter
      i2c_offset += APA102_START_FRAME_BYTES;
    }
    // set pixel data
    // first Byte contains:
    // 3 bits start mark (111) + 5 bits global brightness
    // set global brightness fixed to 31 --> that reduces flickering
    // that can be written as 0xE0 & 0x1F
    output[i2c_offset] = 0xFF;
    // only write pixel data if buffer has complete data for this pixel:
    if ((buffer.Size() - offset) >= APA102_SLOTS_PER_PIXEL) {
      // Convert RGB to APA102 Pixel
      // skip i2c_offset + 0 (is already set)
      output[i2c_offset + 1] = buffer.Get(offset + 2);  // blue
      output[i2c_offset + 2] = buffer.Get(offset + 1);  // green
      output[i2c_offset + 3] = buffer.Get(offset);      // red
    }
  }

  // write output back
  m_backend->Commit(m_output_number);
}


void I2COutput::IndividualAPA102ControlPixelBrightness(
    const DmxBuffer &buffer) {
  // some detailed information on the protocol:
  // https://cpldcpu.wordpress.com/2014/11/30/understanding-the-apa102-superled/
  // Data-Struct
  // StartFrame: 4 bytes = 32 bits zeros (APA102_START_FRAME_BYTES)
  // LEDFrame:
  //    1 byte START_MARK + pixel brightness
  //    3 bytes color info (Blue, Green, Red)
  // EndFrame: (n/2)bits; n = pixel_count

  // calculate DMX-start-address
  const unsigned int first_slot = m_start_address - 1;  // 0 offset

  // only do something if at least 1 pixel can be updated..
  if ((buffer.Size() - first_slot) < APA102_PB_SLOTS_PER_PIXEL) {
    OLA_INFO << "Insufficient DMX data, required " << APA102_PB_SLOTS_PER_PIXEL
             << ", got " << buffer.Size() - first_slot;
    return;
  }

  // We always check out the entire string length, even if we only have data
  // for part of it
  uint16_t output_length = (m_pixel_count * APA102_I2C_BYTES_PER_PIXEL);
  // only add the APA102_START_FRAME_BYTES on the first port!!
  if (m_output_number == 0) {
    output_length += APA102_START_FRAME_BYTES;
  }
  uint8_t *output = m_backend->Checkout(
      m_output_number,
      output_length,
      CalculateAPA102LatchBytes(m_pixel_count));

  // only update I2C data if possible
  if (!output) {
    return;
  }

  // only write to APA102_START_FRAME_BYTES on the first port!!
  if (m_output_number == 0) {
    // set APA102_START_FRAME_BYTES to zero
    memset(output, 0, APA102_START_FRAME_BYTES);
  }

  for (uint16_t i = 0; i < m_pixel_count; i++) {
    // Convert RGB to APA102 Pixel
    uint16_t offset = first_slot + (i * APA102_PB_SLOTS_PER_PIXEL);

    uint16_t i2c_offset = (i * APA102_I2C_BYTES_PER_PIXEL);
    // only skip APA102_START_FRAME_BYTES on the first port!!
    if (m_output_number == 0) {
      // We need to avoid the first 4 bytes of the buffer since that acts as a
      // start of frame delimiter
      i2c_offset += APA102_START_FRAME_BYTES;
    }
    // set pixel data
    // only write pixel data if buffer has complete data for this pixel:
    if ((buffer.Size() - offset) >= APA102_PB_SLOTS_PER_PIXEL) {
      // first Byte:
      // 3 bits start mark (111) (APA102_LEDFRAME_START_MARK) +
      // 5 bits pixel brightness (datasheet name: global brightness)
      output[i2c_offset + 0] = (I2COutput::APA102_LEDFRAME_START_MARK |
          CalculateAPA102PixelBrightness(buffer.Get(offset + 0)));
      // Convert RGB to APA102 Pixel
      output[i2c_offset + 1] = buffer.Get(offset + 3);  // blue
      output[i2c_offset + 2] = buffer.Get(offset + 2);  // green
      output[i2c_offset + 3] = buffer.Get(offset + 1);  // red
    }
  }

  // write output back
  m_backend->Commit(m_output_number);
}

void I2COutput::CombinedAPA102Control(const DmxBuffer &buffer) {
  // for Protocol details see IndividualAPA102Control

  // calculate DMX-start-address
  const uint16_t first_slot = m_start_address - 1;  // 0 offset

  // check if enough data is there.
  if ((buffer.Size() - first_slot) < APA102_SLOTS_PER_PIXEL) {
    OLA_INFO << "Insufficient DMX data, required " << APA102_SLOTS_PER_PIXEL
             << ", got " << buffer.Size() - first_slot;
    return;
  }

  // We always check out the entire string length, even if we only have data
  // for part of it
  uint16_t output_length = (m_pixel_count * APA102_I2C_BYTES_PER_PIXEL);
  // only add the APA102_START_FRAME_BYTES on the first port!!
  if (m_output_number == 0) {
    output_length += APA102_START_FRAME_BYTES;
  }
  uint8_t *output = m_backend->Checkout(
      m_output_number,
      output_length,
      CalculateAPA102LatchBytes(m_pixel_count));

  // only update I2C data if possible
  if (!output) {
    return;
  }

  // only write to APA102_START_FRAME_BYTES on the first port!!
  if (m_output_number == 0) {
    // set APA102_START_FRAME_BYTES to zero
    memset(output, 0, APA102_START_FRAME_BYTES);
  }

  // create Pixel Data
  uint8_t pixel_data[APA102_I2C_BYTES_PER_PIXEL];
  pixel_data[0] = 0xFF;
  pixel_data[1] = buffer.Get(first_slot + 2);  // Get Blue
  pixel_data[2] = buffer.Get(first_slot + 1);  // Get Green
  pixel_data[3] = buffer.Get(first_slot);      // Get Red

  // set all pixel to same value
  for (uint16_t i = 0; i < m_pixel_count; i++) {
    uint16_t i2c_offset = (i * APA102_I2C_BYTES_PER_PIXEL);
    if (m_output_number == 0) {
      i2c_offset += APA102_START_FRAME_BYTES;
    }
    memcpy(&output[i2c_offset], pixel_data,
           APA102_I2C_BYTES_PER_PIXEL);
  }

  // write output back...
  m_backend->Commit(m_output_number);
}


void I2COutput::CombinedAPA102ControlPixelBrightness(const DmxBuffer &buffer) {
  // for Protocol details see IndividualAPA102Control
  // calculate DMX-start-address
  const uint16_t first_slot = m_start_address - 1;  // 0 offset

  // check if enough data is there.
  if ((buffer.Size() - first_slot) < APA102_PB_SLOTS_PER_PIXEL) {
    OLA_INFO << "Insufficient DMX data, required " << APA102_PB_SLOTS_PER_PIXEL
             << ", got " << buffer.Size() - first_slot;
    return;
  }

  // We always check out the entire string length, even if we only have data
  // for part of it
  uint16_t output_length = (m_pixel_count * APA102_I2C_BYTES_PER_PIXEL);
  // only add the APA102_START_FRAME_BYTES on the first port!!
  if (m_output_number == 0) {
    output_length += APA102_START_FRAME_BYTES;
  }
  uint8_t *output = m_backend->Checkout(
      m_output_number,
      output_length,
      CalculateAPA102LatchBytes(m_pixel_count));

  // only update I2C data if possible
  if (!output) {
    return;
  }

  // only write to APA102_START_FRAME_BYTES on the first port!!
  if (m_output_number == 0) {
    // set APA102_START_FRAME_BYTES to zero
    memset(output, 0, APA102_START_FRAME_BYTES);
  }

  // create Pixel Data
  uint8_t pixel_data[APA102_I2C_BYTES_PER_PIXEL];
  // first Byte:
  // 3 bits start mark (111) (APA102_LEDFRAME_START_MARK) +
  // 5 bits pixel brightness (datasheet name: global brightness)
  pixel_data[0] = (I2COutput::APA102_LEDFRAME_START_MARK |
      CalculateAPA102PixelBrightness(buffer.Get(first_slot + 0)));
  // color data
  pixel_data[1] = buffer.Get(first_slot + 3);  // Get Blue
  pixel_data[2] = buffer.Get(first_slot + 2);  // Get Green
  pixel_data[3] = buffer.Get(first_slot + 1);  // Get Red

  // set all pixel to same value
  for (uint16_t i = 0; i < m_pixel_count; i++) {
    uint16_t i2c_offset = (i * APA102_I2C_BYTES_PER_PIXEL);
    if (m_output_number == 0) {
      i2c_offset += APA102_START_FRAME_BYTES;
    }
    memcpy(&output[i2c_offset], pixel_data,
           APA102_I2C_BYTES_PER_PIXEL);
  }

  // write output back...
  m_backend->Commit(m_output_number);
}

/**
 * Calculate Latch Bytes for APA102:
 * Use at least half the pixel count bits
 * round up to next full byte count.
 * datasheet says endframe should consist of 4 bytes -
 * but that's only valid for up to 64 pixels/leds. (4Byte*8Bit*2=64)
 *
 * the function is valid up to 4080 pixels. (255*8*2)
 * ( otherwise the return type must be changed to uint16_t)
 */
uint8_t I2COutput::CalculateAPA102LatchBytes(uint16_t pixel_count) {
  // round up so that we get definitely enough bits
  const uint8_t latch_bits = (pixel_count + 1) / 2;
  const uint8_t latch_bytes = (latch_bits + 7) / 8;
  return latch_bytes;
}

/**
 * Calculate Pixel Brightness for APA102:
 * Map Input to Output range:
 * Input is 8bit value (0..255)
 * Output is 5bit value (0..31)
 */
uint8_t I2COutput::CalculateAPA102PixelBrightness(uint8_t brightness) {
  return (brightness >> 3);
}



RDMResponse *I2COutput::GetDeviceInfo(const RDMRequest *request) {
  return ResponderHelper::GetDeviceInfo(
      request, ola::rdm::OLA_I2C_DEVICE_MODEL,
      ola::rdm::PRODUCT_CATEGORY_FIXTURE,
      5,  // RDM software version (increment on personality changes)
      m_personality_manager.get(),
      m_start_address,
      0, m_sensors.size());
}

RDMResponse *I2COutput::GetProductDetailList(const RDMRequest *request) {
  // Shortcut for only one item in the vector
  return ResponderHelper::GetProductDetailList(request,
    vector<ola::rdm::rdm_product_detail>
        (1, ola::rdm::PRODUCT_DETAIL_LED));
}

RDMResponse *I2COutput::GetDeviceModelDescription(const RDMRequest *request) {
  return ResponderHelper::GetString(request, "OLA I2C Device");
}

RDMResponse *I2COutput::GetManufacturerLabel(const RDMRequest *request) {
  return ResponderHelper::GetString(
      request,
      ola::rdm::OLA_MANUFACTURER_LABEL);
}

RDMResponse *I2COutput::GetDeviceLabel(const RDMRequest *request) {
  return ResponderHelper::GetString(request, m_device_label);
}

RDMResponse *I2COutput::SetDeviceLabel(const RDMRequest *request) {
  return ResponderHelper::SetString(request, &m_device_label);
}

RDMResponse *I2COutput::GetSoftwareVersionLabel(const RDMRequest *request) {
  return ResponderHelper::GetString(request, string("OLA Version ") + VERSION);
}

RDMResponse *I2COutput::GetDmxPersonality(const RDMRequest *request) {
  return ResponderHelper::GetPersonality(request, m_personality_manager.get());
}

RDMResponse *I2COutput::SetDmxPersonality(const RDMRequest *request) {
  return ResponderHelper::SetPersonality(request, m_personality_manager.get(),
                                         m_start_address);
}

RDMResponse *I2COutput::GetPersonalityDescription(const RDMRequest *request) {
  return ResponderHelper::GetPersonalityDescription(
      request, m_personality_manager.get());
}

RDMResponse *I2COutput::GetSlotInfo(const RDMRequest *request) {
  return ResponderHelper::GetSlotInfo(request, m_personality_manager.get());
}

RDMResponse *I2COutput::GetDmxStartAddress(const RDMRequest *request) {
  return ResponderHelper::GetDmxAddress(request, m_personality_manager.get(),
                                        m_start_address);
}

RDMResponse *I2COutput::SetDmxStartAddress(const RDMRequest *request) {
  return ResponderHelper::SetDmxAddress(request, m_personality_manager.get(),
                                        &m_start_address);
}

RDMResponse *I2COutput::GetIdentify(const RDMRequest *request) {
  return ResponderHelper::GetBoolValue(request, m_identify_mode);
}

RDMResponse *I2COutput::SetIdentify(const RDMRequest *request) {
  bool old_value = m_identify_mode;
  RDMResponse *response = ResponderHelper::SetBoolValue(
      request, &m_identify_mode);
  if (m_identify_mode != old_value) {
    OLA_INFO << "I2C " << m_i2c_device_name << " identify mode " << (
        m_identify_mode ? "on" : "off");
    DmxBuffer identify_buffer;
    if (m_identify_mode) {
      identify_buffer.SetRangeToValue(0, DMX_MAX_SLOT_VALUE,
                                      DMX_UNIVERSE_SIZE);
    } else {
      identify_buffer.Blackout();
    }
    InternalWriteDMX(identify_buffer);
  }
  return response;
}

/**
 * PID_SENSOR_DEFINITION
 */
RDMResponse *I2COutput::GetSensorDefinition(const RDMRequest *request) {
  return ResponderHelper::GetSensorDefinition(request, m_sensors);
}

/**
 * PID_SENSOR_VALUE
 */
RDMResponse *I2COutput::GetSensorValue(const RDMRequest *request) {
  return ResponderHelper::GetSensorValue(request, m_sensors);
}

RDMResponse *I2COutput::SetSensorValue(const RDMRequest *request) {
  return ResponderHelper::SetSensorValue(request, m_sensors);
}

/**
 * PID_RECORD_SENSORS
 */
RDMResponse *I2COutput::RecordSensor(const RDMRequest *request) {
  return ResponderHelper::RecordSensor(request, m_sensors);
}

/**
 * E1.37-2 PIDs
 */
RDMResponse *I2COutput::GetListInterfaces(const RDMRequest *request) {
  return ResponderHelper::GetListInterfaces(request,
                                            m_network_manager.get());
}

RDMResponse *I2COutput::GetInterfaceLabel(const RDMRequest *request) {
  return ResponderHelper::GetInterfaceLabel(request,
                                            m_network_manager.get());
}

RDMResponse *I2COutput::GetInterfaceHardwareAddressType1(
    const RDMRequest *request) {
  return ResponderHelper::GetInterfaceHardwareAddressType1(
      request,
      m_network_manager.get());
}

RDMResponse *I2COutput::GetIPV4CurrentAddress(const RDMRequest *request) {
  return ResponderHelper::GetIPV4CurrentAddress(request,
                                                m_network_manager.get());
}

RDMResponse *I2COutput::GetIPV4DefaultRoute(const RDMRequest *request) {
  return ResponderHelper::GetIPV4DefaultRoute(request,
                                              m_network_manager.get());
}

RDMResponse *I2COutput::GetDNSHostname(const RDMRequest *request) {
  return ResponderHelper::GetDNSHostname(request,
                                         m_network_manager.get());
}

RDMResponse *I2COutput::GetDNSDomainName(const RDMRequest *request) {
  return ResponderHelper::GetDNSDomainName(request,
                                           m_network_manager.get());
}

RDMResponse *I2COutput::GetDNSNameServer(const RDMRequest *request) {
  return ResponderHelper::GetDNSNameServer(request,
                                           m_network_manager.get());
}
}  // namespace i2c
}  // namespace plugin
}  // namespace ola
