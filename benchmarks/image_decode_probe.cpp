#include <filesystem>
#include <ctime>
#include <iomanip>
#define main emulator_main
#include PROBE_VM
#undef main
static uint64_t mix(uint64_t hash,uint8_t b){return (hash^b)*1099511628211ULL;}
int main(int argc,char**argv){
    if(argc!=4)return 2;const int repeat=std::atoi(argv[2]);if(repeat<1||repeat>100)return 2;
    std::vector<std::filesystem::path> paths;for(auto& e:std::filesystem::directory_iterator(argv[1]))if(e.is_regular_file())paths.push_back(e.path());
    std::sort(paths.begin(),paths.end());std::vector<std::vector<uint8_t>> inputs(paths.size());
    for(size_t i=0;i<paths.size();i++)if(!read_whole(paths[i].string(),inputs[i]))return 3;
    const auto start=std::clock();uint64_t sum=14695981039346656037ULL;unsigned good=0,bad=0;
    for(int r=0;r<repeat;r++)for(size_t i=0;i<inputs.size();i++){
        std::vector<uint8_t> rgb,alpha;int w=0,h=0;std::string error;
        const bool ok=decode_png(inputs[i],rgb,alpha,w,h,error);
        uint64_t hash=14695981039346656037ULL;
        if(ok){++good;for(size_t j=0;j<rgb.size()/3;j++){hash=mix(hash,rgb[j*3]);hash=mix(hash,rgb[j*3+1]);hash=mix(hash,rgb[j*3+2]);hash=mix(hash,alpha.empty()?255:alpha[j]);}}
        else ++bad;
        for(int k=0;k<8;k++)sum=mix(sum,uint8_t(hash>>(k*8)));sum=mix(sum,ok);
        if(r==0)std::cout<<paths[i].filename().string()<<" "<<ok<<" "<<(ok?w:0)<<" "<<(ok?h:0)<<" "<<std::hex<<hash<<std::dec<<"\n";
    }
    double cpu=1000.0*(std::clock()-start)/CLOCKS_PER_SEC;
    std::cerr<<std::setprecision(10)<<"{\"variant\":\""<<argv[3]<<"\",\"images\":"<<inputs.size()<<",\"repeat\":"<<repeat<<",\"decoded\":"<<good<<",\"rejected\":"<<bad<<",\"cpu_ms\":"<<cpu<<",\"digest\":\""<<std::hex<<sum<<"\",\"scope\":\"PNG cold decoder only, no resource cache, desktop CPU incl pixel hash\"}\n";
}
