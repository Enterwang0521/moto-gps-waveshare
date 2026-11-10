#pragma once
#include "moto/ble_protocol/ble_protocol.hpp"
#include <array>
#include <cstdio>
#include <mutex>
#include <string>
namespace moto::presets {
struct Entry {
  uint32_t token = 0;
  std::string name;
};
uint32_t crc32(const uint8_t *data, size_t size);
// Four A/B slots: a failed import never replaces the last committed generation.
class Store {
public:
  explicit Store(std::string directory) : directory_(std::move(directory)) {}
  ~Store();
  bool initialize();
  void detach();
  std::array<Entry, 4> entries();
  bool consume(const ble::PresetRoute &packet);
  bool available() const { return available_; }

private:
  struct Header {
    uint32_t magic = 0x3152504d, generation = 0, token = 0, size = 0, crc = 0;
    char name[64]{};
    uint32_t checksum = 0;
  };
  bool read(unsigned slot, char side, Header &h);
  void scan();
  bool begin(const ble::PresetRoute &packet);
  bool commit();
  void abort();
  std::string path(unsigned slot, char side, bool temporary = false) const;
  std::string directory_;
  std::mutex mutex_;
  bool available_ = false;
  std::array<Header, 4> headers_{};
  std::array<char, 4> sides_{{0, 0, 0, 0}};
  FILE *file_ = nullptr;
  Header incoming_{};
  unsigned slot_ = 0;
  char side_ = 0;
  uint32_t received_ = 0;
};
} // namespace moto::presets
