#pragma once
// M3G scenes remain Java objects; only framebuffer transfer and DEFLATE run here.
static int m3g_target(int graphics) {
    auto i=VMS_RS(g_gfx_target).find(graphics); return i==VMS_RS(g_gfx_target).end()?0:i->second;
}
static int m3g_read_target(int graphics) {
    int w=0,h=0,target=m3g_target(graphics);auto* px=pixels_of(target,w,h);
    if(!px || w<=0 || h<=0 || px->size()!=size_t(w)*h*3)return 0;
    int a=alloc_array("[I",size_t(w)*h);
    for(size_t i=0;i<size_t(w)*h;++i) {
        unsigned alpha=255;
        if(target && VMS_HEAP(g_heap)[target].alpha.size()==size_t(w)*h)alpha=VMS_HEAP(g_heap)[target].alpha[i];
        uint32_t c=(alpha<<24)|((*px)[i*3]<<16)|((*px)[i*3+1]<<8)|(*px)[i*3+2];
        VMS_HEAP(g_heap)[a].elems.set(i,Value::of_i(int_bits(c)));
    }
    return a;
}
static bool m3g_write_target(int graphics,int array,int x,int y,int cw,int ch) {
    int w=0,h=0,target=m3g_target(graphics);auto* px=pixels_of(target,w,h);
    if(!px || array<=0 || array>=int(VMS_HEAP(g_heap).size()) || VMS_HEAP(g_heap)[array].array_desc!="[I" ||
       w<=0 || h<=0 || VMS_HEAP(g_heap)[array].elems.size()!=size_t(w)*h || px->size()!=size_t(w)*h*3)return false;
    auto b=raster_primitives::intersect({0,0,w,h},{x,y,int64_t(x)+std::max(0,cw),int64_t(y)+std::max(0,ch)});
    for(int64_t yy=b.top;yy<b.bottom;++yy)for(int64_t xx=b.left;xx<b.right;++xx) {
        size_t i=size_t(yy)*w+size_t(xx);uint32_t c=VMS_HEAP(g_heap)[array].elems.get(i).i;
        (*px)[3*i]=c>>16;(*px)[3*i+1]=c>>8;(*px)[3*i+2]=c;
    }
    if(!target)VMS_RS(g_fb_dirty)=true;return true;
}
static int m3g_inflate(int input,int output) {
    if(input<=0||output<=0||input>=int(VMS_HEAP(g_heap).size())||output>=int(VMS_HEAP(g_heap).size())||
       VMS_HEAP(g_heap)[input].array_desc!="[B"||VMS_HEAP(g_heap)[output].array_desc!="[B")return -1;
    const auto& src=VMS_HEAP(g_heap)[input].elems;auto& dst=VMS_HEAP(g_heap)[output].elems;size_t n=src.size();
    if(n<6 || dst.size()>64*1024*1024)return -1;
    const uint8_t* p=src.read_bytes();
    if((p[0]&15)!=8 || (p[0]>>4)>7 || ((unsigned(p[0])<<8)+p[1])%31 || (p[1]&32))return -1;
    unsigned long used=n-6, produced=dst.size();
    if(puff(dst.byte_data(),&produced,p+2,&used)||used!=n-6||produced!=dst.size())return -1;
    uint32_t a=1,b=0;for(size_t i=0;i<produced;++i){a=(a+dst.read_bytes()[i])%65521;b=(b+a)%65521;}
    uint32_t expected=(uint32_t(p[n-4])<<24)|(uint32_t(p[n-3])<<16)|(uint32_t(p[n-2])<<8)|p[n-1];
    return ((b<<16)|a)==expected?int(produced):-1;
}
