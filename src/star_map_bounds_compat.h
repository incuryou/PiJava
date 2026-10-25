#pragma once

// Exact-JAR map-cache compatibility. This port centers a 336-wide world in a
// 360-wide Canvas, but copies unused white Image pixels beyond its right edge.
// Restrict only its verified cache blit; later sprites/UI and Java's clip,
// Image initialization, camera arithmetic and serialized state remain intact.
static bool star_map_bounds_compat_clip(int gfx,int target,int image,int transform,raster_region::Clip& clip) {
    if(target!=0||transform!=0||VMS_RS(g_screen_w)!=360||VMS_RS(g_screen_h)!=360||
       VMS_PLAT(g_rms_suite)!="e808c5b9878639757edcf67d330536a7b95f553790c2cf23c7d267df06ecb6b8")return false;
    const auto* stack=VMS_EXEC(g_active_stack);
    if(!stack||stack->size()<2)return false;
    size_t inner_index=stack->size()-1;
    const auto class_is=[](const Method* m,const char* name){
        return m&&m->class_id>=0&&m->class_id<int(VMS_CL(g_classes).size())&&VMS_CL(g_classes)[m->class_id].name==name;
    };
    const Method* top=(*stack)[inner_index].method;
    // The slow native path retains Graphics.drawImage's frame; the intrinsic
    // path enters the same raster function directly from the Java helper.
    if(class_is(top,"javax/microedition/lcdui/Graphics")&&top->native&&top->name=="drawImage"&&
       top->desc=="(Ljavax/microedition/lcdui/Image;III)V") {
        if(inner_index<2)return false;
        --inner_index;
    }
    if(inner_index==0)return false;
    const Frame& inner=(*stack)[inner_index];const Method* helper=inner.method;
    if(!class_is(helper,"n")||helper->native||(helper->flags&10)!=10||helper->name!="a"||
       helper->desc!="(Ljavax/microedition/lcdui/Graphics;IIIIII)V"||helper->code.size()!=41||
       inner.op_pc!=25||inner.locals.empty()||inner.locals[0].ref!=gfx)return false;
    uint32_t fingerprint=2166136261u;for(uint8_t byte:helper->code){fingerprint^=byte;fingerprint*=16777619u;}
    if(fingerprint!=0x2397c1c2u)return false;
    const int owner=helper->class_id;
    if(find_method(owner,"a",helper->desc,false)!=helper)return false;
    const Frame& outer=(*stack)[inner_index-1];const Method* method=outer.method;
    if(!method||method->class_id!=owner||method->native||(method->flags&8)||method->name!="a"||
       method->desc!="(Ljavax/microedition/lcdui/Graphics;II)V"||outer.locals.size()<4||outer.locals[1].ref!=gfx)return false;
    const int map=outer.locals[0].ref;
    if(map<=0||map>=int(VMS_HEAP(g_heap).size())||VMS_HEAP(g_heap)[map].cls!=owner||
       image<=0||image>=int(VMS_HEAP(g_heap).size()))return false;
    const auto field=[&](const char* name,const char* desc,bool is_static)->const Value*{
        const Field* f=find_field(owner,name,desc);
        if(!f||f->owner!=owner||f->is_static!=is_static||f->slot<0)return nullptr;
        const auto& values=is_static?VMS_CL(g_classes)[owner].statics:VMS_HEAP(g_heap)[map].fields;
        return f->slot<int(values.size())?&values[f->slot]:nullptr;
    };
    const Value* cache=field("G","Ljavax/microedition/lcdui/Image;",true);
    const Value* cache_w=field("E","I",true);const Value* cache_h=field("F","I",true);
    const Value* world_w=field("f","S",false);const Value* world_h=field("g","S",false);
    if(!cache||cache->ref!=image||!cache_w||!cache_h||!world_w||!world_h||
       cache_w->i!=VMS_HEAP(g_heap)[image].width||cache_h->i!=VMS_HEAP(g_heap)[image].height||
       world_w->i<=0||world_h->i<=0||(world_w->i>=360&&world_h->i>=360))return false;
    const auto translation=[&](const std::unordered_map<int,int>& values){auto it=values.find(gfx);return it==values.end()?0:it->second;};
    const int64_t x=int64_t(translation(VMS_RS(g_gfx_tx)))-outer.locals[2].i;
    const int64_t y=int64_t(translation(VMS_RS(g_gfx_ty)))-outer.locals[3].i;
    auto bounds=raster_primitives::intersect({0,0,360,360},{x,y,x+world_w->i,y+world_h->i});
    if(clip.on)bounds=raster_primitives::intersect(bounds,{clip.x,clip.y,int64_t(clip.x)+clip.w,int64_t(clip.y)+clip.h});
    const int left=int(std::clamp<int64_t>(bounds.left,0,360)),top_edge=int(std::clamp<int64_t>(bounds.top,0,360));
    clip={true,left,top_edge,int(std::max<int64_t>(0,bounds.right-left)),int(std::max<int64_t>(0,bounds.bottom-top_edge))};
    return true;
}
