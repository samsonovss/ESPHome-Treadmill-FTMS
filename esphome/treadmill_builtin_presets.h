// GENERATED from canonical web builtins by tools/nextion-history/generate-builtins.cjs; do not edit.
#pragma once
#include <cmath>
namespace treadmill_builtin_presets {
struct Preset {const char *kind,*name,*plan,*wmode,*cmode;int hi,lo,cycles;float warm,cool,low,high;bool manual,hasWC;};
inline const Preset items[]={
 {"hiit","30/30 × 10","","Time","Time",30,30,10,5,5,5,11,true,true},
 {"hiit","1/1 × 8","","Time","Time",60,60,8,5,5,5,10,true,true},
 {"hiit","4×4 мин","","Time","Time",240,180,4,5,5,5.5,9.5,true,true},
 {"fartlek","Шведский 30 мин","300/6.0,60/11.0,120/7.0,30/13.0,90/7.0,180/10.0,120/7.0,20/14.0,120/7.0,90/11.5,120/7.0,45/12.5,105/7.0,300/6.0","","",60,60,8,3,3,NAN,NAN,false,false},
 {"fartlek","Пирамида","300/6.0,30/12.0,60/7.0,60/12.0,60/7.0,90/12.0,90/7.0,120/12.0,120/7.0,90/12.0,90/7.0,60/12.0,60/7.0,30/12.0,300/6.0","","",60,60,8,3,3,NAN,NAN,false,false},
 {"fartlek","Короткие рывки","300/6.0,20/14.0,100/7.0,20/14.0,100/7.0,30/13.0,90/7.0,20/14.0,100/7.0,30/13.0,90/7.0,20/14.0,100/7.0,300/6.0","","",60,60,8,3,3,NAN,NAN,false,false}
};
}
