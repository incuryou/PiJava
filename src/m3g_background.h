#include "vm_rs_fallback.h"
#ifndef J2ME_M3G_BACKGROUND_H
#define J2ME_M3G_BACKGROUND_H
#include <vector>
 VMS_RS_ACCESSOR(g_m3g_native_backgrounds, uint64_t) VMS_RS_ACCESSOR(g_m3g_native_background_pixels, uint64_t) VMS_RS_ACCESSOR(g_m3g_native_sorts, uint64_t)
static bool m3g_clear_to_target(int g,int background,int image,bool imageTarget) {
    int cw=0,ch=0,cropx=0,cropy=0,cropw=0,croph=0,modex=0,modey=0,vieww=0,viewh=0,vx=0,vy=0,ox=0,oy=0,l=0,t=0,r=0,b=0,hints=0,iw=0,ih=0;
    if(!m3g_int(background,"cropx","I",cropx)||!m3g_int(background,"cropy","I",cropy)||!m3g_int(background,"cropw","I",cropw)||!m3g_int(background,"croph","I",croph)||
       !m3g_int(background,"modex","I",modex)||!m3g_int(background,"modey","I",modey)||!m3g_int(image,"width","I",iw)||!m3g_int(image,"height","I",ih)||
       !m3g_int(g,"vieww","I",vieww)||!m3g_int(g,"viewh","I",viewh)||!m3g_int(g,"viewx","I",vx)||!m3g_int(g,"viewy","I",vy)||
       !m3g_int(g,"viewClipL","I",l)||!m3g_int(g,"viewClipT","I",t)||!m3g_int(g,"viewClipR","I",r)||!m3g_int(g,"viewClipB","I",b)||!m3g_int(g,"hints","I",hints))return false;
    if(vieww<=0||viewh<=0||iw<=0||ih<=0||int64_t(iw)*ih>INT32_MAX||(modex!=32&&modex!=33)||(modey!=32&&modey!=33))return false;
    ArrayStorage* source=m3g_array(m3g_ref(image,"image","[I"),"[I",size_t(iw)*ih),*dest=nullptr;
    if(imageTarget) {
        int target=m3g_ref(g,"target","Ljava/lang/Object;");
        if(!m3g_int(target,"width","I",cw)||!m3g_int(target,"height","I",ch))return false;
        if(cw<=0||ch<=0||int64_t(cw)*ch>INT32_MAX)return false;
        dest=m3g_array(m3g_ref(target,"image","[I"),"[I",size_t(cw)*ch);
    } else {
        if(!m3g_int(g,"canvasWidth","I",cw)||!m3g_int(g,"canvasHeight","I",ch)||!m3g_int(g,"originX","I",ox)||!m3g_int(g,"originY","I",oy))return false;
        if(cw<=0||ch<=0||int64_t(cw)*ch>INT32_MAX)return false;
        dest=m3g_array(m3g_ref(g,"rasterData","[I"),"[I",size_t(cw)*ch);
    }
    if(!source||!dest)return false;
    if(cropw<=0)cropw=iw;
    if(croph<=0)croph=ih;
    // Java's 16.16 arithmetic wraps at 32 bits. Unsigned operations preserve that
    // behavior without C++ signed-overflow UB, including negative crop origins.
    int32_t sx=int32_t(uint32_t(cropw)<<16)/vieww,sy=int32_t(uint32_t(croph)<<16)/viewh;
    int64_t originX=int64_t(ox)+vx,originY=int64_t(oy)+vy;
    int64_t left=std::max<int64_t>(l,-originX),right=std::min<int64_t>(r,int64_t(cw)-originX);
    int64_t top=std::max<int64_t>(t,-originY),bottom=std::min<int64_t>(b,int64_t(ch)-originY);
    ++VMS_RS(g_m3g_native_backgrounds);if(left>=right||top>=bottom)return true;
    for(int y=int(top);y<int(bottom);++y) {
        int32_t cy=int32_t((uint32_t(cropy)<<16)+uint32_t(y)*uint32_t(sy));
        int32_t cx=int32_t((uint32_t(cropx)<<16)+uint32_t(left)*uint32_t(sx));
        int ty=cy>>16;
        for(int x=int(left);x<int(right);++x,cx=int32_t(uint32_t(cx)+uint32_t(sx))) {
            int tx=cx>>16;
            if((modex==32&&(tx<0||tx>=iw))||(modey==32&&(ty<0||ty>=ih)))continue;
            int xx=m3g_wrap(tx,iw,modex==33),yy=m3g_wrap(ty,ih,modey==33);
            uint32_t pixel=source->get(size_t(yy)*iw+xx).i;
            if(hints&4) {
                int amount=(((x^y)&1)<<3)|((y&1)<<2)|(((x^y)&2)<<1)|((y&2)>>1),c[4];m3g_unpack(pixel,c);
                for(int k=1;k<4;++k)c[k]=std::min(255,c[k]+amount);
                pixel=m3g_pack(c);
            }
            dest->set(size_t(originY+y)*cw+size_t(originX+x),Value::of_i(int32_t(pixel)));++VMS_RS(g_m3g_native_background_pixels);
        }
    }
    return true;
}
static bool m3g_sort_front_to_back(int array,int count) {
    if(count<0)return false;
    ArrayStorage* triangles=m3g_array(array,"[Ljavax/microedition/m3g/Triangle;",size_t(count));if(!triangles)return false;
    if(count<2)return true;
    struct Entry {int ref;float z;};std::vector<Entry> order;order.reserve(count);
    for(int i=0;i<count;++i){int ref=triangles->get(i).ref;Value* z=m3g_field(ref,"sortZ","F");if(!z||!std::isfinite(bits_f(z->i)))return false;order.push_back({ref,bits_f(z->i)});}
    // The Java insertion sort moves larger sortZ first and preserves ties.
    std::stable_sort(order.begin(),order.end(),[](const Entry& a,const Entry& b){return a.z>b.z;});
    for(int i=0;i<count;++i)triangles->set(i,Value::of_ref(order[i].ref));
    ++VMS_RS(g_m3g_native_sorts);return true;
}
#endif
