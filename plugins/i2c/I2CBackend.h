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
 * I2CBackend.h
 * The backend for I2C output. These are the classes which write the data to
 * the I2C bus.
 * Copyright (C) 2023 Olivier Baumgartner, 1345 Le Lieu, Switzerland
 * from work of Copyright (C) 2013 Simon Newton
 */

#ifndef PLUGINS_I2C_I2CBACKEND_H_
#define PLUGINS_I2C_I2CBACKEND_H_

#include <stdint.h>
#include <ola/thread/Mutex.h>
#include <ola/thread/Thread.h>
#include <string>
#include <vector>

#include "plugins/i2c/I2CWriter.h"

namespace ola {
namespace plugin {
namespace i2c {

/**
 * The interface for all I2C Backends.
 */
class I2CBackendInterface {
 public:
  virtual ~I2CBackendInterface() {}

  virtual uint8_t *Checkout(uint8_t output, unsigned int length) = 0;
  virtual uint8_t *Checkout(uint8_t output,
                            unsigned int length,
                            unsigned int latch_bytes) = 0;
  virtual void Commit(uint8_t output) = 0;

  virtual std::string DevicePath() const = 0;

  virtual bool Init() = 0;

 protected:
  static const char I2C_DROP_VAR[];
  static const char I2C_DROP_VAR_KEY[];
};


/**
 * A HardwareBackend which uses GPIO pins and an external de-multiplexer
 */
class HardwareBackend : public ola::thread::Thread,
                        public I2CBackendInterface {
 public:
  struct Options {
    // Which GPIO bits to use to select the output. The number of outputs
    // will be 2 ** gpio_pins.size();
    std::vector<uint16_t> gpio_pins;
  };

  HardwareBackend(const Options &options,
                  I2CWriterInterface *writer,
                  ExportMap *export_map);
  ~HardwareBackend();

  bool Init();

  uint8_t *Checkout(uint8_t output, unsigned int length) {
    return Checkout(output, length, 0);
  }

  uint8_t *Checkout(uint8_t output,
                    unsigned int length,
                    unsigned int latch_bytes);
  void Commit(uint8_t output);

  std::string DevicePath() const { return m_i2c_writer->DevicePath(); }

 protected:
  void* Run();

 private:
  class OutputData {
   public:
    OutputData()
        : m_data(NULL),
          m_write_pending(false),
          m_size(0),
          m_actual_size(0),
          m_latch_bytes(0) {
    }

    ~OutputData() { delete[] m_data; }

    uint8_t *Resize(unsigned int length);
    void SetLatchBytes(unsigned int latch_bytes);
    void SetPending();
    bool IsPending() const { return m_write_pending; }
    void ResetPending() { m_write_pending = false; }
    const uint8_t *GetData() const { return m_data; }
    unsigned int Size() const { return m_size; }

    OutputData& operator=(const OutputData &other);

   private:
    uint8_t *m_data;
    bool m_write_pending;
    unsigned int m_size;
    unsigned int m_actual_size;
    unsigned int m_latch_bytes;

    OutputData(const OutputData&);
  };

  typedef std::vector<int> GPIOFds;
  typedef std::vector<OutputData*> Outputs;

  I2CWriterInterface *m_i2c_writer;
  UIntMap *m_drop_map;
  const uint8_t m_output_count;
  ola::thread::Mutex m_mutex;
  ola::thread::ConditionVariable m_cond_var;
  bool m_exit;

  Outputs m_output_data;

  // GPIO members
  GPIOFds m_gpio_fds;
  const std::vector<uint16_t> m_gpio_pins;
  std::vector<bool> m_gpio_pin_state;

  void SetupOutputs(Outputs *outputs);
  void WriteOutput(uint8_t output_id, OutputData *output);
  bool SetupGPIO();
  void CloseGPIOFDs();
};


/**
 * An I2C Backend which uses a software multipliexer. This accumulates all data
 * into a single buffer and then writes it to the I2C bus.
 */
class SoftwareBackend : public I2CBackendInterface,
                        public ola::thread::Thread {
 public:
  struct Options {
    /*
     * The number of outputs.
     */
    uint8_t outputs;
    /*
     * Controls if we designate one of the outputs as the 'sync' output.
     * If set >= 0, it denotes the output which triggers the I2C write.
     * If set to -1, we perform an I2C write on each update.
     */
    int16_t sync_output;

    Options() : outputs(1), sync_output(0) {}
  };

  SoftwareBackend(const Options &options,
                  I2CWriterInterface *writer,
                  ExportMap *export_map);
  ~SoftwareBackend();

  bool Init();

  uint8_t *Checkout(uint8_t output, unsigned int length) {
    return Checkout(output, length, 0);
  }

  uint8_t *Checkout(uint8_t output,
                    unsigned int length,
                    unsigned int latch_bytes);
  void Commit(uint8_t output);

  std::string DevicePath() const { return m_i2c_writer->DevicePath(); }

 protected:
  void* Run();

 private:
  I2CWriterInterface *m_i2c_writer;
  UIntMap *m_drop_map;
  ola::thread::Mutex m_mutex;
  ola::thread::ConditionVariable m_cond_var;
  bool m_write_pending;
  bool m_exit;

  const int16_t m_sync_output;
  std::vector<unsigned int> m_output_sizes;
  std::vector<unsigned int> m_latch_bytes;
  uint8_t *m_output;
  unsigned int m_length;
};


/**
 * A fake backend used for testing. If we had gmock this would be much
 * easier...
 */
class FakeI2CBackend : public I2CBackendInterface {
 public:
  explicit FakeI2CBackend(unsigned int outputs);
  ~FakeI2CBackend();

  uint8_t *Checkout(uint8_t output, unsigned int length) {
    return Checkout(output, length, 0);
  }

  uint8_t *Checkout(uint8_t output,
                    unsigned int length,
                    unsigned int latch_bytes);

  void Commit(uint8_t output);
  const uint8_t *GetData(uint8_t output, unsigned int *length);

  std::string DevicePath() const { return "/dev/test"; }

  bool Init() { return true; }

  unsigned int Writes(uint8_t output) const;

 private:
  class Output {
   public:
    Output() : data(NULL), length(0), writes(0) {}
    ~Output() { delete[] data; }

    uint8_t *data;
    unsigned int length;
    unsigned int writes;
  };

  typedef std::vector<Output*> Outputs;
  Outputs m_outputs;
};
}  // namespace i2c
}  // namespace plugin
}  // namespace ola
#endif  // PLUGINS_I2C_I2CBACKEND_H_
