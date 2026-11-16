#pragma once
#include <string>
#include <vector>
#include <cmath>
namespace treadmill_profiles {
struct Profile {
 std::string id,name,gender="Male",hr_max_mode="Formula",hr_zone_method="Percent";
 int age=30,weight=70,hr_max_manual=190,resting_heart_rate=60;
};
inline bool valid(const Profile &p){
 if(p.id.empty()||p.id.size()>24||p.name.empty()||p.name.size()>60)return false;
 for(unsigned char c:p.id)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'))return false;
 for(unsigned char c:p.name)if(c<32||c==127)return false;
 return p.age>=16&&p.age<=100&&p.weight>=40&&p.weight<=100&&p.hr_max_manual>=120&&p.hr_max_manual<=230&&p.resting_heart_rate>=30&&p.resting_heart_rate<=110&&(p.gender=="Male"||p.gender=="Female")&&(p.hr_max_mode=="Formula"||p.hr_max_mode=="Manual")&&(p.hr_zone_method=="Percent"||p.hr_zone_method=="Karvonen");
}
inline bool same_settings(const Profile&a,const Profile&b){return a.age==b.age&&a.weight==b.weight&&a.gender==b.gender&&a.hr_max_mode==b.hr_max_mode&&a.hr_max_manual==b.hr_max_manual&&a.resting_heart_rate==b.resting_heart_rate&&a.hr_zone_method==b.hr_zone_method;}
inline bool valid_store(const std::vector<Profile>&items,const std::string&active){
 if(items.empty()||items.size()>8)return false;
 bool found=false;
 for(size_t i=0;i<items.size();++i){if(!valid(items[i]))return false;if(items[i].id==active)found=true;for(size_t j=0;j<i;++j)if(items[j].id==items[i].id)return false;}return found;
}
}
