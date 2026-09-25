#include "localization.hpp"
#include "newborn_batch.hpp"
#include "cat_moves.hpp"
#include "cat_actions.hpp"
#include <algorithm>
#include <mutex>

namespace roomcats {
namespace {
enum class Kind { Mark, Move, Donate };
struct Command { Kind kind; std::uint64_t cat; std::string value; int npc=-1; };
std::mutex lock;
NewbornBatchStatus status;
RoomSnapshot expected;
std::vector<Command> commands;
bool waiting=false, result_ready=false, stop_requested=false;
std::uint64_t started=0;
std::uint64_t readiness_started=0;
const Cat* Find(const RoomSnapshot& s,std::uint64_t id) {
    const auto it=std::find_if(s.cats.begin(),s.cats.end(),[id](const Cat& c) { return c.id==id; });
    return it==s.cats.end() ? nullptr : &*it;
}
void Fail(const std::string& message) {
    if(waiting) {
        if(commands[status.completed].kind==Kind::Move) CancelCatMove();
        else CancelCatAction();
    }
    status.running=false; status.stopped=true; status.message=message+roomcats::Tx("；剩余操作未执行");
}
}

bool NewbornBatchActive() { std::lock_guard<std::mutex> guard(lock); return status.running; }
bool NewbornBatchNeedsCats() {
    std::lock_guard<std::mutex> guard(lock);
    return status.running && (!waiting || result_ready ||
        (commands[status.completed].kind==Kind::Move ? !HasPendingCatMove() : !HasPendingCatAction()));
}
NewbornBatchStatus GetNewbornBatchStatus() { std::lock_guard<std::mutex> guard(lock); return status; }
std::string NewbornBatchDiagnostic() {
    std::lock_guard<std::mutex> guard(lock);
    auto message=status.message;
    if(const auto* cat=Find(expected,status.cat); cat && !cat->name.empty()) {
        const auto id="#"+std::to_string(cat->id);
        for(auto at=message.find(cat->name); at!=std::string::npos; at=message.find(cat->name,at+id.size()))
            message.replace(at,cat->name.size(),id);
    }
    return message;
}
bool BeginNewbornBatch(const NewbornPlan& plan,const RoomSnapshot& fresh,std::string& reason) {
    std::lock_guard<std::mutex> guard(lock);
    if(status.running || HasPendingCatMove() || HasPendingCatAction()) { reason=roomcats::Tx("请先等待当前操作完成"); return false; }
    if(!ValidateNewbornPlan(plan,fresh,reason)) return false;
    std::vector<Command> marks,moves,donations;
    for(const auto& d:plan.decisions) {
        if(ProtectedCat(d.cat) || (plan.kind==ScreeningKind::Adult && d.cat.details.age<=1)) continue;
        if(!d.marker.empty() && d.marker!=d.cat.details.marker) marks.push_back({Kind::Mark,d.cat.id,d.marker});
        std::string destination=d.destination;
        int npc=-1;
        if(d.discard) {
            if(d.choice==ReviewChoice::Keep) destination=d.cat.location_key;
            else if(d.choice==ReviewChoice::Location) destination=d.alternative_location;
            else { destination=d.cat.location_key; npc=d.choice==ReviewChoice::Npc ? d.npc : 8; }
        }
        if(destination!=d.cat.location_key) {
            const auto choices=CatMoveChoices(fresh,d.cat.id);
            if(std::none_of(choices.begin(),choices.end(),[&](const MoveChoice& c) { return c.destination.key==destination && c.enabled; })) {
                reason=d.cat.name+roomcats::Tx(" 的目标房间或箱子目前不可用"); return false;
            }
            moves.push_back({Kind::Move,d.cat.id,destination});
        }
        if(npc>=0) {
            const auto choices=CatNpcChoices(fresh,d.cat.id);
            const auto choice=std::find_if(choices.begin(),choices.end(),[npc](const NpcChoice& c) { return c.npc==npc; });
            if(choice==choices.end() || !choice->enabled) {
                reason=d.cat.name+"："+(choice==choices.end() ? roomcats::Tx("原投送去向目前不可用") : choice->reason)+roomcats::Tx("；已保留原选择，请等待或修改后再确认"); return false;
            }
            donations.push_back({Kind::Donate,d.cat.id,{},npc});
        }
    }
    commands=std::move(marks); commands.insert(commands.end(),moves.begin(),moves.end()); commands.insert(commands.end(),donations.begin(),donations.end());
    MoveResult old_move; TakeCatMoveResult(old_move);
    CatActionResult old_action; TakeCatActionResult(old_action);
    expected=fresh; waiting=result_ready=stop_requested=false; readiness_started=0; status={};
    status.kind=plan.kind;
    status.rules_fingerprint=plan.kind==ScreeningKind::Adult ? RuleFingerprint(plan.adult_rules) : RuleFingerprint(plan.newborn_rules);
    status.total=commands.size(); status.running=!commands.empty(); status.finished=commands.empty();
    status.message=status.running ? roomcats::Tx("已确认，开始处理") : roomcats::Tx("无需调整");
    if(status.finished) status.post_fingerprint=ScreeningFingerprint(fresh,status.kind);
    return true;
}

void StopNewbornBatch() {
    std::lock_guard<std::mutex> guard(lock);
    if(!status.running) return;
    stop_requested=true;
    status.message=roomcats::Tx("当前操作结束后停止");
    if(!waiting) Fail(roomcats::Tx("已停止处理"));
}

void TickNewbornBatch(const RoomSnapshot& fresh,std::uint64_t now) {
    std::lock_guard<std::mutex> guard(lock);
    if(!status.running) return;
    if(fresh.scene!=expected.scene || fresh.generation!=expected.generation || fresh.game_day!=expected.game_day) { Fail(roomcats::Tx("家园或日期已变化，已停止处理")); return; }
    auto& command=commands[status.completed];
    if(waiting) {
        if(command.kind==Kind::Move) {
            MoveResult r;
            if(TakeCatMoveResult(r)) {
                if(r.cat!=command.cat || !r.success) { Fail(r.message); return; }
                result_ready=true;
            }
        } else {
            CatActionResult r;
            if(TakeCatActionResult(r)) {
                if(r.cat!=command.cat || !r.success) { Fail(r.message); return; }
                if(r.completed) result_ready=true;
                status.message=r.message;
            }
        }
    }
    if(fresh.suspended) { started=now; readiness_started=0; status.message=roomcats::Tx("等待返回家园后继续"); return; }
    if(!fresh.valid || !fresh.in_house) { Fail(roomcats::Tx("家园数据暂不可用")); return; }
    if(status.kind==ScreeningKind::Adult && (!waiting || result_ready) && !fresh.population_comfort_valid) {
        if(!readiness_started) readiness_started=now;
        status.message=roomcats::Tx("等待房间舒适度数据稳定");
        if(now-readiness_started>15000) Fail(roomcats::Tx("房间舒适度数据未就绪"));
        return;
    }
    if(waiting) {
        if(now-started>40000) { Fail(roomcats::Tx("当前操作等待超时，请查看原生界面")); return; }
        if(!result_ready) return;
        const auto* actual=Find(fresh,command.cat);
        const auto* before=Find(expected,command.cat);
        bool verified=false;
        if(command.kind==Kind::Donate) verified=!actual || !actual->active || actual->record_only;
        else if(actual && before && actual->active && actual->component==before->component && actual->generation==before->generation)
            verified=command.kind==Kind::Move ? actual->location_key==command.value : actual->details.marker==command.value;
        if(!verified) return;
        for(auto& cat:expected.cats) if(cat.id==command.cat) {
            if(command.kind==Kind::Donate) cat.active=false;
            else if(command.kind==Kind::Mark) cat.details.marker=command.value;
            else {
                cat.location_key=command.value;
                for(const auto& location:fresh.locations) if(location.key==command.value) { cat.location=location.component; cat.location_label=location.label; }
            }
        }
        ++status.completed; waiting=result_ready=false;
        if(command.kind==Kind::Donate)
            for(auto& cat:expected.cats) if(const auto* actual=Find(fresh,cat.id)) cat.parents=actual->parents;
        if(status.completed==commands.size()) {
            status.running=false; status.finished=true;
            status.message=status.kind==ScreeningKind::Adult ? roomcats::Tx("老猫数量控制处理完成") : roomcats::Tx("新生猫筛选处理完成");
            status.post_fingerprint=ScreeningFingerprint(fresh,status.kind); return;
        }
        if(stop_requested) { Fail(roomcats::Tx("已停止处理")); return; }
    }
    if(ScreeningFingerprint(fresh,status.kind)!=ScreeningFingerprint(expected,status.kind)) { Fail(roomcats::Tx("执行期间猫咪资料、位置或家具被改变，已停止处理")); return; }
    const auto& next=commands[status.completed];
    status.cat=next.cat;
    const auto* cat=Find(fresh,next.cat);
    if(!cat || cat->record_only || !cat->active) { Fail(roomcats::Tx("待处理猫咪已不可用")); return; }
    if(next.kind==Kind::Donate) {
        const auto choices=CatNpcChoices(fresh,next.cat);
        const auto choice=std::find_if(choices.begin(),choices.end(),[&](const NpcChoice& c) { return c.npc==next.npc; });
        if(choice!=choices.end() && !choice->enabled && choice->transient) {
            if(!readiness_started) readiness_started=now;
            status.cat=next.cat; status.message=choice->reason+"："+cat->name;
            if(now-readiness_started>15000) Fail(roomcats::Tx("投送管道或界面尚未复位，请查看游戏界面"));
            return;
        }
        if(choice==choices.end() || !choice->enabled) {
            Fail(cat->name+"："+(choice==choices.end() ? roomcats::Tx("投送去向不可用") : choice->reason)); return;
        }
    }
    readiness_started=0;
    const bool queued=next.kind==Kind::Move ? QueueCatMove(fresh,next.cat,next.value) :
        QueueCatAction(fresh,next.cat,next.kind==Kind::Mark ? CatActionKind::Mark : CatActionKind::Donate,next.npc,next.value);
    if(!queued) { Fail(cat->name+roomcats::Tx(" 的操作当前不可执行")); return; }
    status.cat=next.cat; status.message=roomcats::Tx("正在处理 ")+cat->name;
    waiting=true; started=now;
}
}
