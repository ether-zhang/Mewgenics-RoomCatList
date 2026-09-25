#pragma once
#include "newborn_plan.hpp"
namespace roomcats {
struct NewbornBatchStatus {
    bool running=false, finished=false, stopped=false;
    std::size_t completed=0,total=0;
    std::uint64_t cat=0, post_fingerprint=0;
    std::string message;
    ScreeningKind kind=ScreeningKind::Newborn;
    std::uint64_t rules_fingerprint=0;
};
bool BeginNewbornBatch(const NewbornPlan&,const RoomSnapshot& fresh,std::string& reason);
bool NewbornBatchActive();
bool NewbornBatchNeedsCats();
NewbornBatchStatus GetNewbornBatchStatus();
std::string NewbornBatchDiagnostic();
void TickNewbornBatch(const RoomSnapshot& fresh,std::uint64_t now);
void StopNewbornBatch();
}
