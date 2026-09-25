#include "../src/game_reader.hpp"
// These readers cannot access memory or a game. Metadata is supplied by each
// test fixture; accidental use of a native reader fails instead of touching it.
namespace roomcats {
bool ReadBytes(std::uintptr_t, void*, std::size_t) noexcept { return false; }
bool ReadNarrowString(std::uintptr_t, std::string&) { return false; }
bool ReadWideString(std::uintptr_t, std::string&) { return false; }
}
