#include "../esphome/profile_core.h"
#include <cassert>
using namespace treadmill_profiles;
int main(){
 Profile p;p.id="p1";p.name="User";assert(valid(p));assert(valid_store({p},"p1"));
 assert(!valid_store({},"p1"));assert(!valid_store({p,p},"p1"));assert(!valid_store({p},"missing"));
 for(auto v:{15,101}){auto q=p;q.age=v;assert(!valid(q));}
 for(auto v:{39,101}){auto q=p;q.weight=v;assert(!valid(q));}
 for(auto v:{119,231}){auto q=p;q.hr_max_manual=v;assert(!valid(q));}
 for(auto v:{29,111}){auto q=p;q.resting_heart_rate=v;assert(!valid(q));}
 for(auto v:{16,100}){auto q=p;q.age=v;assert(valid(q));}
 for(auto v:{40,100}){auto q=p;q.weight=v;assert(valid(q));}
 for(auto v:{120,230}){auto q=p;q.hr_max_manual=v;assert(valid(q));}
 for(auto v:{30,110}){auto q=p;q.resting_heart_rate=v;assert(valid(q));}
 auto empty=p;empty.name="";assert(!valid(empty));empty=p;empty.name=std::string(61,'a');assert(!valid(empty));
 auto q=p;q.id="../x";assert(!valid(q));q=p;q.name="bad\nname";assert(!valid(q));
 q=p;q.gender="Other";assert(!valid(q));q=p;q.hr_zone_method="Other";assert(!valid(q));
 q=p;q.name="Renamed";assert(same_settings(p,q));q.age++;assert(!same_settings(p,q));
 std::vector<Profile> all;for(int i=0;i<8;i++){q=p;q.id="p"+std::to_string(i);all.push_back(q);}assert(valid_store(all,"p0"));q.id="p8";all.push_back(q);assert(!valid_store(all,"p0"));
}
