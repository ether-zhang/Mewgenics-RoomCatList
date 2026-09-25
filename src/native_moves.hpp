#pragma once
#include <cstdint>

namespace roomcats {
void StartNativeCatMoves(std::uintptr_t image);
void TickNativeCatMoves(std::uintptr_t scene);
void StopNativeCatMoves();
}
