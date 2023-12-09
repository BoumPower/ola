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
 * I2CWriter.cpp
 * This writes data to a I2C device.
  * Copyright (C) 2023 Olivier Baumgartner, 1345 Le Lieu, Switzerland
 * from work of Copyright (C) 2013 Simon Newton
 */

#include <errno.h>
#include <fcntl.h>
#include <linux/ioctl.h>
#include <linux/i2c/i2cdev.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include <numeric>
#include <sstream>
#include <string>
#include "ola/io/IOUtils.h"
#include "ola/Logging.h"
#include "ola/network/SocketCloser.h"
#include "plugins/i2c/I2CWriter.h"

namespace ola {
namespace plugin {
namespace i2c {

using ola::thread::MutexLocker;
using std::string;

const uint8_t I2CWriter::I2C_BITS_PER_WORD = 8;
const uint8_t I2CWriter::I2C_MODE = 0;
const char I2CWriter::I2C_DEVICE_KEY[] = "device";
const char I2CWriter::I2C_ERROR_VAR[] = "i2c-write-errors";
const char I2CWriter::I2C_WRITE_VAR[] = "i2c-writes";

I2CWriter::I2CWriter(const string &i2c_device,
                     const Options &options,
                     ExportMap *export_map)
    : m_device_path(i2c_device),
      m_i2c_speed(options.i2c_speed),
      m_cs_enable_high(options.cs_enable_high),
      m_fd(-1),
      m_error_map_var(NULL),
      m_write_map_var(NULL) {
  OLA_INFO << "Created I2C Writer " << i2c_device << " with speed "
           << options.i2c_speed << ", CE is " << m_cs_enable_high;
  if (export_map) {
    m_error_map_var = export_map->GetUIntMapVar(I2C_ERROR_VAR,
                                                I2C_DEVICE_KEY);
    (*m_error_map_var)[m_device_path] = 0;
    m_write_map_var = export_map->GetUIntMapVar(I2C_WRITE_VAR,
                                                I2C_DEVICE_KEY);
    (*m_write_map_var)[m_device_path] = 0;
  }
}

I2CWriter::~I2CWriter() {
  if (m_fd >= 0)
    close(m_fd);
}

bool I2CWriter::Init() {
  int fd;
  if (!ola::io::Open(m_device_path, O_RDWR, &fd)) {
    return false;
  }
  ola::network::SocketCloser closer(fd);

  uint8_t i2c_mode = I2C_MODE;
  if (m_cs_enable_high) {
    i2c_mode |= I2C_CS_HIGH;
  }

  if (ioctl(fd, I2C_IOC_WR_MODE, &i2c_mode) < 0) {
    OLA_WARN << "Failed to set I2C_IOC_WR_MODE for " << m_device_path;
    return false;
  }

  uint8_t i2c_bits_per_word = I2C_BITS_PER_WORD;
  if (ioctl(fd, I2C_IOC_WR_BITS_PER_WORD, &i2c_bits_per_word) < 0) {
    OLA_WARN << "Failed to set I2C_IOC_WR_BITS_PER_WORD for " << m_device_path;
    return false;
  }

  if (ioctl(fd, I2C_IOC_WR_MAX_SPEED_HZ, &m_i2c_speed) < 0) {
    OLA_WARN << "Failed to set I2C_IOC_WR_MAX_SPEED_HZ for " << m_device_path;
    return false;
  }
  m_fd = closer.Release();
  return true;
}

bool I2CWriter::WriteI2CData(const uint8_t *data, unsigned int length) {
  struct i2c_ioc_transfer i2c;
  memset(&i2c, 0, sizeof(i2c));
  i2c.tx_buf = reinterpret_cast<__u64>(data);
  i2c.len = length;

  if (m_write_map_var) {
    (*m_write_map_var)[m_device_path]++;
  }

  int bytes_written = ioctl(m_fd, I2C_IOC_MESSAGE(1), &i2c);
  if (bytes_written != static_cast<int>(length)) {
    OLA_WARN << "Failed to write all the I2C data: " << strerror(errno);
    if (m_error_map_var) {
      (*m_error_map_var)[m_device_path]++;
    }
    return false;
  }
  return true;
}
}  // namespace i2c
}  // namespace plugin
}  // namespace ola
