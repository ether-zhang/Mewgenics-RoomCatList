// Headless preview of production DrawUi, using its real native font/art atlas.
// No window, graphics device, process hooks or game data are accessed.
#include "ui_test_host.hpp"
#include "newborn_test_fixture.hpp"
#include <filesystem>
#include <fstream>
#include <cmath>

namespace {
float Edge(ImVec2 a,ImVec2 b,float x,float y) { return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x); }
bool Inclusive(ImVec2 a,ImVec2 b) { return b.y<a.y || (b.y==a.y && b.x>a.x); }
void ExportImage(const std::filesystem::path& file) {
    const auto* data=ImGui::GetDrawData();
    const int width=int(data->DisplaySize.x),height=int(data->DisplaySize.y);
    unsigned char* atlas=nullptr; int tw=0,th=0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&atlas,&tw,&th);
    std::vector<unsigned char> pixels(width*height*3);
    for(int i=0;i<width*height;++i) { pixels[i*3]=74;pixels[i*3+1]=76;pixels[i*3+2]=71; }
    for(const auto* list:data->CmdLists) for(const auto& cmd:list->CmdBuffer) {
        if(cmd.UserCallback) continue;
        for(unsigned index=cmd.IdxOffset;index<cmd.IdxOffset+cmd.ElemCount;index+=3) {
            auto a=list->VtxBuffer[cmd.VtxOffset+list->IdxBuffer[index]];
            auto b=list->VtxBuffer[cmd.VtxOffset+list->IdxBuffer[index+1]];
            auto c=list->VtxBuffer[cmd.VtxOffset+list->IdxBuffer[index+2]];
            float area=Edge(a.pos,b.pos,c.pos.x,c.pos.y);
            if(std::abs(area)<1e-6f) continue;
            if(area<0) { std::swap(b,c);area=-area; }
            const int x0=std::max({0,int(std::floor(std::min({a.pos.x,b.pos.x,c.pos.x}))),int(std::ceil(cmd.ClipRect.x))});
            const int y0=std::max({0,int(std::floor(std::min({a.pos.y,b.pos.y,c.pos.y}))),int(std::ceil(cmd.ClipRect.y))});
            const int x1=std::min({width,int(std::ceil(std::max({a.pos.x,b.pos.x,c.pos.x}))),int(cmd.ClipRect.z)});
            const int y1=std::min({height,int(std::ceil(std::max({a.pos.y,b.pos.y,c.pos.y}))),int(cmd.ClipRect.w)});
            for(int y=y0;y<y1;++y) for(int x=x0;x<x1;++x) {
                const float e0=Edge(b.pos,c.pos,x+.5f,y+.5f),e1=Edge(c.pos,a.pos,x+.5f,y+.5f),e2=Edge(a.pos,b.pos,x+.5f,y+.5f);
                if(e0<0 || e1<0 || e2<0 || (e0==0 && !Inclusive(b.pos,c.pos)) || (e1==0 && !Inclusive(c.pos,a.pos)) || (e2==0 && !Inclusive(a.pos,b.pos))) continue;
                const float wa=e0/area,wb=e1/area,wc=e2/area;
                const float u=std::clamp((a.uv.x*wa+b.uv.x*wb+c.uv.x*wc)*tw-.5f,0.0f,float(tw-1));
                const float v=std::clamp((a.uv.y*wa+b.uv.y*wb+c.uv.y*wc)*th-.5f,0.0f,float(th-1));
                const int tx=int(u),ty=int(v),rx=std::min(tx+1,tw-1),by=std::min(ty+1,th-1);
                float color[4]{};
                for(int k=0;k<4;++k) {
                    const float top=atlas[(ty*tw+tx)*4+k]*(1-(u-tx))+atlas[(ty*tw+rx)*4+k]*(u-tx);
                    const float bottom=atlas[(by*tw+tx)*4+k]*(1-(u-tx))+atlas[(by*tw+rx)*4+k]*(u-tx);
                    color[k]=(top*(1-(v-ty))+bottom*(v-ty))*(((a.col>>(k*8))&255)*wa+((b.col>>(k*8))&255)*wb+((c.col>>(k*8))&255)*wc)/255;
                }
                auto* dst=pixels.data()+(y*width+x)*3;
                for(int k=0;k<3;++k) dst[k]=static_cast<unsigned char>(std::clamp(color[k]*(color[3]/255)+dst[k]*(1-color[3]/255),0.0f,255.0f));
            }
        }
    }
    std::ofstream out(file,std::ios::binary); out<<"P6\n"<<width<<" "<<height<<"\n255\n";
    out.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());
}
void Frame() {
    ImGui::NewFrame(); DrawUi(); PublishMouseCapture(); ImGui::Render();g_mouse.EndFrame();
}
}
int MakeUiThemePreview(bool chinese) {
    using namespace roomcats; using namespace newborn_test;
    SetLanguage(chinese ? Language::Chinese : Language::English);
    const char* suffix=chinese ? "cn" : "en";
    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path();
    ImGui::CreateContext(); ui::Apply(); RegisterScreeningRulesSettings();
    auto& io=ImGui::GetIO(); io.IniFilename=io.LogFilename=nullptr;io.DisplaySize={1960,1100};io.DeltaTime=1.0f/60;
    ImFontGlyphRangesBuilder ranges; ranges.AddRanges(io.Fonts->GetGlyphRangesDefault());
    ranges.AddText("。，：；（）“”≥≤！");
    ranges.AddText("简体中文 语言");
    ranges.AddText("猫咪列表全屋猫咪只名字职业位置送至选择去向序号父母年龄性取繁总实遗力量敏捷体质智力速度魅力幸运好突变坏突变疾病");
    ranges.AddText("桑葚乐乐清歌肥肥卢锡安百合西奥胖墩煤球米莉森雷恩加尔蔚卡蜜尔哈娜蓝莓顶楼阁楼一楼左右二楼左右箱子真言男母公异同无轻中重");
    ranges.AddText("筛选设置新生猫筛选老猫数量控制新生猫老猫确定处理返回名单保存并关闭恢复默认取消调房遗弃原地保留打标优质变异");
    ranges.AddText("排序属性有无职业房子标记始终保护小猫老猫修改只影响新生成的方案保存后仍须复查并确认数值可直接输入也可点击加减按钮二楼每房最多可设对面出生数量需小于等于每房上限滚轮上下左右点击表头排序关闭未知全部突变固定属性合计总计条件效果见左侧身体突变腿部缺陷速度减少天生敏捷减少严重影响行动");
    VisitScreeningRules(g_screening_rules,[&](const auto&,const char* label,const auto&,int,int) { ranges.AddText(label); });
    ImVector<ImWchar> glyphs; ranges.BuildRanges(&glyphs);
    auto* font=io.Fonts->AddFontFromFileTTF((root/"build/assets/native-ui.ttf").string().c_str(),22,nullptr,glyphs.Data);
    CHECK_NEWBORN(font);BuildMarkerGlyphs(*io.Fonts,font);io.Fonts->SetTexID(1);
    CHECK_NEWBORN(font->FindGlyphNoFallback(0x732B) && font->FindGlyphNoFallback(0x7B5B));
    CHECK_NEWBORN(StatValueColor(7,7).x<.1f && StatValueColor(10,7).y>StatValueColor(7,7).y && StatValueColor(3,7).x>StatValueColor(7,7).x);
    g_snapshot=House();g_snapshot.room=0;g_has_house=g_open=true;
    for(auto& r:g_snapshot.locations) if(r.kind==LocationKind::Room) r.label=RoomLabel(r.key.substr(5));
    const char* names[]={"桑葚","乐乐","清歌","肥肥","卢锡安","百合","西奥","胖墩","煤球","米莉森","雷恩加尔","蔚"};
    const char* collars[]={"Colorless","Fighter","Mage","Hunter","Tank","Thief","Cleric","Necromancer","Psychic","Druid","Jester","Monk"};
    for(int i=0;i<12;++i) {
        auto c=MakeCat(i+1,i%5,45+i%5,52+i,1+i*2,i%3);
        c.name=names[i];c.details.collar=collars[i];c.details.sex=i%2;c.details.sexuality=i%3 ? 0 : 1;
        c.details.marker=i%3==0 ? "triangle" : i%3==1 ? "square" : "circle";
        c.location_label=g_snapshot.locations[i%5].label;c.parents={true,{90,91},{"真言","蓝莓"}};
        c.details.good={{"身体突变","力量 +1\n速度 +2",{},true,"Body Mutation","Strength +1\nSpeed +2"}};
        if(i%3==0) c.details.bad={{"腿部缺陷","速度 -1\n天生敏捷减少，影响行动。",{},false,"Leg Defect","Speed -1\nLower innate dexterity."}};
        g_snapshot.cats.push_back(c);
    }
    Frame();ImGui::SetWindowPos("猫咪列表###RoomCatsPanel",{34,32});ImGui::SetWindowSize("猫咪列表###RoomCatsPanel",{1890,1024});
    for(int i=0;i<4;++i) Frame();
    ExportImage(root/(std::string("build/ui-theme-list-")+suffix+".ppm"));
    auto* panel=ImGui::FindWindowByName("猫咪列表###RoomCatsPanel");const auto& style=ImGui::GetStyle();
    auto* table=GImGui->Tables.GetByKey(ImHashStr("RoomCatsDetailsRowsV6",0,panel->ID));
    CHECK_NEWBORN(table);
    const float hx=table->Columns[BadMutations].WorkMinX+12;
    const float hy=table->OuterRect.Min.y+ImGui::GetTextLineHeight()*2+4+style.CellPadding.y*2+8;
    g_mouse.Move(int(hx),int(hy));io.AddMousePosEvent(hx,hy);
    for(int i=0;i<4;++i) Frame();
    CHECK_NEWBORN(g_trait_hover.cat==1 && g_trait_hover.group==1);
    ExportImage(root/(std::string("build/ui-theme-traits-")+suffix+".ppm"));
    const auto width=[&](const char* label) { return ImGui::CalcTextSize(label).x+style.FramePadding.x*2; };
    const float x=panel->ContentRegionRect.Max.x-width(Tx("新生猫筛选"))-width(Tx("老猫数量控制"))-
        style.ItemSpacing.x*2-width(Tx("筛选设置"))*.5f;
    const float y=panel->WorkRect.Min.y+ImGui::GetFrameHeight()*.5f;
    g_mouse.Move(int(x),int(y));io.AddMousePosEvent(x,y);Frame();
    io.AddMouseButtonEvent(0,true);Frame();io.AddMouseButtonEvent(0,false);
    for(int i=0;i<15;++i) Frame();
    CHECK_NEWBORN(g_rules_popup_drawn);
    ExportImage(root/(std::string("build/ui-theme-settings-")+suffix+".ppm"));
    ui::Reset();ImGui::DestroyContext();
    std::cout<<"Native theme previews rendered from production UI, using extracted paper, button states and TikaFont/TikaFontCN.\n";
    return 0;
}
