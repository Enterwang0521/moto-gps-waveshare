#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "preset_route_store.hpp"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
namespace moto::presets {
namespace {
uint32_t update(uint32_t crc, const uint8_t *data, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    crc ^= data[i];
    for (int j = 0; j < 8; ++j)
      crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320U : 0);
  }
  return crc;
}
} // namespace
uint32_t crc32(const uint8_t *data, size_t size) {
  return ~update(0xffffffffU, data, size);
}
Store::~Store() { abort(); }
std::string Store::path(unsigned slot, char side, bool temp) const {
  return directory_ + "/" + std::to_string(slot) + side +
         (temp ? ".tmp" : ".route");
}
void Store::abort() {
  if (file_)
    fclose(file_);
  file_ = nullptr;
}
bool Store::read(unsigned slot, char side, Header &h) {
  FILE *f = fopen(path(slot, side).c_str(), "rb");
  if (!f)
    return false;
  bool ok = fread(&h, 1, sizeof(h), f) == sizeof(h) && h.magic == 0x3152504d &&
            h.size <= 262144 &&
            h.checksum == crc32(reinterpret_cast<const uint8_t *>(&h),
                                offsetof(Header, checksum)) &&
            memchr(h.name, 0, sizeof(h.name)) &&
            ((h.token != 0 && h.size != 0 && h.name[0]) ||
             (h.token == 0 && h.size == 0));
  uint32_t crc = 0xffffffffU, remaining = ok ? h.size : 0;
  uint8_t buffer[512];
  while (ok && remaining) {
    size_t count = std::min<size_t>(remaining, sizeof(buffer));
    if (fread(buffer, 1, count, f) != count) {
      ok = false;
      break;
    }
    crc = update(crc, buffer, count);
    remaining -= count;
  }
  ok = ok && ~crc == h.crc && fgetc(f) == EOF && !ferror(f);
  fclose(f);
  return ok;
}
void Store::scan() {
  for (unsigned i = 0; i < 4; ++i) {
    Header a{}, b{};
    bool va = read(i + 1, 'a', a), vb = read(i + 1, 'b', b);
    if (vb && (!va || static_cast<int32_t>(b.generation - a.generation) > 0)) {
      headers_[i] = b;
      sides_[i] = 'b';
    } else if (va) {
      headers_[i] = a;
      sides_[i] = 'a';
    } else {
      headers_[i] = Header{};
      sides_[i] = 0;
    }
  }
}
void Store::detach() {
  std::lock_guard<std::mutex> lock(mutex_);
  abort();
  available_ = false;
  headers_ = {};
  sides_ = {};
}
bool Store::initialize() {
  std::lock_guard<std::mutex> lock(mutex_);
  struct stat st {};
  if (stat(directory_.c_str(), &st) != 0) {
    if (mkdir(directory_.c_str(), 0700) != 0)
      return false;
  } else if (!S_ISDIR(st.st_mode))
    return false;
  available_ = true;
  scan();
  return true;
}
std::array<Entry, 4> Store::entries() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::array<Entry, 4> out{};
  for (unsigned i = 0; i < 4; ++i)
    if (sides_[i] && headers_[i].token)
      out[i] = {headers_[i].token, headers_[i].name};
  return out;
}
bool Store::begin(const ble::PresetRoute &p) {
  abort();
  slot_ = p.slot;
  side_ = sides_[slot_ - 1] == 'a' ? 'b' : 'a';
  incoming_ = Header{};
  incoming_.generation = headers_[slot_ - 1].generation + 1;
  incoming_.token = p.operation == 4 ? 0 : p.token;
  incoming_.size = p.operation == 4 ? 0 : p.total_size;
  incoming_.crc = p.operation == 4 ? 0 : p.crc32;
  if (p.operation != 4)
    memcpy(incoming_.name, p.name.data(), p.name.size());
  incoming_.checksum = crc32(reinterpret_cast<const uint8_t *>(&incoming_),
                             offsetof(Header, checksum));
  file_ = fopen(path(slot_, side_, true).c_str(), "wb");
  received_ = 0;
  if (!file_)
    return false;
  if (fwrite(&incoming_, 1, sizeof(incoming_), file_) != sizeof(incoming_)) {
    abort();
    return false;
  }
  return true;
}
bool Store::commit() {
  if (!file_ || received_ != incoming_.size)
    return false;
  const bool flushed = fflush(file_) == 0 && fsync(fileno(file_)) == 0;
  const bool closed = fclose(file_) == 0;
  file_ = nullptr;
  if (!flushed || !closed)
    return false;
  // Validate the finished temporary file before exposing it. The old side stays
  // intact.
  FILE *f = fopen(path(slot_, side_, true).c_str(), "rb");
  if (!f)
    return false;
  Header stored{};
  if (fread(&stored, 1, sizeof(stored), f) != sizeof(stored) ||
      memcmp(&stored, &incoming_, sizeof(stored)) != 0) {
    fclose(f);
    return false;
  }
  uint8_t buf[512];
  uint32_t crc = 0xffffffffU, left = incoming_.size;
  bool ok = true;
  while (left) {
    size_t n = std::min<size_t>(left, sizeof(buf));
    if (fread(buf, 1, n, f) != n) {
      ok = false;
      break;
    }
    crc = update(crc, buf, n);
    left -= n;
  }
  ok = ok && ~crc == incoming_.crc && fgetc(f) == EOF && !ferror(f);
  fclose(f);
  if (!ok)
    return false;
  // FAT rename cannot overwrite a target; remove only the inactive older
  // generation.
  remove(path(slot_, side_).c_str());
  if (rename(path(slot_, side_, true).c_str(), path(slot_, side_).c_str()) != 0)
    return false;
  headers_[slot_ - 1] = incoming_;
  sides_[slot_ - 1] = side_;
  return true;
}
bool Store::consume(const ble::PresetRoute &p) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!available_ || !ble::encode_message(ble::Message{p}).ok())
    return false;
  if (p.operation == 1)
    return begin(p);
  if (p.operation == 4)
    return begin(p) && commit();
  if (!file_ || p.slot != slot_ || p.token != incoming_.token ||
      p.total_size != incoming_.size || p.crc32 != incoming_.crc)
    return false;
  if (p.operation == 2) {
    if (p.offset != received_)
      return false;
    if (fwrite(p.data.data(), 1, p.data.size(), file_) != p.data.size()) {
      abort();
      return false;
    }
    received_ += p.data.size();
    return true;
  }
  return p.operation == 3 && commit();
}
} // namespace moto::presets
