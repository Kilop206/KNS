#include "DeviceIcons.hpp"
#include <algorithm>
#include <cmath>

namespace gui {
const char* deviceDisplayName(kns::DeviceType type) {
    constexpr const char* names[] = {"Device", "Computer", "Router", "Switch", "Access point",
        "Server", "Phone", "Printer", "IoT", "Network segment"};
    return names[std::clamp(static_cast<int>(type), 0, 9)];
}

ImU32 deviceColor(kns::DeviceType type) {
    constexpr ImU32 colors[] = {IM_COL32(141,157,180,255), IM_COL32(73,162,248,255),
        IM_COL32(63,181,195,255), IM_COL32(81,193,150,255), IM_COL32(167,132,245,255),
        IM_COL32(111,148,223,255), IM_COL32(231,137,181,255), IM_COL32(233,175,94,255),
        IM_COL32(172,195,96,255), IM_COL32(138,160,181,255)};
    return colors[std::clamp(static_cast<int>(type), 0, 9)];
}

void drawDeviceIcon(ImDrawList* d, kns::DeviceType type, ImVec2 c, float size) {
    const float s = size / 64.0f;
    const ImU32 color = deviceColor(type), ink = IM_COL32(235,245,255,255), dark = IM_COL32(25,39,57,255);
    auto p = [&](float x, float y) { return ImVec2(c.x + x*s, c.y + y*s); };
    auto line = [&](float x, float y, float xx, float yy, ImU32 col = IM_COL32(235,245,255,255)) {
        d->AddLine(p(x,y), p(xx,yy), col, std::max(1.0f, 2*s));
    };
    auto box = [&](float x, float y, float xx, float yy, ImU32 col, float round = 3) {
        d->AddRectFilled(p(x,y), p(xx,yy), col, round*s);
    };
    switch (type) {
    case kns::DeviceType::Computer:
        box(-25,-22,25,13,color); box(-21,-18,21,8,dark,1);
        box(-4,13,4,20,color,0); box(-15,20,15,24,color,2);
        line(-17,-13,1,-13); line(-17,-7,-6,-7);
        break;
    case kns::DeviceType::Router:
        d->AddEllipseFilled(p(0,8), ImVec2(27*s,15*s), color);
        box(-27,-7,27,8,color,0);
        d->AddEllipseFilled(p(0,-7), ImVec2(27*s,15*s), dark);
        d->AddEllipse(p(0,-7), ImVec2(27*s,15*s), color, 0, 0, 2*s);
        line(-18,-7,-4,-7); line(-18,-7,-13,-11); line(-18,-7,-13,-3);
        line(4,-7,18,-7); line(18,-7,13,-11); line(18,-7,13,-3);
        line(0,-17,0,3); line(0,-17,-4,-12); line(0,3,4,-2);
        break;
    case kns::DeviceType::Switch:
        box(-29,-15,29,17,color,4); box(-25,-10,25,12,dark,2);
        for (int i=0;i<6;++i) { box(-21+i*7,-5,-16+i*7,3,ink,0); box(-21+i*7,6,-18+i*7,8,color,0); }
        break;
    case kns::DeviceType::AccessPoint:
        box(-23,7,23,22,color,5); line(-17,7,-17,-6,color); line(17,7,17,-6,color);
        d->AddCircleFilled(p(0,4), 3*s, ink);
        for (int i=0;i<3;++i) {
            d->PathArcTo(p(0,4), (10+i*8)*s, 3.8f, 5.63f, 16);
            d->PathStroke(color, 0, 2.5f*s);
        }
        break;
    case kns::DeviceType::Server:
        box(-19,-27,19,27,color,3);
        for (int i=0;i<3;++i) {
            box(-15,-22+i*16,15,-9+i*16,dark,2);
            d->AddCircleFilled(p(-9,-15+i*16),2*s,IM_COL32(99,233,175,255));
            line(-3,-15+i*16,10,-15+i*16);
        }
        break;
    case kns::DeviceType::Phone:
        box(-15,-27,15,27,color,6); box(-11,-21,11,18,dark,2);
        line(-4,-23,4,-23); d->AddCircleFilled(p(0,22),2*s,ink);
        break;
    case kns::DeviceType::Printer:
        box(-17,-25,17,-5,ink,1); box(-26,-10,26,16,color,4);
        box(-18,5,18,12,dark,1); box(-15,9,15,26,ink,1);
        line(-10,15,10,15,dark); line(-10,20,5,20,dark);
        d->AddCircleFilled(p(18,-3),2*s,dark);
        break;
    case kns::DeviceType::IoT:
        box(-17,-17,17,17,color,3); box(-10,-10,10,10,dark,2);
        for (int i=-12;i<=12;i+=8) {
            line(i,-24,i,-17,color); line(i,17,i,24,color);
            line(-24,i,-17,i,color); line(17,i,24,i,color);
        }
        d->AddCircleFilled(p(0,0),4*s,ink);
        break;
    case kns::DeviceType::NetworkSegment:
        line(-25,0,25,0,color); line(0,-20,0,0,color);
        line(-20,0,-20,18,color); line(20,0,20,18,color);
        box(-7,-27,7,-14,color,2); box(-27,14,-13,27,color,2); box(13,14,27,27,color,2);
        break;
    default:
        box(-22,-22,22,22,color,6); box(-16,-16,16,16,dark,3);
        line(-5,-7,5,-7); line(5,-7,5,0); line(5,0,0,3);
        d->AddCircleFilled(p(0,10),2*s,ink);
        break;
    }
}
}
