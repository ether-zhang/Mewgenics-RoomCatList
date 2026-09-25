#pragma once
#include <cstdint>
namespace roomcats {
void StartNativeCatActions(std::uintptr_t image);
void TickNativeCatActions(std::uintptr_t scene);
void StopNativeCatActions();
}
