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
 * I2CWriter.h
 * This writes data to a I2C device.
 * Copyright (C) 2023 Olivier Baumgartner, 1345 Le Lieu, Switzerland
 * from work of Copyright (C) 2013 Simon Newton
 */

#ifndef PLUGINS_I2C_I2CWRITER_H_
#define PLUGINS_I2C_I2CWRITER_H_

#include <ola/ExportMap.h>
#include <ola/thread/Mutex.h>
#include <stdint.h>
#include <string>

namespace ola {
namespace plugin {
namespace i2c {

/**
 * The interface for the I2C Writer
 */
class I2CWriterInterface {
 public:
  virtual ~I2CWriterInterface() {}

  virtual std::string DevicePath() const = 0;
  virtual bool Init() = 0;
  virtual bool WriteI2CData(const uint8_t *data, unsigned int length) = 0;
};

/**
 * The I2C Writer, this writes data to a I2C device
 */
class I2CWriter : public I2CWriterInterface {
 public:
  /**
   * I2CWriter Options
   */
  struct Options {
    uint32_t i2c_speed;
    bool cs_enable_high;

    Options() : i2c_speed(1000000), cs_enable_high(false) {}
  };

  I2CWriter(const std::string &i2c_device, const Options &options,
            ExportMap *export_map);
  ~I2CWriter();

  std::string DevicePath() const { return m_device_path; }

  /**
   * Init the I2CWriter
   * @returns false if initialization failed.
   */
  bool Init();

  bool WriteI2CData(const uint8_t *data, unsigned int length);

 private:
  const std::string m_device_path;
  const uint32_t m_i2c_speed;
  const bool m_cs_enable_high;
  int m_fd;
  UIntMap *m_error_map_var;
  UIntMap *m_write_map_var;

  static const uint8_t I2C_MODE;
  static const uint8_t I2C_BITS_PER_WORD;
  static const char I2C_DEVICE_KEY[];
  static const char I2C_ERROR_VAR[];
  static const char I2C_WRITE_VAR[];
};
}  // namespace i2c
}  // namespace plugin
}  // namespace ola
#endif  // PLUGINS_I2C_I2CWRITER_H_
