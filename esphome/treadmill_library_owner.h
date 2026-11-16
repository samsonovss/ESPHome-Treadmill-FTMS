#pragma once
// All calls hold recorder FsLease. Profile definitions follow library headers.
namespace treadmill_profiles { std::string library_owner(); std::string library_default_owner(); }
namespace treadmill_recorder {
inline bool library_expected(AsyncWebServerRequest *r){auto h=r->get_header("X-Library-Owner");auto o=treadmill_profiles::library_owner();return !o.empty()&&h.has_value()&&h.value()==o;}
inline bool library_atomic(const std::string &path,const std::string &body){
 const auto tmp=path+".tmp";FILE*f=fopen(tmp.c_str(),"wb");if(!f)return false;
 bool ok=fwrite(body.data(),1,body.size(),f)==body.size();if(fflush(f))ok=false;if(fsync(fileno(f)))ok=false;if(fclose(f))ok=false;
 return ok&&rename(tmp.c_str(),path.c_str())==0;
}
inline bool library_backup(const std::string &src,const std::string &dest){
 struct stat st{};if(stat(dest.c_str(),&st)==0)return true;if(errno!=ENOENT||stat(src.c_str(),&st)||st.st_size<0||st.st_size>65536)return false;
 FILE*f=fopen(src.c_str(),"rb");if(!f)return false;std::string b(st.st_size,'\0');bool ok=fread(b.data(),1,b.size(),f)==b.size();if(fclose(f))ok=false;return ok&&library_atomic(dest,b);
}
// Move the complete legacy directory once. Atomic rename keeps all source bytes,
// including unknown files. Binding is committed BEFORE the rename: retries never
// assign legacy data to whichever profile happens to be active after a reboot.
inline bool library_prepare(const char *kind){
 const std::string base=treadmill_storage::BASE_PATH,root=base+"/"+kind,bind=base+"/library-legacy-owner";
 if(treadmill_profiles::library_owner().empty())return false;
 std::string owner;struct stat st{};
 if(stat(bind.c_str(),&st)==0){if(st.st_size<1||st.st_size>24)return false;FILE*f=fopen(bind.c_str(),"rb");if(!f)return false;owner.resize(st.st_size);bool ok=fread(owner.data(),1,owner.size(),f)==owner.size();fclose(f);if(!ok)return false;}
 else {if(errno!=ENOENT)return false;owner=treadmill_profiles::library_default_owner();if(owner.empty()||!library_atomic(bind,owner))return false;}
 for(char c:owner)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'))return false;
 const auto personal=base+"/personal-"+kind;
 if(mkdir(personal.c_str(),0700)&&errno!=EEXIST)return false;
 const auto dest=personal+"/"+owner;
 if(stat(root.c_str(),&st)==0){
  // Never merge/overwrite an existing destination; fail closed for manual recovery.
  struct stat ds{};if(stat(dest.c_str(),&ds)==0||errno!=ENOENT)return false;
  const auto backup=base+"/legacy-backup-"+kind;if(mkdir(backup.c_str(),0700)&&errno!=EEXIST)return false;
  DIR*d=opendir(root.c_str());if(!d)return false;bool ok=true;while(auto*e=readdir(d)){std::string n=e->d_name;if(n=="."||n=="..")continue;struct stat fs{};if(stat((root+"/"+n).c_str(),&fs)){ok=false;break;}if(S_ISREG(fs.st_mode)&&!library_backup(root+"/"+n,backup+"/"+n)){ok=false;break;}}closedir(d);if(!ok)return false;
  if(rename(root.c_str(),dest.c_str()))return false;
 }else if(errno!=ENOENT)return false;
 return true;
}
inline std::string library_dir(const char *kind){return std::string(treadmill_storage::BASE_PATH)+"/personal-"+kind+"/"+treadmill_profiles::library_owner();}
}
