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
 * FakeI2CWriter.h
 * The I2CWriter used for testing.
 * Copyright (C) 2023 Olivier Baumgartner, 1345 Le Lieu, Switzerland
 * from work of Copyright (C) 2013 Simon Newton
 */

#ifndef PLUGINS_I2C_FAKEI2CWRITER_H_
#define PLUGINS_I2C_FAKEI2CWRITER_H_

#include <ola/testing/TestUtils.h>
#include <ola/thread/Mutex.h>
#include <stdint.h>
#include <string>
#include "plugins/i2c/I2CWriter.h"

namespace ola {
namespace plugin {
namespace i2c {

/**
 * A Fake I2C Writer used for testing
 */
class FakeI2CWriter : public I2CWriterInterface {
 public:
  explicit FakeI2CWriter(const std::string &device_path)
    : m_device_path(device_path),
      m_write_pending(0),
      m_writes(0),
      m_last_write_size(0),
      m_data(NULL) {
  }

  ~FakeI2CWriter() {
    delete[] m_data;
  }

  bool Init() { return true; }

  std::string DevicePath() const { return m_device_path; }

  bool WriteI2CData(const uint8_t *data, unsigned int length);

  // Methods used for testing
  void BlockWriter();
  void UnblockWriter();

  void ResetWrite();
  void WaitForWrite();

  unsigned int WriteCount() const;
  unsigned int LastWriteSize() const;
  void CheckDataMatches(const ola::testing::SourceLine &source_line,
                        const uint8_t *data,
                        unsigned int length);

 private:
  const std::string m_device_path;
  bool m_write_pending;  // GUARDED_BY(m_mutex)
  unsigned int m_writes;  // GUARDED_BY(m_mutex)
  unsigned int m_last_write_size;  // GUARDED_BY(m_mutex)
  uint8_t *m_data;  // GUARDED_BY(m_mutex)

  ola::thread::Mutex m_write_lock;
  mutable ola::thread::Mutex m_mutex;
  ola::thread::ConditionVariable m_cond_var;
};

}  // namespace i2c
}  // namespace plugin
}  // namespace ola
#endif  // PLUGINS_I2C_FAKEI2CWRITER_H_
