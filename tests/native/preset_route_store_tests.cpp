#include "preset_routes/preset_route_store.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
using moto::ble::PresetRoute;
using moto::presets::Store;
static PresetRoute start(unsigned token, const std::string &payload) {
  PresetRoute p;
  p.operation = 1;
  p.slot = 1;
  p.token = token;
  p.name = "Commute";
  p.total_size = payload.size();
  p.crc32 = moto::presets::crc32(
      reinterpret_cast<const uint8_t *>(payload.data()), payload.size());
  return p;
}
static bool upload(Store &store, PresetRoute p, const std::string &payload) {
  if (!store.consume(p))
    return false;
  p.operation = 2;
  p.data.assign(payload.begin(), payload.end());
  if (!store.consume(p))
    return false;
  p.operation = 3;
  p.offset = p.total_size;
  p.data.clear();
  return store.consume(p);
}
int main() {
  const std::string vector = "123456789";
  assert(moto::presets::crc32(reinterpret_cast<const uint8_t *>(vector.data()),
                              vector.size()) == 0xcbf43926U);
  const auto dir =
      std::filesystem::temp_directory_path() /
      ("moto-preset-test-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directory(dir);
  {
    Store s((dir / "routes").string());
    assert(s.initialize());
    auto p = start(100, vector);
    assert(upload(s, p, vector));
    assert(s.entries()[0].token == 100);
    auto bad = start(200, vector);
    bad.crc32 ^= 1;
    assert(!upload(s, bad, vector));
    assert(s.entries()[0].token == 100);
    auto incomplete = start(300, vector);
    assert(s.consume(incomplete));
    incomplete.operation = 2;
    incomplete.offset = 1;
    incomplete.data = {1};
    assert(!s.consume(incomplete));
  }
  {
    Store s((dir / "routes").string());
    assert(s.initialize());
    assert(s.entries()[0].token == 100);
    assert(upload(s, start(400, vector), vector));
    assert(s.entries()[0].token == 400);
  }
  // Corruption of the newest generation falls back to the older valid copy.
  {
    std::fstream f(dir / "routes/1b.route",
                   std::ios::in | std::ios::out | std::ios::binary);
    f.seekp(-1, std::ios::end);
    f.put('x');
  }
  {
    Store s((dir / "routes").string());
    assert(s.initialize());
    assert(s.entries()[0].token == 100);
    PresetRoute remove;
    remove.operation = 4;
    remove.slot = 1;
    assert(s.consume(remove));
    assert(s.entries()[0].token == 0);
  }
  {
    Store s((dir / "routes").string());
    assert(s.initialize());
    assert(s.entries()[0].token == 0);
  }
  std::filesystem::remove_all(dir);
  std::cout << "Preset persistence, CRC, interrupted import and A/B recovery "
               "passed\n";
}
