#pragma once
// Launcher-only RMS copying. Never shares suites or changes J2S snapshots.
#include "rms_catalog.h"
#include "suite_identity.h"
#include <map>
#include <unistd.h>

namespace chapter_save {
struct Store { std::string file, source; uint64_t bytes=0; };
struct Result { bool ok=false; size_t stores=0; std::string backup,error; };
inline bool identity(const std::string& id) {
    return id.size()==64 && id.find_first_not_of("0123456789abcdef")==std::string::npos;
}
// 0 absent; 1 ordinary directory; -1 inspection failure or unsafe object.
inline int directory(const std::string& path) {
    struct stat st{};
    if(lstat(path.c_str(),&st)!=0)return errno==ENOENT?0:-1;
    return S_ISDIR(st.st_mode)?1:-1;
}
inline bool make_directory(const std::string& path) {
    const int state=directory(path);
    return state==1 || (state==0 && mkdir(path.c_str(),0777)==0);
}
inline bool names(const std::string& path,std::vector<std::string>& out) {
    DIR* dir=opendir(path.c_str());if(!dir)return false;
    bool ok=true;out.clear();
    for(;;){errno=0;auto* e=readdir(dir);if(!e){ok=errno==0;break;}
        if(std::strcmp(e->d_name,".") && std::strcmp(e->d_name,".."))out.emplace_back(e->d_name);}
    if(closedir(dir)!=0)ok=false;return ok;
}
inline std::string jar_identity(const std::string& path) {
    struct stat st{};if(lstat(path.c_str(),&st)!=0 || !S_ISREG(st.st_mode))return {};
    FILE* f=std::fopen(path.c_str(),"rb");if(!f)return {};
    rms_suite_identity_detail::Sha256 hash;uint8_t buffer[65536];size_t n;
    while((n=std::fread(buffer,1,sizeof buffer,f))>0)hash.update(buffer,n);
    bool ok=!std::ferror(f);if(std::fclose(f)!=0)ok=false;
    return ok?hash.finish():std::string();
}
inline bool inspect(const std::string& root,const std::string& id,
                    std::vector<Store>& stores,std::string& error) {
    stores.clear();error.clear();
    if(!identity(id)){error="存档身份无效";return false;}
    const std::string suite=root+"/suites/"+id;
    if(directory(root)!=1 || directory(root+"/suites")!=1 || directory(suite)!=1){error="来源没有游戏内存档，或目录不可读取";return false;}
    std::vector<std::string> entries;if(!names(suite,entries)){error="无法读取来源目录";return false;}
    std::map<std::string,std::string> candidates;
    for(const auto& file:entries){std::string decoded,primary;
        if(j2me_rms::decode_name(file,decoded,primary))candidates[primary]=primary;}
    uint64_t total=0;
    for(const auto& candidate:candidates){
        const std::string primary=suite+"/"+candidate.first;std::string path=primary;
        struct stat st{};
        if(lstat(path.c_str(),&st)!=0){
            if(errno!=ENOENT){error="无法检查来源存档";return false;}
            path+=".previous";if(lstat(path.c_str(),&st)!=0){error="来源存档已变化";return false;}
        }
        if(!S_ISREG(st.st_mode) || st.st_size<0 || !j2me_rms::valid_snapshot(path)){error="来源存档损坏或类型不受支持";return false;}
        total+=uint64_t(st.st_size);
        if(total>32*1024*1024 || stores.size()>=256){error="来源存档过大（限 32 MiB / 256 个存档库）";return false;}
        stores.push_back({candidate.first,path,uint64_t(st.st_size)});
    }
    if(stores.empty()){error="来源没有可继承的游戏内存档";return false;}
    return true;
}
inline bool archive(const std::string& root,const std::string& id,const std::string& path,
                    const std::string& tag,std::string& saved) {
    const std::string base=root+"/chapter-backups",folder=base+"/"+id;
    if(!make_directory(base)||!make_directory(folder))return false;
    for(unsigned n=1;n<=100000;++n){
        saved=folder+"/"+tag+"-"+std::to_string(n);struct stat st{};
        if(lstat(saved.c_str(),&st)==0)continue;
        if(errno!=ENOENT)return false;
        return std::rename(path.c_str(),saved.c_str())==0;
    }
    return false;
}
// Called before cold launch, continue and another import. No work on ordinary
// suites; abandoned data is archived, never recursively removed.
inline bool recover(const std::string& root,const std::string& id,std::string& error,bool for_import=false) {
    error.clear();if(!identity(id)){error="存档身份无效";return false;}
    const int r=directory(root),s=directory(root+"/suites");
    if(r<0 || (r==1 && s<0)){error="存档目录不可用";return false;}
    if(r==0 || s==0)return true;
    const std::string target=root+"/suites/"+id,old=target+".chapter-old",stage=target+".chapter-new";
    const int t=directory(target),o=directory(old),n=directory(stage);
    if(t<0||o<0||n<0){error="继承目录类型异常，已保留原文件";return false;}
    std::string saved;
    if(o==1){
        if(t==0){
            if(std::rename(old.c_str(),target.c_str())!=0){error="无法恢复原进度，原文件已保留";return false;}
        }else if(!archive(root,id,old,"old",saved) && for_import){
            error="原进度备份尚未归档，请检查空间或目录后重试";return false;
        }
    }
    // Relocating residual data is optional for launch, but its transaction
    // names MUST be vacant before another import may touch the target.
    if(n==1 && !archive(root,id,stage,"staging",saved) && for_import){error="无法归档未完成的导入，原文件已保留";return false;}
    return true;
}
inline bool copy_store(const Store& store,const std::string& target) {
    FILE* in=std::fopen(store.source.c_str(),"rb");if(!in)return false;
    FILE* out=std::fopen(target.c_str(),"wb");if(!out){std::fclose(in);return false;}
    uint8_t buffer[65536];uint64_t bytes=0;size_t n;bool ok=true;
    rms_suite_identity_detail::Sha256 source_hash;
    while((n=std::fread(buffer,1,sizeof buffer,in))>0){
        bytes+=n;if(bytes>store.bytes || std::fwrite(buffer,1,n,out)!=n){ok=false;break;}
        source_hash.update(buffer,n);
    }
    ok=ok && !std::ferror(in) && bytes==store.bytes;
    if(std::fclose(in)!=0)ok=false;
    if(std::fflush(out)!=0)ok=false;
    if(ok && fsync(fileno(out))!=0)ok=false;
    if(std::fclose(out)!=0)ok=false;
    return ok && j2me_rms::valid_snapshot(target) && jar_identity(target)==source_hash.finish();
}
inline Result import_stores(const std::string& root,const std::string& source,const std::string& target) {
    Result result;
    if(!identity(source)||!identity(target)||source==target){result.error="请选择不同游戏作为来源";return result;}
    if(!recover(root,target,result.error,true))return result;
    std::vector<Store> stores;if(!inspect(root,source,stores,result.error))return result;
    const std::string dest=root+"/suites/"+target,stage=dest+".chapter-new",old=dest+".chapter-old";
    if(mkdir(stage.c_str(),0777)!=0){result.error="无法准备导入目录";return result;}
    for(const auto& store:stores)if(!copy_store(store,stage+"/"+store.file)){
        result.error="复制或校验失败，原进度未替换";return result;}
    const int existing=directory(dest);
    if(existing<0){result.error="目标存档目录异常";return result;}
    if(existing==1 && std::rename(dest.c_str(),old.c_str())!=0){result.error="无法备份目标进度，未替换";return result;}
    if(std::rename(stage.c_str(),dest.c_str())!=0){
        if(existing==1)std::rename(old.c_str(),dest.c_str()); // recover retries if this also fails.
        result.error="切换失败，原进度已保留，请重试";return result;
    }
    result.ok=true;result.stores=stores.size();
    if(existing==1){result.backup=old;std::string saved;
        if(archive(root,target,old,"old",saved))result.backup=saved;}
    return result;
}
} // namespace chapter_save
