#pragma once
// Private Micro3D/MIDP pixel transfer. All owning state is in Java Graphics,
// Images and arrays. No allocation, callback, suspension or retained pointer.
struct Micro3DTargetView {
    int target=0,width=0,height=0;
    const std::vector<uint8_t>* pixels=nullptr;
    const std::vector<uint8_t>* alpha=nullptr;
};
static bool micro3d_target_view(int graphics,Micro3DTargetView& out) {
    if(graphics<=0||size_t(graphics)>=VMS_HEAP(g_heap).size())return false;
    auto it=VMS_RS(g_gfx_target).find(graphics);
    if(it==VMS_RS(g_gfx_target).end())return false;
    int cls=VMS_HEAP(g_heap)[graphics].cls;
    if(cls<0||size_t(cls)>=VMS_CL(g_classes).size()||VMS_CL(g_classes)[cls].name!="javax/microedition/lcdui/Graphics")return false;
    out.target=it->second;
    if(out.target==0){out.width=VMS_RS(g_screen_w);out.height=VMS_RS(g_screen_h);out.pixels=&VMS_RS(g_fb);}
    else {
        if(out.target<=0||size_t(out.target)>=VMS_HEAP(g_heap).size())return false;
        const auto& image=VMS_HEAP(g_heap)[out.target];
        if(image.cls<0||size_t(image.cls)>=VMS_CL(g_classes).size())return false;
        const auto* flag=find_field(image.cls,"mutable","Z");
        if(!flag||flag->is_static||flag->slot<0||size_t(flag->slot)>=image.fields.size()||!image.fields[flag->slot].i)return false;
        out.width=image.width;out.height=image.height;out.pixels=&image.pixels;out.alpha=&image.alpha;
    }
    if(out.width<=0||out.height<=0)return false;
    const uint64_t count=uint64_t(out.width)*uint64_t(out.height);
    return count<=INT32_MAX&&out.pixels->size()==count*3&&(!out.alpha||out.alpha->empty()||out.alpha->size()==count);
}
static int micro3d_target_dimension(int graphics,bool height) {
    Micro3DTargetView target;if(!micro3d_target_view(graphics,target))return 0;
    return height?target.height:target.width;
}
static bool micro3d_read_target(int graphics,int array) {
    Micro3DTargetView target;if(!micro3d_target_view(graphics,target))return false;
    const size_t count=size_t(target.width)*target.height;
    auto* dest=m3g_array(array,"[I",count);if(!dest)return false;
    auto* bytes=dest->int_bytes_for_write();if(!bytes)return false;
    const bool alpha=target.alpha&&!target.alpha->empty();
    for(size_t i=0;i<count;++i){
        uint32_t pixel=(uint32_t(alpha?(*target.alpha)[i]:255)<<24)|
            (uint32_t((*target.pixels)[i*3])<<16)|(uint32_t((*target.pixels)[i*3+1])<<8)|(*target.pixels)[i*3+2];
        std::memcpy(bytes+i*4,&pixel,4);
    }
    return true;
}
static bool micro3d_write_target(int graphics,int array,int x,int y,int width,int height) {
    Micro3DTargetView target;if(width<0||height<0||!micro3d_target_view(graphics,target))return false;
    const size_t count=size_t(target.width)*target.height;
    const auto* src=m3g_array(array,"[I",count);if(!src)return false;
    const auto clip=raster_primitives::intersect({0,0,target.width,target.height},
        {x,y,int64_t(x)+width,int64_t(y)+height});
    if(clip.left>=clip.right||clip.top>=clip.bottom)return true;
    // Validation is complete. Borrow the writable image only at commit time.
    int w=0,h=0;auto* pixels=pixels_of(target.target,w,h);
    auto* alpha=target.target&&target.alpha&&!target.alpha->empty()?&VMS_HEAP(g_heap)[target.target].alpha:nullptr;
    const uint8_t* bytes=src->raw_payload();
    for(int64_t yy=clip.top;yy<clip.bottom;++yy)for(int64_t xx=clip.left;xx<clip.right;++xx){
        const size_t i=size_t(yy)*target.width+size_t(xx);uint32_t pixel;std::memcpy(&pixel,bytes+i*4,4);
        (*pixels)[i*3]=uint8_t(pixel>>16);(*pixels)[i*3+1]=uint8_t(pixel>>8);(*pixels)[i*3+2]=uint8_t(pixel);
        if(alpha)(*alpha)[i]=uint8_t(pixel>>24);
    }
    if(!target.target)VMS_RS(g_fb_dirty)=true;
    return true;
}
