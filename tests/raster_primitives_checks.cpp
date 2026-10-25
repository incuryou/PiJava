#include "../src/raster_primitives.h"
#include <climits>
#include <cstdlib>
#include <iostream>
#include <random>
using namespace raster_primitives;
static void check(bool ok,const char* label){if(!ok){std::cerr<<"FAIL "<<label<<"\n";std::exit(1);}}
static bool colored(const std::vector<uint8_t>& p,int x,int y){return p[(size_t(y)*31+x)*3]!=0;}
static void old_line(Surface& s,int x0,int y0,int x1,int y1) {
    int dx=std::abs(x1-x0),sx=x0<x1?1:-1,dy=-std::abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy;
    while(true){s.put(x0,y0,0xffffff);if(x0==x1&&y0==y1)break;int e2=2*err;if(e2>=dy){err+=dy;x0+=sx;}if(e2<=dx){err+=dx;y0+=sy;}}
}
int main() {
    const int w=31,h=27;std::vector<uint8_t> p(w*h*3),q(p.size());
    Surface s{p,w,h,{0,0,w,h}},t{q,w,h,{0,0,w,h}};
    rect(s,2,3,4,5,0xffffff,true);
    check(colored(p,6,8)&&!colored(p,4,5),"draw rectangle uses inclusive right/bottom and hollow interior");
    p.assign(p.size(),0);rect(s,5,5,0,0,0xffffff,true);check(colored(p,5,5),"zero-size outlined rectangle paints one pixel");
    p.assign(p.size(),0);rect(s,5,5,-1,5,0xffffff,true);rect(s,5,5,0,5,0xffffff,false);
    check(p==q,"negative outlines and empty fills are no-ops");
    check(source_over(0x80ff0000,0x0000ff,true)==0x80007f,"source-over blends semitransparent pixels");
    check(source_over(0x00ff0000,0x0000ff,true)==0x0000ff,"transparent source preserves destination");
    check(source_over(0x00ff0000,0x0000ff,false)==0xff0000,"ignored alpha means opaque");
    check(array_range(4,2,-2,2,2)&&array_range(2,0,0,2,4),"negative and repeated scanlines");
    check(!array_range(4,0,-2,2,2)&&!array_range(4,3,2,2,2),"all addressed pixels validated");
    check(!array_range(4,INT_MAX,INT_MAX,INT_MAX,INT_MAX),"array index products widened");
    std::mt19937 random(57);int cases=0;
    for(int i=0;i<5000;++i) {
        p.assign(p.size(),0);q.assign(q.size(),0);
        int x0=int(random()%101)-35,y0=int(random()%101)-35,x1=int(random()%101)-35,y1=int(random()%101)-35;
        line(s,x0,y0,x1,y1,0xffffff);old_line(t,x0,y0,x1,y1);
        check(p==q,"closed-form line preserves Bresenham rasterization");++cases;
    }
    for(int shape=0;shape<6;++shape)for(int width: {0,1,2,9,28,INT_MAX})for(int height:{0,1,8,23,INT_MAX})for(int sweep:{-450,-100,-1,0,1,95,360,INT_MIN}) {
        p.assign(p.size(),0);q.assign(q.size(),0);s.clip={0,0,w,h};t.clip={7,5,24,20};
        auto paint=[&](Surface& surface){
            if(shape<2)arc(surface,-3,2,width,height,-30,sweep,0xffffff,shape==1);
            else if(shape<4)round_rect(surface,-3,2,width,height,13,17,0xffffff,shape==3);
            else rect(surface,-3,2,width,height,0xffffff,shape==5);
        };
        paint(s);paint(t);
        for(int y=0;y<h;++y)for(int x=0;x<w;++x)
            check(colored(q,x,y)==(x>=7&&x<24&&y>=5&&y<20&&colored(p,x,y)),"clipping preserves primitive coverage");
        ++cases;
    }
    s.clip={0,0,w,h};p.assign(p.size(),0);arc(s,2,2,16,16,0,90,0xffffff,false);
    check(colored(p,13,6)&&!colored(p,6,13)&&!colored(p,2,2),"filled arcs are counterclockwise pie wedges");
    p.assign(p.size(),0);arc(s,2,2,16,16,0,-90,0xffffff,false);
    check(colored(p,13,13)&&!colored(p,13,6),"negative arc sweep is clockwise");
    p.assign(p.size(),0);round_rect(s,2,2,16,16,12,12,0xffffff,false);
    check(!colored(p,2,2)&&colored(p,10,10),"rounded corners cut away outer corner");
    p.assign(p.size(),0);line(s,INT_MIN,13,INT_MAX,13,0xffffff);
    for(int x=0;x<w;++x)check(colored(p,x,13),"extreme endpoints rasterize bounded visible line");
    p.assign(p.size(),0);triangle(s,INT_MIN,INT_MIN,INT_MAX,INT_MIN,0,INT_MAX,0xffffff);
    check(colored(p,15,13),"extreme triangle geometry stays bounded");
    std::cout<<"PASS RasterPrimitivesChecks "<<cases<<" differential/clip cases, degenerate geometry, alpha and extreme coordinates\n";
}
