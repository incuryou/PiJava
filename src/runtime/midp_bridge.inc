// Java native API and host-service boundary; original Java call/exception contracts.
// Included once by the runtime composition unit; not a standalone TU.
static void call_native_impl(Method*,Frame&);
static void call_native(Method* m,Frame& fr){
    const auto token=diag_native_enter(m);call_native_impl(m,fr);diag_native_leave(token);
}
static void call_native_impl(Method* m, Frame& fr) { P61_SCOPE(2, m);
    Class& native_owner=VMS_CL(g_classes)[m->class_id];
    const std::string& cn = native_owner.name;
    if(J2ME_OPT_NATIVE_CLASS_CACHE && native_owner.native_id==NativeClassId::Unresolved)
        native_owner.native_id=native_class_id(cn);
    const auto native_match=[&](NativeClassId id,const char* name) {
        return J2ME_OPT_NATIVE_CLASS_CACHE ? native_owner.native_id==id : cn==name;
    };
    const std::string& name = m->name;
    auto arg = [&](int i) -> Value { return i < (int)fr.locals.size() ? fr.locals[i] : Value{}; };
    auto ret_ref = [&](int r) { fr.stack.push_back(Value::of_ref(r)); };
    auto ret_i = [&](int v) { fr.stack.push_back(Value::of_i(v)); };
    auto ret_l = [&](int64_t v) { fr.stack.push_back(Value::of_l(v)); };
    if(native_match(NativeClassId::Mesh242,"javax/microedition/m3g/M3GMesh242") && (m->flags&0x0008)) {
        if(name=="enabled" && m->desc=="()Z")ret_i(VMS_RS(g_m3g_mesh242_enabled));
        else if(name=="eligible" && m->desc=="(Ljavax/microedition/m3g/VertexArray;Ljavax/microedition/m3g/VertexArray;Ljavax/microedition/m3g/VertexArray;Ljavax/microedition/m3g/VertexArray;Ljavax/microedition/m3g/VertexBuffer;)Z")
            ret_i(m3g_mesh242::eligible_skin(arg(0).ref,arg(1).ref,arg(2).ref,arg(3).ref,arg(4).ref));
        else if(name=="skinBlock" && m->desc=="(Ljavax/microedition/m3g/VertexArray;Ljavax/microedition/m3g/VertexArray;[[F[I[F[F[F[FII)Z")
            ret_i(m3g_mesh242::skin_block(arg(0).ref,arg(1).ref,arg(2).ref,arg(3).ref,arg(4).ref,arg(5).ref,arg(6).ref,arg(7).ref,arg(8).i,arg(9).i));
        else if(name=="quantize" && m->desc=="([FLjava/lang/Object;IIIFFFFZ)Z")
            ret_i(m3g_mesh242::quantize(arg(0).ref,arg(1).ref,arg(2).i,arg(3).i,arg(4).i,bits_f(arg(5).i),bits_f(arg(6).i),bits_f(arg(7).i),bits_f(arg(8).i),arg(9).i!=0));
        else if(name=="composite" && m->desc=="(Ljavax/microedition/m3g/Transformable;Ljavax/microedition/m3g/Transform;)Z")ret_i(m3g_mesh242::composite(arg(0).ref,arg(1).ref));
        else if(name=="transformTo" && m->desc=="(Ljavax/microedition/m3g/Node;Ljavax/microedition/m3g/Node;Ljavax/microedition/m3g/Transform;)I")ret_i(m3g_mesh242::transform_to(arg(0).ref,arg(1).ref,arg(2).ref));
        else if(name=="identity" && m->desc=="([F)Z")ret_i(m3g_mesh242::identity(arg(0).ref));
        else if(name=="invert" && m->desc=="([F[F)Z")ret_i(m3g_mesh242::invert(arg(0).ref,arg(1).ref));
        else die("native M3GMesh242."+name+m->desc);
        return;
    }
    if(native_match(NativeClassId::C1,"javax/microedition/m3g/Transform")) {
        if(name=="_transformVectors")ret_i(m3g_transform_vectors(arg(0).ref,arg(1).ref));
        else if(name=="_transformVertices")ret_i(m3g_transform_vertices(arg(0).ref,arg(1).ref,arg(2).ref,arg(3).i!=0));
        else if(name=="_transformTexCoords")ret_i(m3g_transform_texcoords(arg(0).ref,arg(1).ref));
        else if(name=="_multiply")ret_i(m3g_multiply(arg(1).ref,arg(2).ref,arg(3).ref));
        else die("native Transform."+name);
        return;
    }
    if(native_match(NativeClassId::C2,"javax/microedition/m3g/Graphics3D")) {
        int gfx=arg(0).ref;
        if(name=="_applyTexture238" && m->desc=="(FIII)Z" && (m->flags&0x0008)) { ret_i(m3g_apply_texture238(m->class_id,bits_f(arg(0).i),arg(1).i,arg(2).i,arg(3).i)); }
        else if(name=="_spanEnabled239" && m->desc=="()Z" && (m->flags&0x0008))ret_i(VMS_RS(g_m3g_span239_enabled));
        else if(name=="_rasterSpan239" && m->desc=="(IIIIIIIFFFFFFIIII)Z" && !(m->flags&0x0008)) {
            m3g_span239::Input in;
            in.index=arg(1).i;in.x=arg(2).i;in.y=arg(3).i;in.count=arg(4).i;
            in.color=arg(5).i;in.flags=arg(6).i;in.alpha_threshold=arg(7).i;
            in.z=bits_f(arg(8).i);in.z_step=bits_f(arg(9).i);in.pw=bits_f(arg(10).i);in.pw_step=bits_f(arg(11).i);
            in.inv_pw=bits_f(arg(12).i);in.inv_pw_step=bits_f(arg(13).i);
            in.fog=arg(14).i;in.fog_step=arg(15).i;in.fog_rb=arg(16).i;in.fog_g=arg(17).i;
            ret_i(m3g_span239::draw(gfx,m->class_id,in));
        }
        else if(name=="_spriteEnabled241" && m->desc=="()Z" && (m->flags&0x0008))ret_i(VMS_RS(g_m3g_sprite241_enabled));
        else if(name=="_spriteRow241" && m->desc=="(Ljavax/microedition/m3g/Image2D;IIIIIFFIIIIIII)Z" && !(m->flags&0x0008)) {
            m3g_sprite241::Input in;
            in.image=arg(1).ref;in.index=arg(2).i;in.count=arg(3).i;in.tex_row=arg(4).i;in.left=arg(5).i;in.width=arg(6).i;
            in.u=bits_f(arg(7).i);in.du=bits_f(arg(8).i);in.z=arg(9).i;in.flags=arg(10).i;in.alpha=arg(11).i;in.threshold=arg(12).i;
            in.fog=arg(13).i;in.fog_rb=arg(14).i;in.fog_g=arg(15).i;
            ret_i(m3g_sprite241::draw(gfx,m->class_id,in));
        }
        else if(name=="_rowEnabled240" && m->desc=="()Z" && (m->flags&0x0008))ret_i(VMS_RS(g_m3g_row240_enabled));
        else if(name=="_rasterRow240" && m->desc=="(IIIIIIIFFFFFFIIIIZIFFF)Z" && !(m->flags&0x0008)) {
            m3g_row240::Input in;
            in.index=arg(1).i;in.x=arg(2).i;in.y=arg(3).i;in.count=arg(4).i;
            in.color=arg(5).i;in.flags=arg(6).i;in.alpha_threshold=arg(7).i;
            in.z=bits_f(arg(8).i);in.z_step=bits_f(arg(9).i);in.pw=bits_f(arg(10).i);in.pw_step=bits_f(arg(11).i);
            in.inv_pw=bits_f(arg(12).i);in.inv_pw_step=bits_f(arg(13).i);
            in.fog=arg(14).i;in.fog_step=arg(15).i;in.fog_rb=arg(16).i;in.fog_g=arg(17).i;
            in.perspective=arg(18).i!=0;in.fog_mode=arg(19).i;
            in.fog_far_norm=bits_f(arg(20).i);in.inv_fog_div=bits_f(arg(21).i);in.fog_density=bits_f(arg(22).i);
            ret_i(m3g_row240::draw(gfx,m->class_id,in));
        }
        else if(name=="_clearToTarget")ret_i(m3g_clear_to_target(gfx,arg(1).ref,arg(2).ref,arg(3).i!=0));
        else if(name=="_drawTriangle")ret_i(m3g_draw_triangle(gfx,arg(1).ref,arg(2).ref,arg(3).i,
            arg(4).i!=0,arg(5).i!=0,arg(6).i!=0));
        else if(name=="_readTarget")ret_ref(m3g_read_target(gfx));
        else if(name=="_targetWidth"||name=="_targetHeight") {
            int w=0,h=0;pixels_of(m3g_target(gfx),w,h);ret_i(name=="_targetWidth"?w:h);
        } else if(name=="_writeTarget") {
            if(!m3g_write_target(gfx,arg(1).ref,arg(2).i,arg(3).i,arg(4).i,arg(5).i))
                throw_new("java/lang/IllegalArgumentException","M3G target dimensions");
        } else die("native Graphics3D."+name);
        return;
    }
    if(native_match(NativeClassId::C3,"javax/microedition/m3g/Triangle") && (m->flags&0x0008)) {
        if(name=="_gather242" && m->desc=="([F[[FLjavax/microedition/m3g/VertexArray;Ljavax/microedition/m3g/VertexBuffer;IIIIZI)I") {
            ret_i(m3g_assembly242::gather(m->class_id,arg(0).ref,arg(1).ref,arg(2).ref,arg(3).ref,arg(4).i,arg(5).i,arg(6).i,arg(7).i,arg(8).i!=0,arg(9).i));return;
        }
        if(name=="_emit242" && m->desc=="(Ljavax/microedition/m3g/Triangle;[F[[F[IIZZZI)Z") {
            ret_i(m3g_assembly242::emit(arg(0).ref,arg(1).ref,arg(2).ref,arg(3).ref,arg(4).i,arg(5).i!=0,arg(6).i!=0,arg(7).i!=0,arg(8).i));return;
        }
    }
    if(native_match(NativeClassId::C3,"javax/microedition/m3g/Triangle") && name=="_lighting242" && m->desc=="([FLjavax/microedition/m3g/VertexArray;Ljavax/microedition/m3g/Transform;Ljavax/microedition/m3g/Material;IZZLjavax/microedition/m3g/M3GVector;[F[FI[IIIIIIII[I)Z" && (m->flags&0x0008)) {
        m3g_lighting242::Input in{arg(0).ref,arg(1).ref,arg(2).ref,arg(3).ref,arg(4).i,arg(5).i!=0,arg(6).i!=0,
            arg(7).ref,arg(8).ref,arg(9).ref,arg(10).i,arg(11).ref,arg(12).i,
            {arg(13).i,arg(14).i,arg(15).i},{arg(16).i,arg(17).i,arg(18).i},arg(19).ref};
        ret_i(m3g_lighting242::draw(m->class_id,in));return;
    }
    if(native_match(NativeClassId::C3,"javax/microedition/m3g/Triangle")&&name=="_sortFrontToBack") {
        ret_i(m3g_sort_front_to_back(arg(0).ref,arg(1).i));return;
    }
    if(native_match(NativeClassId::C4,"javax/microedition/m3g/Loader")&&name=="_inflate") {ret_i(m3g_inflate(arg(0).ref,arg(1).ref));return;}
    if(native_match(NativeClassId::C5,"javax/microedition/m3g/M3GArrays") && (m->flags&0x0008)) {
        if(name=="_prepEnabled241" && m->desc=="()Z"){ret_i(VMS_RS(g_m3g_prep241_enabled));return;}
        if(name=="_convert241" && m->desc=="([IIII)Z"){ret_i(m3g_image241::convert(arg(0).ref,arg(1).i,arg(2).i,arg(3).i));return;}
        if(name=="_decode241" && m->desc=="([BI[B[IIIIZ)Z"){ret_i(m3g_image241::decode(arg(0).ref,arg(1).i,arg(2).ref,arg(3).ref,arg(4).i,arg(5).i,arg(6).i,arg(7).i!=0));return;}
        if(name=="_mipRow241" && m->desc=="(Ljavax/microedition/m3g/Image2D;Ljavax/microedition/m3g/Image2D;I)Z"){ret_i(m3g_image241::mip_row(arg(0).ref,arg(1).ref,arg(2).i));return;}
    }
    if(native_match(NativeClassId::C5,"javax/microedition/m3g/M3GArrays")&&(name=="_fillShort"||name=="_fillInt")) {
        int ref=arg(0).ref,from=arg(1).i,to=arg(2).i;
        if(!ref){throw_new("java/lang/NullPointerException","M3G fill");return;}
        if(from<0||to<from||size_t(to)>VMS_HEAP(g_heap)[ref].elems.size()){throw_new("java/lang/ArrayIndexOutOfBoundsException","M3G fill");return;}
        for(int i=from;i<to;++i)VMS_HEAP(g_heap)[ref].elems.set(i,arg(3));return;
    }
    if(native_match(NativeClassId::C6,"java/lang/Object")&&name=="clone") {
        int self=arg(0).ref;
        if(!VMS_HEAP(g_heap)[self].is_array&&!type_compatible(self,"java/lang/Cloneable")) {
            throw_new("java/lang/CloneNotSupportedException","clone");return;
        }
        int copy;
        if(VMS_HEAP(g_heap)[self].is_array) {
            std::string desc=VMS_HEAP(g_heap)[self].array_desc;size_t n=VMS_HEAP(g_heap)[self].elems.size();
            copy=alloc_array(desc,n);VMS_HEAP(g_heap)[copy].elems.copy_from(VMS_HEAP(g_heap)[self].elems,0,0,n);
        } else {copy=alloc_obj(VMS_HEAP(g_heap)[self].cls);const uint32_t gen=VMS_HEAP(g_heap)[copy].generation;VMS_HEAP(g_heap)[copy]=VMS_HEAP(g_heap)[self];VMS_HEAP(g_heap)[copy].generation=gen; // P2.5: identity stays the copy's own
            if(!VMS_HEAP(g_heap)[copy].pixels.empty()){VMS_HEAP(g_heap)[copy].image_layout_version=VMS_HEAP(g_heap)[copy].image_content_version=resver::tick();++resver::g_stats.image_layout;}}
        ret_ref(copy);return;
    }
    if (native_match(NativeClassId::C7,"java/lang/Throwable")) {
        int self = arg(0).ref;
        if (name == "fillInStackTrace") { capture_exception_trace(self); ret_ref(self); return; }
        if (name == "traceText") {
            auto it = VMS_DIAG(g_exception_traces).find(self);
            ret_ref(make_string(it == VMS_DIAG(g_exception_traces).end() ? "" : it->second)); return;
        }
    }
    if (native_match(NativeClassId::C8,"java/io/ConsoleOutputStream")) {
        int self = arg(0).ref;
        if (name == "write") {
            unsigned char ch = (unsigned char)arg(1).i;
            if (ch == '\n') flush_console(self, true);
            else {
                // Bound pending output even if a game never writes a newline.
                if (VMS_HEAP(g_heap)[self].text.size() >= 4096 && (ch & 0xc0) != 0x80) flush_console(self, false);
                VMS_HEAP(g_heap)[self].text.push_back((char)ch);
            }
            return;
        }
        if (name == "flush" || name == "close") { flush_console(self, false); return; }
    }
    if(cn=="javax/microedition/lcdui/Font") {
        const int self=arg(0).ref;const MidpFont font=midp_font(self);
        if(name=="init")midp_font_set(self,{arg(1).i,arg(2).i,arg(3).i});
        else if(name=="getHeight")ret_i(font.height());
        else if(name=="getBaselinePosition")ret_i(font.baseline());
        else if(name=="getFace")ret_i(font.face);
        else if(name=="getStyle")ret_i(font.style);
        else if(name=="getSize")ret_i(font.size);
        else if(name=="charWidth")ret_i(font.advance(uint16_t(arg(1).i)));
        else if(name=="stringWidth") {
            if(!arg(1).ref){throw_new("java/lang/NullPointerException","string");return;}
            ret_i(text_width(read_string(arg(1).ref),font));
        } else die("native Font."+name);
        return;
    }
    if (native_match(NativeClassId::C9,"javax/microedition/lcdui/Graphics")) {
        const auto target_entry=VMS_RS(g_gfx_target).find(arg(0).ref);
        const int target=target_entry==VMS_RS(g_gfx_target).end()?0:target_entry->second;
        GraphicsGlobalScope global_sample(VMS_RS(g_graphics_profile),target!=0);
        GraphicsProfileScope native_sample(VMS_RS(g_graphics_profile),name);
        int self=arg(0).ref;
        auto state=[&](const std::unordered_map<int,int>& values) {
            auto it=values.find(self);return it==values.end()?0:it->second;
        };
        auto dotted=[&]() {
            Field* f=find_field(m->class_id,"strokeStyle","I");
            return f&&VMS_HEAP(g_heap)[self].fields[f->slot].i==1;
        };
        int tx=state(VMS_RS(g_gfx_tx)),ty=state(VMS_RS(g_gfx_ty));
        auto coord=[](int a,int b){return int_bits(uint32_t(a)+uint32_t(b));};
        int tw=0,th=0;
        auto anchor_ok=[](int anchor,bool text) {
            if(anchor==0)return true;
            int horizontal=anchor&(1|4|8),vertical=anchor&(2|16|32|64);
            return !(anchor&~127)&&(horizontal==1||horizontal==4||horizontal==8)&&
                (text?(vertical==16||vertical==32||vertical==64):(vertical==2||vertical==16||vertical==32));
        };
        if(name=="setFont")midp_font_set(self,midp_font(arg(1).ref),true);
        else if(name=="getFont") {
            const MidpFont font=midp_font(self,true);
            const int ref=alloc_obj(ensure_class("javax/microedition/lcdui/Font"));
            midp_font_set(ref,font);ret_ref(ref);
        } else if(name=="setColor") {
            if(m->desc.find("III")!=std::string::npos) {
                if(arg(1).i<0||arg(1).i>255||arg(2).i<0||arg(2).i>255||arg(3).i<0||arg(3).i>255) {
                    throw_new("java/lang/IllegalArgumentException","color component");return;
                }
                VMS_RS(g_gfx_color)[self]=(arg(1).i<<16)|(arg(2).i<<8)|arg(3).i;
            } else VMS_RS(g_gfx_color)[self]=arg(1).i&0xffffff;
            gfx_opacity_set(self,255);
        } else if(name=="getColor")ret_i(state(VMS_RS(g_gfx_color))&0xffffff);
        else if(name=="fillRect") {
            int color=state(VMS_RS(g_gfx_color));
            int x=coord(arg(1).i,tx),y=coord(arg(2).i,ty);
            note_rect(x,y,arg(3).i,arg(4).i,color&0xffffff);
            { p86::Scope p86_scope(p86::FillRect,target!=0,uint64_t(std::max(0,arg(3).i))*uint64_t(std::max(0,arg(4).i))); fill_rect(self,target,x,y,arg(3).i,arg(4).i,color); }
        } else if(name=="drawRect"||name=="fillArc"||name=="drawArc"||name=="drawRoundRect"||name=="fillRoundRect") {
            int color=state(VMS_RS(g_gfx_color));auto* px=pixels_of(target,tw,th);
            int x=coord(arg(1).i,tx),y=coord(arg(2).i,ty),w=arg(3).i,h=arg(4).i;
            if(px) {
                auto surface=gfx_surface(self,target,*px,tw,th);
                bool is_dotted=dotted();
                if(name=="drawRect")raster_primitives::rect(surface,x,y,w,h,uint32_t(color),true,is_dotted);
                else if(name=="fillArc"||name=="drawArc")raster_primitives::arc(surface,x,y,w,h,arg(5).i,arg(6).i,uint32_t(color),name=="drawArc",is_dotted);
                else raster_primitives::round_rect(surface,x,y,w,h,arg(5).i,arg(6).i,uint32_t(color),name=="drawRoundRect",is_dotted);
            }
            note_rect(x,y,w,h,color&0xffffff);
        } else if(name=="drawLine") {
            int color=state(VMS_RS(g_gfx_color));auto* px=pixels_of(target,tw,th);
            if(VMS_RS(g_drawacc).on)VMS_RS(g_drawacc).lines++;
            if(px) {
                auto surface=gfx_surface(self,target,*px,tw,th);
                raster_primitives::line(surface,coord(arg(1).i,tx),coord(arg(2).i,ty),coord(arg(3).i,tx),coord(arg(4).i,ty),uint32_t(color),dotted());
            }
        } else if(name=="fillTriangle") {
            int color=state(VMS_RS(g_gfx_color));auto* px=pixels_of(target,tw,th);
            if(px) {
                auto surface=gfx_surface(self,target,*px,tw,th);
                raster_primitives::triangle(surface,coord(arg(1).i,tx),coord(arg(2).i,ty),coord(arg(3).i,tx),coord(arg(4).i,ty),coord(arg(5).i,tx),coord(arg(6).i,ty),uint32_t(color));
            }
        } else if(name=="drawString") {
            int color=state(VMS_RS(g_gfx_color));
            if(!arg(1).ref){throw_new("java/lang/NullPointerException","string");return;}
            if(!anchor_ok(arg(4).i,true)){throw_new("java/lang/IllegalArgumentException","text anchor");return;}
            const std::string& text=read_string(arg(1).ref);
            int x=coord(arg(2).i,tx),y=coord(arg(3).i,ty);
            const MidpFont font=midp_font(self,true);
            const int anchor=arg(4).i;
            apply_anchor(anchor&~64,x,y,text_width(text,font),font.height());
            if(anchor&64)y=int_bits(uint32_t(y)-uint32_t(font.baseline()));
            note_text(text);draw_string(self,target,x,y,color,text);
        } else if(name=="drawRegion"||name=="drawImage") {
            int img=arg(1).ref;
            if(!img){throw_new("java/lang/NullPointerException","image");return;}
            int sx=0,sy=0,w=VMS_HEAP(g_heap)[img].width,h=VMS_HEAP(g_heap)[img].height,transform=0;
            int x=coord(arg(2).i,tx),y=coord(arg(3).i,ty),anchor=arg(4).i;
            if(name=="drawRegion") {
                sx=arg(2).i;sy=arg(3).i;w=arg(4).i;h=arg(5).i;transform=arg(6).i;
                x=coord(arg(7).i,tx);y=coord(arg(8).i,ty);anchor=arg(9).i;
                if(target==img||sx<0||sy<0||w<0||h<0||int64_t(sx)+w>VMS_HEAP(g_heap)[img].width||int64_t(sy)+h>VMS_HEAP(g_heap)[img].height||transform<0||transform>7) {
                    throw_new("java/lang/IllegalArgumentException","image region");return;
                }
            }
            if(!anchor_ok(anchor,false)){throw_new("java/lang/IllegalArgumentException","image anchor");return;}
            apply_anchor(anchor,x,y,transform>=4?h:w,transform>=4?w:h);
            draw_region(self,target,img,sx,sy,w,h,transform,x,y);
        } else if(name=="copyArea") {
            if(target==0){throw_new("java/lang/IllegalStateException","copyArea on display");return;}
            auto* px=pixels_of(target,tw,th);
            int sx=coord(arg(1).i,tx),sy=coord(arg(2).i,ty),w=arg(3).i,h=arg(4).i;
            int dx=coord(arg(5).i,tx),dy=coord(arg(6).i,ty),anchor=arg(7).i;
            if(!anchor_ok(anchor,false)){throw_new("java/lang/IllegalArgumentException","copy anchor");return;}
            apply_anchor(anchor,dx,dy,w,h);
            if(!px||!pixel_compat236::copy(*px,tw,th,sx,sy,w,h,dx,dy,gfx_bounds(self,tw,th),&VMS_HEAP(g_heap)[target].alpha))
                throw_new("java/lang/IllegalArgumentException","copy source bounds");
        } else if(name=="drawRGB") {
            int array=arg(1).ref;
            if(!array){throw_new("java/lang/NullPointerException","RGB data");return;}
            if(!raster_primitives::array_range(VMS_HEAP(g_heap)[array].elems.size(),arg(2).i,arg(3).i,arg(6).i,arg(7).i)) {
                throw_new("java/lang/ArrayIndexOutOfBoundsException","RGB range");return;
            }
            blit_argb(self,target,tx,ty,array,arg(2).i,arg(3).i,arg(4).i,arg(5).i,arg(6).i,arg(7).i,0,arg(8).i!=0);
        } else if(name=="setClip"||name=="clipRect") {
            pixels_of(target,tw,th);
            int64_t x=coord(arg(1).i,tx),y=coord(arg(2).i,ty);
            auto b=raster_primitives::intersect({0,0,tw,th},{x,y,x+std::max(0,arg(3).i),y+std::max(0,arg(4).i)});
            if(name=="clipRect")b=raster_primitives::intersect(b,gfx_bounds(self,tw,th));
            int left=int(std::max(int64_t(0),std::min(int64_t(tw),b.left)));
            int top=int(std::max(int64_t(0),std::min(int64_t(th),b.top)));
            VMS_RS(g_clip)[self]=Clip{left,top,int(std::max(int64_t(0),b.right-left)),int(std::max(int64_t(0),b.bottom-top)),true};
            if(VMS_RS(g_drawacc).on)VMS_RS(g_drawacc).clips++;
        } else if(name=="getClipX"||name=="getClipY"||name=="getClipWidth"||name=="getClipHeight") {
            auto it=VMS_RS(g_clip).find(self);const Clip* clip=it!=VMS_RS(g_clip).end()&&it->second.on?&it->second:nullptr;
            if(name=="getClipX")ret_i(coord(clip?clip->x:0,int_bits(0-uint32_t(tx))));
            else if(name=="getClipY")ret_i(coord(clip?clip->y:0,int_bits(0-uint32_t(ty))));
            else {pixels_of(target,tw,th);ret_i(name=="getClipWidth"?(clip?std::max(0,clip->w):tw):(clip?std::max(0,clip->h):th));}
        }
        else if(name=="translate"){VMS_RS(g_gfx_tx)[self]=coord(tx,arg(1).i);VMS_RS(g_gfx_ty)[self]=coord(ty,arg(2).i);}
        else if(name=="getTranslateX")ret_i(tx);
        else if(name=="getTranslateY")ret_i(ty);
        else die("native Graphics."+name);
        return;
    }
    if (native_match(NativeClassId::C10,"com/nokia/mid/ui/DirectGraphics")) {
        int self = arg(0).ref, gfx = 0;
        if (Field* f = find_field(VMS_HEAP(g_heap)[self].cls, "g", "Ljavax/microedition/lcdui/Graphics;"))
            gfx = VMS_HEAP(g_heap)[self].fields[f->slot].ref;
        int target = VMS_RS(g_gfx_target).count(gfx) ? VMS_RS(g_gfx_target)[gfx] : 0;
        int tx = VMS_RS(g_gfx_tx).count(gfx) ? VMS_RS(g_gfx_tx)[gfx] : 0;
        int ty = VMS_RS(g_gfx_ty).count(gfx) ? VMS_RS(g_gfx_ty)[gfx] : 0;
        if (name == "setARGBColor") { if (gfx) {VMS_RS(g_gfx_color)[gfx] = arg(1).i;gfx_opacity_set(gfx,uint32_t(arg(1).i)>>24);} }
        else if(name=="getAlphaComponent")ret_i(gfx_opacity(gfx));
        else if(name=="drawTriangle"||name=="fillTriangle"||name=="drawPolygon"||name=="fillPolygon") {
            int tw=0,th=0;auto* px=pixels_of(target,tw,th);if(!px)return;
            std::vector<std::pair<int64_t,int64_t>> points;
            uint32_t color;
            if(name=="drawTriangle"||name=="fillTriangle") {
                for(int i=1;i<7;i+=2)points.push_back({int64_t(arg(i).i)+tx,int64_t(arg(i+1).i)+ty});
                color=uint32_t(arg(7).i);
            } else {
                int ax=arg(1).ref,ay=arg(3).ref,ox=arg(2).i,oy=arg(4).i,n=arg(5).i;
                if(!ax||!ay){throw_new("java/lang/NullPointerException","Nokia polygon");return;}
                if(n<0||ox<0||oy<0||uint64_t(ox)+uint64_t(n)>VMS_HEAP(g_heap)[ax].elems.size()||uint64_t(oy)+uint64_t(n)>VMS_HEAP(g_heap)[ay].elems.size()){
                    throw_new("java/lang/ArrayIndexOutOfBoundsException","Nokia polygon bounds");return;
                }
                if(n>65536){throw_new("java/lang/OutOfMemoryError","Nokia polygon work limit");return;}
                points.reserve(n);for(int i=0;i<n;++i)points.push_back({int64_t(VMS_HEAP(g_heap)[ax].elems.get(ox+i).i)+tx,int64_t(VMS_HEAP(g_heap)[ay].elems.get(oy+i).i)+ty});
                color=uint32_t(arg(6).i);
            }
            auto surface=gfx_surface(gfx,target,*px,tw,th);surface.opacity=color>>24;
            if(name=="drawTriangle"||name=="drawPolygon") {
                if(points.size()==1)surface.put(points[0].first,points[0].second,color);
                for(size_t i=0;i<points.size();++i) {
                    auto a=points[i],b=points[(i+1)%points.size()];raster_primitives::line(surface,a.first,a.second,b.first,b.second,color,false);
                }
            } else if(name=="fillTriangle") {
                raster_primitives::triangle(surface,points[0].first,points[0].second,points[1].first,points[1].second,points[2].first,points[2].second,color);
            } else if(points.size()>=3) {
                // Even-odd scan conversion supports concave polygons. Work is
                // bounded by visible height, never by untrusted coordinates.
                auto bounds=surface.bounds();std::vector<long double> intersections;intersections.reserve(points.size());
                for(int64_t yy=bounds.top;yy<bounds.bottom;++yy) {
                    intersections.clear();long double cy=static_cast<long double>(yy)+0.5L;
                    for(size_t i=0;i<points.size();++i) {
                        auto a=points[i],b=points[(i+1)%points.size()];if(a.second>b.second)std::swap(a,b);
                        if(cy<a.second||cy>=b.second||a.second==b.second)continue;
                        intersections.push_back(a.first+(cy-a.second)*static_cast<long double>(b.first-a.first)/(b.second-a.second));
                    }
                    std::sort(intersections.begin(),intersections.end());
                    for(size_t i=1;i<intersections.size();i+=2) {
                        int64_t left=std::max(bounds.left,int64_t(std::ceil(intersections[i-1]-0.5L)));
                        int64_t right=std::min(bounds.right,int64_t(std::ceil(intersections[i]-0.5L)));
                        for(int64_t xx=left;xx<right;++xx)surface.put(xx,yy,color);
                    }
                }
            }
        }
        else if(name=="drawImage") {
            int img=arg(1).ref,t=pixel_compat236::transform(arg(5).i);
            if(!img){throw_new("java/lang/NullPointerException","Nokia image");return;}
            int anchor=arg(4).i,horizontal=anchor&13,vertical=anchor&50;
            if(t<0||(anchor&&((anchor&~63)||(horizontal!=1&&horizontal!=4&&horizontal!=8)||(vertical!=2&&vertical!=16&&vertical!=32)))) {
                throw_new("java/lang/IllegalArgumentException","Nokia transform or anchor");return;
            }
            int w=VMS_HEAP(g_heap)[img].width,h=VMS_HEAP(g_heap)[img].height;
            int x=int_bits(uint32_t(arg(2).i)+uint32_t(tx)),y=int_bits(uint32_t(arg(3).i)+uint32_t(ty));
            apply_anchor(anchor,x,y,t>=4?h:w,t>=4?w:h);
            draw_region(gfx,target,img,0,0,w,h,t,x,y);
        } else if (name == "drawPixels") {
            int arr=arg(1).ref,w=arg(7).i,h=arg(8).i,format=arg(10).i;
            if(!arr){throw_new("java/lang/NullPointerException","Nokia pixels");return;}
            if(w<0||h<0||pixel_compat236::transform(arg(9).i)<0||(format!=888&&format!=8888&&format!=0x8888)) {
                throw_new("java/lang/IllegalArgumentException","Nokia pixel format/shape");return;
            }
            if(!raster_primitives::array_range(VMS_HEAP(g_heap)[arr].elems.size(),arg(3).i,arg(4).i,w,h)){
                throw_new("java/lang/ArrayIndexOutOfBoundsException","Nokia pixel range");return;
            }
            blit_argb(gfx,target,tx,ty,arr,arg(3).i,arg(4).i,arg(5).i,arg(6).i,w,h,arg(9).i,arg(2).i!=0&&format!=888);
        } else if (name == "getPixels") {
            // Keep the old boolean overload for existing snapshots/classes;
            // the public Nokia overload omits it.
            const int base=m->desc.find("[IZ")!=std::string::npos?3:2;
            int arr=arg(1).ref,w=arg(base+4).i,h=arg(base+5).i,format=arg(base+6).i;
            if(!arr){throw_new("java/lang/NullPointerException","Nokia pixels");return;}
            int tw=0,th=0;pixels_of(target,tw,th);
            int64_t x=int64_t(arg(base+2).i)+tx,y=int64_t(arg(base+3).i)+ty;
            if(w<0||h<0||x<0||y<0||x+w>tw||y+h>th||(format!=888&&format!=8888&&format!=0x8888)){
                throw_new("java/lang/IllegalArgumentException","Nokia getPixels bounds/format");return;
            }
            if(!raster_primitives::array_range(VMS_HEAP(g_heap)[arr].elems.size(),arg(base).i,arg(base+1).i,w,h)){
                throw_new("java/lang/ArrayIndexOutOfBoundsException","Nokia getPixels range");return;
            }
            read_argb(target,tx,ty,arr,arg(base).i,arg(base+1).i,arg(base+2).i,arg(base+3).i,w,h);
        } else die("native DirectGraphics." + name);
        return;
    }
    if (native_match(NativeClassId::C11,"javax/microedition/media/NativePlayer")) {
        int h = arg(0).i;
        if(name=="nExtendedAudio") { ret_i(audio_ffmpeg236::available()); }
        else if(name=="nCodecSupported") {ret_i(arg(0).ref&&audio_ffmpeg236::supports(read_string(arg(0).ref)));}
        else if (name == "nLoad") {
            int array = arg(0).ref;
            if (!array) { ret_i(0); return; }
            const auto& bytes = VMS_HEAP(g_heap)[array].elems;
            if (bytes.size() > 8 * 1024 * 1024) { ret_i(0); return; }
            std::vector<uint8_t> data(bytes.size());
            for (size_t i=0;i<data.size();++i) data[i]=(uint8_t)bytes.get(i).i;
            VMS_PLAT(g_audio).setDecoder(audio_ffmpeg236::decode);
            std::string error; int id=VMS_PLAT(g_audio).load(data,read_string(arg(1).ref),error);
            if (!id) log_line("AUDIO decode rejected " + error);
            else audio_event("load handle="+std::to_string(id)+" bytes="+std::to_string(data.size())+
                 " format="+read_string(arg(1).ref)+" duration_us="+std::to_string(VMS_PLAT(g_audio).duration(id)));
            ret_i(id);
        } else if (name == "nStart") {
            int old=VMS_PLAT(g_audio).state(h);bool ok=VMS_PLAT(g_audio).start(h);ret_i(ok);
            if(ok && old!=j2me_audio::Engine::Started)audio_event("start handle="+std::to_string(h)+" media_us="+std::to_string(VMS_PLAT(g_audio).time(h)));
        } else if (name == "nStop") {
            int old=VMS_PLAT(g_audio).state(h);bool ok=VMS_PLAT(g_audio).stop(h);ret_i(ok);
            if(ok && old==j2me_audio::Engine::Started)audio_event("stop handle="+std::to_string(h)+" media_us="+std::to_string(VMS_PLAT(g_audio).time(h)));
        } else if (name == "nClose") {
            if(VMS_PLAT(g_audio).close(h))audio_event("close handle="+std::to_string(h));
        } else if (name == "nSeek") {
            int64_t value=VMS_PLAT(g_audio).seek(h,arg(1).l);ret_l(value);
            audio_event("seek handle="+std::to_string(h)+" media_us="+std::to_string(value));
        }
        else if (name == "nTime") ret_l(VMS_PLAT(g_audio).time(h));
        else if (name == "nDuration") ret_l(VMS_PLAT(g_audio).duration(h));
        else if (name == "nRate") {
            int control=arg(2).ref;
            const Field* rate=control>0 && size_t(control)<VMS_HEAP(g_heap).size()?find_field(VMS_HEAP(g_heap)[control].cls,"rate","I"):nullptr;
            if(!rate || rate->slot<0 || size_t(rate->slot)>=VMS_HEAP(g_heap)[control].fields.size()) {ret_i(-1);return;}
            int applied=VMS_PLAT(g_audio).rate(h,arg(1).i);
            // One native boundary updates both saved Java metadata and the mixer.
            if(applied>=0) VMS_HEAP(g_heap)[control].fields[rate->slot]=Value::of_i(applied);
            ret_i(applied);
        }
        else if (name == "nVolume") {
            int old=VMS_PLAT(g_audio).volume(h),level=VMS_PLAT(g_audio).volume(h,arg(1).i);ret_i(level);
            if(level>=0 && level!=old)audio_event("volume handle="+std::to_string(h)+" level="+std::to_string(level));
        }
        else if (name == "nLoop") {
            bool ok=VMS_PLAT(g_audio).loop(h,arg(1).i);ret_i(ok);
            if(ok)audio_event("loop handle="+std::to_string(h)+" count="+std::to_string(arg(1).i));
        }
        else if (name == "nState") ret_i(VMS_PLAT(g_audio).state(h));
        else if (name == "nTone") {
            int id=VMS_PLAT(g_audio).playTone(h,arg(1).i,arg(2).i);
            if (id>0) {VMS_PLAT(g_tones).insert(id);audio_event("tone handle="+std::to_string(id)+" note="+std::to_string(h)+" duration_ms="+std::to_string(arg(1).i));}
            ret_i(id>0);
        } else die("native NativePlayer."+name);
        return;
    }
    if(native_match(NativeClassId::C12,"javax/microedition/lcdui/Displayable") && (name=="getWidth"||name=="getHeight")) {
        ret_i(name=="getWidth"?VMS_RS(g_screen_w):VMS_RS(g_screen_h));return;
    }
    if (native_match(NativeClassId::C12,"javax/microedition/lcdui/Displayable") && name == "invalidate") {
        if(arg(0).ref==VMS_RS(g_current))VMS_RS(g_repaint)=true;return;
    }
    if (native_match(NativeClassId::C12,"javax/microedition/lcdui/Displayable") && name == "isShown") {
        ret_i(VMS_RS(g_shown) == arg(0).ref && VMS_RS(g_shown) != 0 && !VMS_PLAT(g_host_paused) &&
              (VMS_EXEC(g_midlet_state) == MidletState::Active || VMS_EXEC(g_midlet_state) == MidletState::Empty)); return;
    }
    if (native_match(NativeClassId::C13,"javax/microedition/lcdui/Display")) {
        if (name == "getDisplay") {
            int cid = ensure_class("javax/microedition/lcdui/Display");
            if (VMS_RS(g_display) <= 0 || VMS_RS(g_display) >= (int)VMS_HEAP(g_heap).size() || VMS_HEAP(g_heap)[VMS_RS(g_display)].cls != cid) VMS_RS(g_display) = alloc_obj(cid);
            ret_ref(VMS_RS(g_display));
        } else if (name == "setCurrent0") {
            if(VMS_RS(g_current)!=arg(1).ref) {
                // Release a certified gate while its drawing owner still owns
                // the budget; the new Canvas can immediately paint as host.
                speed_invalidate_group(VMS_EXEC(g_main_thread));
                for(auto& t:VMS_EXEC(g_threads))speed_invalidate_group(t);
                VMS_EXEC(g_exec_ctx).frame_wait_request_owner=0;
                VMS_EXEC(g_main_thread).event_frame_sleep.reset();
                for(auto& t:VMS_EXEC(g_threads))t.event_frame_sleep.reset();
                VMS_EXEC(g_main_thread).compensated_sleep.reset();VMS_EXEC(g_main_thread).cached_elapsed.reset();VMS_EXEC(g_main_thread).frame_prep.reset();VMS_EXEC(g_main_thread).wait_frame.reset();VMS_EXEC(g_main_thread).residual_sleep.reset();VMS_EXEC(g_main_thread).rechecked_deadline.reset();VMS_EXEC(g_main_thread).field_gate_anchor.reset();VMS_EXEC(g_main_thread).frame_due_us=0;VMS_EXEC(g_main_thread).sleep_paced=false;
                VMS_EXEC(g_main_thread).frame_sleep_bridge=VMS_EXEC(g_main_thread).mixed_wait=false;
                for(auto& t:VMS_EXEC(g_threads)) {t.compensated_sleep.reset();t.cached_elapsed.reset();t.frame_prep.reset();t.wait_frame.reset();t.residual_sleep.reset();t.rechecked_deadline.reset();t.field_gate_anchor.reset();t.frame_due_us=0;t.sleep_paced=false;t.frame_sleep_bridge=t.mixed_wait=false;}
            }
            VMS_RS(g_current)=arg(1).ref; VMS_RS(g_alert_return)=arg(3).ref;
            VMS_RS(g_alert_remaining)=-1;
            VMS_RS(g_alert_deadline)=arg(2).i>0 ? monotonic_ms()+arg(2).i : 0;
            if(VMS_EXEC(g_midlet_state)==MidletState::Paused && arg(2).i>0) {VMS_RS(g_alert_remaining)=arg(2).i;VMS_RS(g_alert_deadline)=0;}
            VMS_RS(g_repaint)=true;
            // Exact reviewed intro assumes visibility callbacks before its
            // post-setCurrent disposal. Deliver the real transition, rather
            // than suppressing its hideNotify exception or modifying Java.
            if(VMS_PLAT(g_rms_suite)=="0e5e8024cbe7405614aff5d1366f6b9fc6bb1824023ecedbf03dc973c26521fe" &&
               VMS_RS(g_current)!=VMS_RS(g_shown) && !VMS_EXEC(g_lifecycle_busy)) {
                VMS_EXEC(g_lifecycle_busy)=true;display_visibility();VMS_EXEC(g_lifecycle_busy)=false;
            }
        } else if (name == "getCurrent") ret_ref(VMS_RS(g_current));
        else if (name == "callSerially") {
            if (!arg(1).ref) { throw_new("java/lang/NullPointerException","runnable"); return; }
            p1_serial_enqueue();VMS_EXEC(g_serial_calls).push_back(arg(1).ref);cont_serial_enqueued(VMS_EXEC(g_serial_calls).size());
        } else die("native Display." + name);
        return;
    }
    if(cn=="com/nokia/mid/ui/DirectUtils"&&name=="createImage") {
        int w=0,h=0;std::vector<uint8_t> rgb,alpha;
        if(m->desc.find("[B")!=std::string::npos) {
            int array=arg(0).ref,offset=arg(1).i,length=arg(2).i;
            if(!array){throw_new("java/lang/NullPointerException","Nokia image bytes");return;}
            if(offset<0||length<0||uint64_t(offset)+uint64_t(length)>VMS_HEAP(g_heap)[array].elems.size()) {throw_new("java/lang/ArrayIndexOutOfBoundsException","Nokia image bytes");return;}
            std::vector<uint8_t> bytes(length);for(int i=0;i<length;++i)bytes[i]=uint8_t(VMS_HEAP(g_heap)[array].elems.get(offset+i).i);
            std::string error;if(!decode_image(bytes,rgb,alpha,w,h,error)){throw_new("java/lang/IllegalArgumentException",error);return;}
        } else {
            w=arg(0).i;h=arg(1).i;uint32_t color=uint32_t(arg(2).i);
            if(w<=0||h<=0){throw_new("java/lang/IllegalArgumentException","Nokia image dimensions");return;}
            if(uint64_t(w)*uint64_t(h)>16777216){throw_new("java/lang/OutOfMemoryError","Nokia image too large");return;}
            rgb.resize(size_t(w)*h*3);alpha.assign(size_t(w)*h,uint8_t(color>>24));
            for(size_t i=0;i<alpha.size();++i){rgb[i*3]=uint8_t(color>>16);rgb[i*3+1]=uint8_t(color>>8);rgb[i*3+2]=uint8_t(color);}
        }
        install_image(m,std::move(rgb),std::move(alpha),w,h,fr,true);return;
    }
    if (native_match(NativeClassId::C14,"javax/microedition/lcdui/Image") && name == "createImage") {
        p86::Scope p86_scope(p86::CreateImage,true);
        int w=0,h=0;std::vector<uint8_t> rgb,alpha;bool mutable_image=false;
        if(m->desc.find("String")!=std::string::npos) {
            if(!arg(0).ref){throw_new("java/lang/NullPointerException","image name");return;}
            p86_scope.cat=p86::CreateNamed;
            std::string path=read_string(arg(0).ref);auto* data=jar_lookup(path);
            if(!data){log_line("IMG miss "+path);throw_new("java/io/IOException",path);return;}
            std::string err;
            const int64_t started=paint_clock_us();
            const bool hit=J2ME_OPT_IMAGE_CACHE && VMS_RS(g_decoded_images).get(data,rgb,alpha,w,h);
            if(hit)VMS_RS(g_image_copy_us)+=uint64_t(std::max<int64_t>(0,paint_clock_us()-started));
            else {
                if(!decode_image(*data,rgb,alpha,w,h,err)){log_line("IMG bad "+path+" "+err);throw_new("java/io/IOException",path);return;}
                VMS_RS(g_image_decode_us)+=uint64_t(std::max<int64_t>(0,paint_clock_us()-started));
                if(J2ME_OPT_IMAGE_CACHE)VMS_RS(g_decoded_images).put(data,rgb,alpha,w,h);
            }
            log_line("IMG "+path+" "+std::to_string(w)+"x"+std::to_string(h)+" cache="+(hit?"hit":"miss"));
        } else if(m->desc.find("[B")!=std::string::npos) {
            p86_scope.cat=p86::CreateBytes;
            int arr=arg(0).ref,off=arg(1).i,len=arg(2).i;
            if(!arr){throw_new("java/lang/NullPointerException","image bytes");return;}
            if(off<0||len<0||size_t(off)>VMS_HEAP(g_heap)[arr].elems.size()||size_t(len)>VMS_HEAP(g_heap)[arr].elems.size()-size_t(off)) {
                throw_new("java/lang/ArrayIndexOutOfBoundsException","image bytes range");return;
            }
            std::vector<uint8_t> bytes(len);
            for(int i=0;i<len;++i)bytes[i]=uint8_t(VMS_HEAP(g_heap)[arr].elems.get(off+i).i);
            std::string err;
            if(!decode_image(bytes,rgb,alpha,w,h,err)){throw_new("java/lang/IllegalArgumentException",err);return;}
        } else {
            p86_scope.cat=p86::CreateMutable;
            w=arg(0).i;h=arg(1).i;
            if(w<=0||h<=0){throw_new("java/lang/IllegalArgumentException","image dimensions");return;}
            if(uint64_t(w)*uint64_t(h)>16777216){throw_new("java/lang/OutOfMemoryError","image too large");return;}
            p86_scope.px=uint64_t(w)*uint64_t(h);
            rgb.assign(size_t(w)*size_t(h)*3,255);mutable_image=true;
        }
        install_image(m,std::move(rgb),std::move(alpha),w,h,fr,mutable_image);return;
    }
    if(native_match(NativeClassId::C14,"javax/microedition/lcdui/Image")&&name=="createRGBImage") {
        int arr=arg(0).ref,w=arg(1).i,h=arg(2).i;
        if(!arr){throw_new("java/lang/NullPointerException","RGB data");return;}
        if(w<=0||h<=0){throw_new("java/lang/IllegalArgumentException","image dimensions");return;}
        uint64_t count=uint64_t(w)*uint64_t(h);
        auto& e=VMS_HEAP(g_heap)[arr].elems;
        if(count>e.size()){throw_new("java/lang/ArrayIndexOutOfBoundsException","RGB data length");return;}
        if(count>16777216){throw_new("java/lang/OutOfMemoryError","image too large");return;}
        bool process=arg(3).i!=0;
        std::vector<uint8_t> rgb(size_t(count)*3,0),alpha(size_t(count),255);
        for(size_t i=0;i<size_t(count);++i) {
            uint32_t argb=uint32_t(e.get(i).i);alpha[i]=process?uint8_t(argb>>24):255;
            rgb[i*3]=uint8_t(argb>>16);rgb[i*3+1]=uint8_t(argb>>8);rgb[i*3+2]=uint8_t(argb);
        }
        install_image(m,std::move(rgb),std::move(alpha),w,h,fr);return;
    }
    if(native_match(NativeClassId::C14,"javax/microedition/lcdui/Image")&&name=="getRGB") {
        int self=arg(0).ref,array=arg(1).ref,offset=arg(2).i,scan=arg(3).i;
        int x=arg(4).i,y=arg(5).i,w=arg(6).i,h=arg(7).i;
        if(!array){throw_new("java/lang/NullPointerException","RGB data");return;}
        if(x<0||y<0||int64_t(x)+w>VMS_HEAP(g_heap)[self].width||int64_t(y)+h>VMS_HEAP(g_heap)[self].height) {
            throw_new("java/lang/IllegalArgumentException","image region");return;
        }
        if(w<=0||h<=0)return;
        if(std::abs(int64_t(scan))<w){throw_new("java/lang/IllegalArgumentException","RGB scanlength");return;}
        if(!raster_primitives::array_range(VMS_HEAP(g_heap)[array].elems.size(),offset,scan,w,h)) {
            throw_new("java/lang/ArrayIndexOutOfBoundsException","RGB range");return;
        }
        read_argb(self,0,0,array,offset,scan,x,y,w,h);return;
    }
    if(native_match(NativeClassId::C14,"javax/microedition/lcdui/Image")&&name=="getGraphics") {
        int self=arg(0).ref;Field* field=find_field(m->class_id,"mutable","Z");
        if(field&&!VMS_HEAP(g_heap)[self].fields[field->slot].i){throw_new("java/lang/IllegalStateException","immutable image");return;}
        int gid=alloc_obj(ensure_class("javax/microedition/lcdui/Graphics"));
        gfx_bind(gid,self,0);ret_ref(gid);return;
    }
    if (native_match(NativeClassId::C15,"javax/microedition/lcdui/game/GameCanvas")) {
        if (name == "getBuffer") {
            ret_ref(game_buffer(arg(0).ref));
        } else if (name == "getWidth") {
            ret_i(VMS_RS(g_screen_w));
        } else if (name == "getHeight") {
            ret_i(VMS_RS(g_screen_h));
        } else if (name == "getGraphics") {
            int buf = game_buffer(arg(0).ref);
            int gid = alloc_obj(ensure_class("javax/microedition/lcdui/Graphics"));
            gfx_bind(gid, buf, 0);
            if (VMS_DIAG(g_log_draw) && !VMS_RS(g_drawacc).on) draw_begin();
            ret_ref(gid);
        } else if (name == "flushGraphics") {
            const int canvas = arg(0).ref;
            if (canvas != VMS_RS(g_current) || canvas != VMS_RS(g_shown) || VMS_PLAT(g_host_paused) || VMS_EXEC(g_notify_destroyed) ||
                VMS_EXEC(g_midlet_state) == MidletState::Paused || VMS_EXEC(g_midlet_state) == MidletState::Destroyed) return;
            int x = 0, y = 0, w = VMS_RS(g_screen_w), h = VMS_RS(g_screen_h);
            if (m->desc == "(IIII)V") { x = arg(1).i; y = arg(2).i; w = arg(3).i; h = arg(4).i; }
            if (w <= 0 || h <= 0 || x >= VMS_RS(g_screen_w) || y >= VMS_RS(g_screen_h) ||
                int64_t(x) + w <= 0 || int64_t(y) + h <= 0) return;
            const int64_t frame_started_us = speed_begin_frame(&current_context());
            if (!show_buffer(game_buffer(canvas), x, y, w, h)) return;
            speed_note_paint(frame_started_us, &current_context());
            if (speed_period_us() && VMS_EXEC(g_can_suspend) && !current_context().auto_fps.grouped_update()) VMS_EXEC(g_yield) = true;
            std::string who = (arg(0).ref > 0 && VMS_HEAP(g_heap)[arg(0).ref].cls >= 0) ? VMS_CL(g_classes)[VMS_HEAP(g_heap)[arg(0).ref].cls].name : "GameCanvas";
            draw_end(who);
            if (VMS_DIAG(g_log_draw)) draw_begin();
        } else if (name == "getKeyStates") {
            const int canvas = arg(0).ref;
            int states = 0;
            if (canvas == VMS_RS(g_shown) && canvas == VMS_RS(g_current) && !VMS_PLAT(g_host_paused) &&
                (VMS_EXEC(g_midlet_state) == MidletState::Active || VMS_EXEC(g_midlet_state) == MidletState::Empty)) {
                Value* held = game_canvas_state(canvas, "keyHeld");
                Value* latched = game_canvas_state(canvas, "keyLatched");
                if (held && latched) { states = held->i | latched->i; latched->i = 0; }
            }
            ret_i(states);
        } else die("native GameCanvas." + name);
        return;
    }
    if(native_match(NativeClassId::C16,"javax/microedition/lcdui/Canvas") && name=="setFullScreenMode") {
        const int self=arg(0).ref;ScreenMeta meta=screen_meta(VMS_HEAP(g_heap)[self].text);meta.full=arg(1).i!=0;screen_meta_set(self,meta);
        // This backend reserves no Canvas chrome in either mode. A mode toggle
        // alone changes neither drawable size nor paint/sizeChanged counts.
        return;
    }
    if (native_match(NativeClassId::C16,"javax/microedition/lcdui/Canvas") && name == "serviceRepaints") {
        // Complete the pending frame before the game can update/free its resources.
        // Calling from paint must not recursively paint the same pending request.
        S0_HOOK(s0_service_enter(arg(0).ref == VMS_RS(g_current) && VMS_RS(g_repaint) && VMS_RS(g_paint_depth) == 0));
        if (arg(0).ref == VMS_RS(g_current) && VMS_RS(g_repaint) && VMS_RS(g_paint_depth) == 0) {
            S0_ENTRY(S0Trace::E_SERVICE);
            VMS_RS(g_repaint) = false;
            repaint_current();
        }
        S0_HOOK(s0_service_exit());
        return;
    }
    if (native_match(NativeClassId::C16,"javax/microedition/lcdui/Canvas") && name == "repaint") {
        // MIDP repaint schedules a paint; it must not suspend the game before sleep().
        speed_frame_wait_repaint(arg(0).ref);
        p1_request();VMS_RS(g_repaint) = true;
        VMS_EXEC(g_repaint_owner_ref) = current_context().ref;
        VMS_EXEC(g_repaint_yield_canvas) = arg(0).ref;
        VMS_EXEC(g_repaint_owner_yields) = 0;
        VMS_EXEC(g_repaint_yield_handoff) = false;
        return;
    }
    if (native_match(NativeClassId::C17,"java/lang/Thread")) {
        auto& owner=current_context();
        if(name=="yield" && speed_period_us() && VMS_RS(g_repaint) && owner.ref &&
            VMS_EXEC(g_repaint_owner_ref)==owner.ref && VMS_EXEC(g_repaint_yield_canvas)==VMS_RS(g_current) && VMS_RS(g_current)==VMS_RS(g_shown) &&
            VMS_EXEC(g_can_suspend) && VMS_EXEC(g_run_depth)==1 && VMS_EXEC(g_pump_depth)==1 && VMS_RS(g_paint_depth)==0 && !(J2ME_OPT_ZXX_HANDOFF ? owner.auto_fps.helper_drawing() : owner.auto_fps.grouped_update())) {
            // A fresh gate group still needs its first asynchronous paint.
            // Once drawn, helper repaints stay together until gate closure.
            bool locked=false;
            for(const auto& monitor:VMS_EXEC(g_monitors))
                if(monitor.second.owner==&owner && monitor.second.depth>0) {locked=true;break;}
            if(locked)VMS_EXEC(g_repaint_owner_yields)=0;
            else if(VMS_EXEC(g_active_stack) && !VMS_EXEC(g_active_stack)->empty()) {
                Method* caller=VMS_EXEC(g_active_stack)->back().method;
                if(VMS_EXEC(g_repaint_yield_method)!=caller || VMS_EXEC(g_repaint_yield_pc)!=fr.op_pc) {
                    VMS_EXEC(g_repaint_yield_method)=caller;VMS_EXEC(g_repaint_yield_pc)=fr.op_pc;VMS_EXEC(g_repaint_owner_yields)=0;
                }
                if(++VMS_EXEC(g_repaint_owner_yields)>=2)VMS_EXEC(g_repaint_yield_handoff)=true;
            }
        }
        thread_native(m,fr);return;
    }
    if (native_match(NativeClassId::C6,"java/lang/Object") && (name == "wait" || name == "notify" || name == "notifyAll")) {
        object_monitor_native(m, fr); return;
    }
    if (native_match(NativeClassId::C18,"java/lang/String")) {
        if (name == "intern" && m->desc == "()Ljava/lang/String;" && !(m->flags & 0x0008)) {
            const int self = arg(0).ref;
            if (!self) { throw_new("java/lang/NullPointerException", "String.intern"); return; }
            // Native invoke arguments can live only in this temporary frame.
            RootScope roots(ReentrySite::RunArgs, {&self});
            ret_ref(literal_string(read_string(self), self));
            return;
        }
        if (name == "<init>" || name == "initBytes" || name == "initChars") {
            int self = arg(0).ref, arr = arg(1).ref, off = 0;
            int len = arr > 0 ? (int)VMS_HEAP(g_heap)[arr].elems.size() : 0;
            bool chars = name == "initChars" || m->desc.find("[C") != std::string::npos;
            if (name != "<init>" || m->desc.find("II") != std::string::npos) { off = arg(2).i; len = arg(3).i; }
            if (!arr) { throw_new("java/lang/NullPointerException", "String data"); return; }
            if (off < 0 || len < 0 || (size_t)off > VMS_HEAP(g_heap)[arr].elems.size() || (size_t)len > VMS_HEAP(g_heap)[arr].elems.size() - off) {
                throw_new("java/lang/StringIndexOutOfBoundsException", "String range"); return;
            }
            std::string built;
            for (int i = 0; i < len; ++i) utf8_append(built, VMS_HEAP(g_heap)[arr].elems.get(off + i).i & (chars ? 0xFFFF : 0xFF));
            if(VMS_HEAP(g_heap)[self].indexed_key)++VMS_NATIVE(g_index_key_epoch);
            VMS_HEAP(g_heap)[self].text = built;
            return;
        }
        if (name == "initBytesCharset") {
            int self = arg(0).ref, arr = arg(1).ref, off = arg(2).i, len = arg(3).i;
            if (!arr || !arg(4).ref) { throw_new("java/lang/NullPointerException", "String data or charset"); return; }
            if (off < 0 || len < 0 || (size_t)off > VMS_HEAP(g_heap)[arr].elems.size() || (size_t)len > VMS_HEAP(g_heap)[arr].elems.size() - off) {
                throw_new("java/lang/StringIndexOutOfBoundsException", "String range"); return;
            }
            std::string charset = read_string(arg(4).ref);
            int encoding = string_encoding(charset);
            if (!encoding) { throw_new("java/io/UnsupportedEncodingException", charset); return; }
            std::string built;
            append_encoded(built, VMS_HEAP(g_heap)[arr].elems, off, len, encoding);
            if(VMS_HEAP(g_heap)[self].indexed_key)++VMS_NATIVE(g_index_key_epoch);
            VMS_HEAP(g_heap)[self].text = built;
            return;
        }
        if (name == "getBytes") {
            int encoding = 1;
            if (m->desc != "()[B") {
                if (!arg(1).ref) { throw_new("java/lang/NullPointerException", "charset"); return; }
                std::string charset = read_string(arg(1).ref);
                encoding = string_encoding(charset);
                if (!encoding) { throw_new("java/io/UnsupportedEncodingException", charset); return; }
            }
            auto bytes = encode_string(read_string(arg(0).ref), encoding);
            int array = alloc_array("[B", bytes.size());
            if (!bytes.empty()) std::memcpy(VMS_HEAP(g_heap)[array].elems.byte_data(), bytes.data(), bytes.size());
            ret_ref(array); return;
        }
        const std::string& s = read_string(name == "valueOf" ? 0 : arg(0).ref);
        if (name == "length") ret_i(utf8_len(s));
        else if (name == "charAt") {
            int i = arg(1).i;
            int cp = i < 0 ? -1 : utf8_at(s, i);
            if (cp < 0) throw_new("java/lang/StringIndexOutOfBoundsException", "charAt");
            else ret_i(cp);
        } else if (name == "equals") {
            // Object text payload is also empty for non-String objects; payload
            // equality alone made "".equals(new Object()) incorrectly true.
            const int other=arg(1).ref;
            ret_i(other>0 && size_t(other)<VMS_HEAP(g_heap).size() && !VMS_HEAP(g_heap)[other].is_array &&
                class_assignable(VMS_HEAP(g_heap)[other].cls,m->class_id) && read_string(other)==s ? 1:0);
        }
        else if (name == "startsWith") { if(!arg(1).ref)throw_new("java/lang/NullPointerException","prefix");else ret_i(s.rfind(read_string(arg(1).ref), 0) == 0 ? 1 : 0); }
        else if (name == "endsWith") {
            if(!arg(1).ref){throw_new("java/lang/NullPointerException","suffix");return;}
            const std::string& p = read_string(arg(1).ref);
            ret_i(s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0 ? 1 : 0);
        } else if (name == "indexOf" && m->desc.find("String") != std::string::npos) {
            if(!arg(1).ref){throw_new("java/lang/NullPointerException","substring");return;}
            const std::string& part = read_string(arg(1).ref);
            int from = m->desc.find(";I)") != std::string::npos ? arg(2).i : 0;
            if (from < 0) from = 0;
            size_t b = utf8_byte(s, from);
            auto p = b <= s.size() ? s.find(part, b) : std::string::npos;
            if (p == std::string::npos) ret_i(-1);
            else {
                int n = 0; size_t i = 0;
                while (i < p && i < s.size()) { utf8_next(s, i); n++; }
                ret_i(n);
            }
        } else if (name == "indexOf") {
            int from = m->desc.find("II") != std::string::npos ? arg(2).i : 0;
            if (from < 0) from = 0;
            int n = 0, found = -1; size_t i = 0;
            while (i < s.size()) {
                int cp = utf8_next(s, i);
                if (n >= from && cp == (arg(1).i & 0xFFFF)) { found = n; break; }
                n++;
            }
            ret_i(found);
        }
        else if (name == "substring") {
            int b = arg(1).i, chars = utf8_len(s);
            int e = m->desc.find("II") != std::string::npos ? arg(2).i : chars;
            if (b < 0 || e > chars || b > e) throw_new("java/lang/StringIndexOutOfBoundsException", "substring");
            else ret_ref(make_string(s.substr(utf8_byte(s, b), utf8_byte(s, e) - utf8_byte(s, b))));
        } else if (name == "trim") {
            size_t a = 0, b = s.size();
            while (a < b && (unsigned char)s[a] <= ' ') ++a;
            while (b > a && (unsigned char)s[b - 1] <= ' ') --b;
            ret_ref(make_string(s.substr(a, b - a)));
        } else if (name == "valueOf") {
            if (m->desc.find("(F)") != std::string::npos) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.3f", bits_f(arg(0).i));
                ret_ref(make_string(buf));
            } else if (m->desc.find("(D)") != std::string::npos) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.3f", bits_d(as_long(arg(0))));
                ret_ref(make_string(buf));
            } else ret_ref(make_string(std::to_string(arg(0).i)));
        } else die("native String." + name);
        return;
    }
    if (native_match(NativeClassId::C19,"java/lang/StringBuilder")) {
        int self = arg(0).ref;
        if (name == "toString") ret_ref(make_string(VMS_HEAP(g_heap)[self].text));
        else if (name == "append") {
            std::string add;
            std::string params = m->desc.substr(0, m->desc.find(')'));
            if (params == "(C") utf8_append(add, arg(1).i);
            else if (params.find("String;") != std::string::npos) add = arg(1).ref ? read_string(arg(1).ref) : "null";
            else if (params.find("I") != std::string::npos) add = std::to_string(arg(1).i);
            else die("unexpected native append descriptor");
            VMS_HEAP(g_heap)[self].text += add;
            ret_ref(self);
        } else if (name == "length") ret_i(utf8_len(VMS_HEAP(g_heap)[self].text));
        else if (name == "charAt") {
            int cp = arg(1).i < 0 ? -1 : utf8_at(VMS_HEAP(g_heap)[self].text, arg(1).i);
            if (cp < 0) throw_new("java/lang/StringIndexOutOfBoundsException", "charAt");
            else ret_i(cp);
        } else if (name == "setCharAt") {
            auto& t = VMS_HEAP(g_heap)[self].text;
            int i = arg(1).i;
            size_t a = utf8_byte(t, i), b = utf8_byte(t, i + 1);
            if (i < 0 || a >= t.size()) { throw_new("java/lang/StringIndexOutOfBoundsException", "setCharAt"); return; }
            {
                std::string one; utf8_append(one, arg(2).i);
                t.replace(a, b - a, one);
            }
        } else if (name == "setLength") {
            auto& t = VMS_HEAP(g_heap)[self].text;
            int n = arg(1).i;
            if (n < 0) { throw_new("java/lang/StringIndexOutOfBoundsException", "setLength"); return; }
            int have = utf8_len(t);
            if (n < have) t.resize(utf8_byte(t, n));
            else while (have < n) { t.push_back('\0'); have++; }
        } else if (name == "delete") {
            int a = arg(1).i, b = arg(2).i;
            auto& t = VMS_HEAP(g_heap)[self].text;
            int n = utf8_len(t);
            if (a < 0 || a > n || a > b) { throw_new("java/lang/StringIndexOutOfBoundsException", "delete"); return; }
            if (b > n) b = n;
            if (b > a) t.erase(utf8_byte(t, a), utf8_byte(t, b) - utf8_byte(t, a));
            ret_ref(self);
        } else if (name == "insert") {
            int at = arg(1).i;
            std::string add;
            if (m->desc.rfind("(ILjava/lang/String;)", 0) == 0) add = arg(2).ref ? read_string(arg(2).ref) : "null";
            else if (m->desc.find("C)") != std::string::npos) utf8_append(add, arg(2).i);
            else add = std::to_string(arg(2).i);
            auto& t = VMS_HEAP(g_heap)[self].text;
            int n = utf8_len(t);
            if (at < 0 || at > n) { throw_new("java/lang/StringIndexOutOfBoundsException", "insert"); return; }
            t.insert(utf8_byte(t, at), add);
            ret_ref(self);
        } else if (name == "reverse") {
            auto& t = VMS_HEAP(g_heap)[self].text;
            std::string rev;
            size_t i = 0;
            std::vector<std::string> parts;
            while (i < t.size()) {
                size_t a = i;
                utf8_next(t, i);
                parts.push_back(t.substr(a, i - a));
            }
            for (int k = (int)parts.size() - 1; k >= 0; --k) rev += parts[k];
            t.swap(rev);
            ret_ref(self);
        } else die("native StringBuilder." + name);
        if (name == "append" || name == "insert" || name == "setLength") {
            if (Field* f = find_field(m->class_id, "capacity", "I")) {
                int old = VMS_HEAP(g_heap)[self].fields[f->slot].i, size = utf8_len(VMS_HEAP(g_heap)[self].text);
                if (size > old) VMS_HEAP(g_heap)[self].fields[f->slot] = Value::of_i((int)std::max<int64_t>(size, std::min<int64_t>(INT32_MAX, (int64_t)old * 2 + 2)));
            }
        }
        return;
    }
    if (native_match(NativeClassId::C20,"java/lang/Math")) {
        double a = bits_d(as_long(arg(0))), result = 0;
        if (name == "sqrt") result = std::sqrt(a);
        else if (name == "sin") result = std::sin(a);
        else if (name == "cos") result = std::cos(a);
        else if (name == "tan") result = std::tan(a);
        else if (name == "ceil") result = std::ceil(a);
        else if (name == "floor") result = std::floor(a);
        else if (name == "pow") result = std::pow(a, bits_d(as_long(arg(1))));
        else { die("native Math." + name); return; }
        ret_l(d_bits(result)); return;
    }
    if (native_match(NativeClassId::C21,"java/lang/Float")) {
        if (name == "_set") {
            int32_t bits=arg(1).i;VMS_HEAP(g_heap)[arg(0).ref].text=numeric236::store_bits(uint32_t(bits),4);return;
        }
        if (name == "floatValue") {
            int32_t bits=0;const auto& text=VMS_HEAP(g_heap)[arg(0).ref].text;
            bits=int32_t(numeric236::load_bits(text,4));
            ret_i(bits);return;
        }
        if (name == "parseFloat") {
            if(!arg(0).ref){throw_new("java/lang/NullPointerException","number");return;}
            std::string text=read_string(arg(0).ref);
            if(!numeric236::literal(text)){throw_new("java/lang/NumberFormatException",text);return;}
            char* end=nullptr; const float n=std::strtof(text.c_str(),&end);
            if(text.empty()||end!=text.c_str()+text.size()){throw_new("java/lang/NumberFormatException",text);return;}
            ret_i(f_bits(n));return;
        }
        if (name == "floatToIntBits") ret_i(std::isnan(bits_f(arg(0).i)) ? 0x7fc00000 : arg(0).i);
        else if (name == "floatToRawIntBits" || name == "intBitsToFloat") ret_i(arg(0).i);
        else die("native Float." + name);
        return;
    }
    if (native_match(NativeClassId::C22,"java/lang/Double")) {
        if (name == "_set") {
            int64_t bits=as_long(arg(1));VMS_HEAP(g_heap)[arg(0).ref].text=numeric236::store_bits(uint64_t(bits),8);return;
        }
        if (name == "doubleValue") {
            int64_t bits=0;const auto& text=VMS_HEAP(g_heap)[arg(0).ref].text;
            bits=int64_t(numeric236::load_bits(text,8));
            ret_l(bits);return;
        }
        if (name == "parseDouble") {
            if(!arg(0).ref){throw_new("java/lang/NullPointerException","number");return;}
            std::string text=read_string(arg(0).ref);
            if(!numeric236::literal(text)){throw_new("java/lang/NumberFormatException",text);return;}
            char* end=nullptr; const double n=std::strtod(text.c_str(),&end);
            if(text.empty()||end!=text.c_str()+text.size()){throw_new("java/lang/NumberFormatException",text);return;}
            ret_l(d_bits(n));return;
        }
        if (name == "doubleToLongBits") ret_l(std::isnan(bits_d(as_long(arg(0)))) ? 0x7ff8000000000000LL : as_long(arg(0)));
        else if (name == "doubleToRawLongBits" || name == "longBitsToDouble") ret_l(as_long(arg(0)));
        else die("native Double." + name);
        return;
    }
    if (native_match(NativeClassId::C23,"java/lang/System")) {
        if (name == "exit") { VMS_EXEC(g_notify_destroyed)=true; log_line("LIFECYCLE System.exit status="+std::to_string(arg(0).i)); }
        else if (name == "currentTimeMillis") ret_l(game_current_ms());
        else if (name == "identityHashCode") ret_i(arg(0).ref);
        else if (name == "gc") { /* collected by the host */ }
        else if (name == "arraycopy") {
            int src = arg(0).ref, dest = arg(2).ref, sp = arg(1).i, dp = arg(3).i, n = arg(4).i;
            if (!src || !dest) { throw_new("java/lang/NullPointerException", "arraycopy"); return; }
            const auto& sd = VMS_HEAP(g_heap)[src].array_desc; const auto& dd = VMS_HEAP(g_heap)[dest].array_desc;
            if (!VMS_HEAP(g_heap)[src].is_array || !VMS_HEAP(g_heap)[dest].is_array || ((sd.size() == 2 || dd.size() == 2) && sd != dd)) {
                throw_new("java/lang/ArrayStoreException", "arraycopy types"); return;
            }
            auto& a = VMS_HEAP(g_heap)[src].elems; auto& b = VMS_HEAP(g_heap)[dest].elems;
            if (sp < 0 || dp < 0 || n < 0 || (size_t)sp > a.size() || (size_t)dp > b.size() || (size_t)n > a.size() - sp || (size_t)n > b.size() - dp) {
                throw_new("java/lang/ArrayIndexOutOfBoundsException", "arraycopy bounds"); return;
            }
            // Prove bulk reference compatibility without loading component classes:
            // merely copying an empty or all-null array must not run <clinit>.
            if (n == 0) return;
            if (!a.references() || sd == dd || dd == "[Ljava/lang/Object;") {
                if (!b.copy_from(a, sp, dp, n)) die("arraycopy storage mismatch");
            } else for (int i = 0; i < n; ++i) {
                Value value = a.get(sp + i);
                if (!type_compatible(value.ref, component_type(dd))) { throw_new("java/lang/ArrayStoreException", "arraycopy element"); return; }
                b.set(dp + i, value); // Java requires the valid prefix to remain copied.
            }
        } else die("native System." + name);
        return;
    }
    if (native_match(NativeClassId::C6,"java/lang/Object") && name == "getClass") {
        int self = arg(0).ref;
        std::string nm = VMS_HEAP(g_heap)[self].is_array ? VMS_HEAP(g_heap)[self].array_desc : VMS_CL(g_classes)[VMS_HEAP(g_heap)[self].cls].name;
        ret_ref(class_object(nm)); return;
    }
    if (native_match(NativeClassId::C24,"java/lang/Class")) {
        if (name == "getName") {
            std::string nm = read_string(arg(0).ref);
            for (char& c : nm) if (c == '/') c = '.';
            ret_ref(make_string(nm));
        } else if (name == "forName") {
            std::string nm = read_string(arg(0).ref);
            for (char& c : nm) if (c == '.') c = '/';
            int id = load_class_file(nm, false);
            if (id < 0) { throw_new("java/lang/ClassNotFoundException", nm); return; }
            ret_ref(class_object(VMS_CL(g_classes)[id].name));
        } else if (name == "newInstance") {
            std::string nm = VMS_HEAP(g_heap)[arg(0).ref].text;
            int id = ensure_class(nm);
            int obj = alloc_obj(id);
            RootScope roots(ReentrySite::NewInstance, {&obj}); // S5: obj is held in C++ until ret_ref
            if (Method* init = find_method(id, "<init>", "()V", false)) run(init, {Value::of_ref(obj)});
            ret_ref(obj);
        } else if (name == "getResourceAsStream") {
            std::string path = read_string(arg(1).ref);
            const std::vector<uint8_t>* data = nullptr;
            if (!path.empty() && path.front() != '/') {
                // CLDC 1.1: relative names use the receiver's package, including array components.
                std::string owner = VMS_HEAP(g_heap)[arg(0).ref].text;
                if (!owner.empty() && owner.front() == '[') {
                    const size_t component = owner.find_first_not_of('[');
                    owner = component != std::string::npos && owner[component] == 'L'
                        ? owner.substr(component + 1, owner.size() - component - 2) : std::string();
                }
                const size_t slash = owner.rfind('/');
                if (slash != std::string::npos) path = owner.substr(0, slash + 1) + path;
                const auto entry = VMS_CL(g_jar_files).find(path);
                if (entry != VMS_CL(g_jar_files).end()) data = &entry->second;
            } else {
                data = jar_lookup(path);
            }
            if (!data) {
                // P8.5: log-only. Record resource misses (null is the MIDP-specified result; behaviour unchanged).
                static int s_res_miss = 0;
                if (++s_res_miss <= 32) log_line("P8RESMISS n=" + std::to_string(s_res_miss) + " name=" + path);
                ret_ref(0); return;
            }
            ret_ref(stream_from(*data));
        } else if (name == "isInstance") {
            // 0.2.112 F1 (CLDC 1.1): null is never an instance; same assignability rules as checkcast/instanceof.
            const int obj = arg(1).ref;
            if (!obj) { ret_i(0); return; }
            const auto& o = VMS_HEAP(g_heap)[obj];
            const std::string actual = o.is_array ? o.array_desc : VMS_CL(g_classes)[o.cls].name;
            ret_i(descriptor_assignable(actual, VMS_HEAP(g_heap)[arg(0).ref].text) ? 1 : 0);
        } else if (name == "isAssignableFrom") {
            const int other = arg(1).ref;
            if (!other) { throw_new("java/lang/NullPointerException", "isAssignableFrom"); return; }
            const std::string from = VMS_HEAP(g_heap)[other].text, to = VMS_HEAP(g_heap)[arg(0).ref].text;
            ret_i(descriptor_assignable(from, to) ? 1 : 0);
        } else if (name == "isArray") {
            const std::string& nm = VMS_HEAP(g_heap)[arg(0).ref].text;
            ret_i(!nm.empty() && nm[0] == '[' ? 1 : 0);
        } else if (name == "isInterface") {
            const std::string nm = VMS_HEAP(g_heap)[arg(0).ref].text;
            if (nm.empty() || nm[0] == '[') { ret_i(0); return; }
            const int id = ensure_class(nm);
            ret_i((VMS_CL(g_classes)[id].flags & 0x0200) ? 1 : 0);
        } else die("native Class." + name);
        return;
    }
    if (native_match(NativeClassId::C25,"javax/microedition/io/Connector") && name == "__vmSmsSkip") {
        // 0.2.107: SMS / SMS payment always reported sent. One line per event; nothing is transmitted.
        const int addr=arg(1).ref;
        std::string to=addr?read_string(addr):std::string("-");
        if(to.empty())to="-";
        for(char& c:to)if(c==' '||c=='\n'||c=='\r'||c=='\t')c='_';
        if(to.size()>96)to=to.substr(0,96)+"...";
        log_line("SMSSKIP kind="+read_string(arg(0).ref)+" to="+to+" len="+std::to_string(arg(2).i)+(arg(3).i?" closed_conn=1":"")+" result=sent");
        return;
    }
    if (native_match(NativeClassId::C25,"javax/microedition/io/Connector") && name == "openStream") {
        auto* data = jar_lookup(read_string(arg(0).ref));
        if (!data) { ret_ref(0); return; }
        log_line("RES " + read_string(arg(0).ref) + " bytes=" + std::to_string(data->size()));
        ret_ref(stream_from(*data));
        return;
    }
    if (native_match(NativeClassId::C26,"javax/microedition/midlet/MIDlet") && name != "getAppProperty") {
        if (arg(0).ref != VMS_EXEC(g_midlet)) return;
        if (name == "notifyDestroyed") VMS_EXEC(g_notify_destroyed)=true;
        else if (name == "notifyPaused") VMS_EXEC(g_notify_paused)=true;
        else if (name == "resumeRequest") VMS_EXEC(g_midlet_resume)=true;
        else if (name == "__vmStartCompleted") {
            const int outcome=arg(1).i;
            VMS_DIAG(g_p1).start_outcome=outcome;p1_event("startApp complete async=1 outcome=%d",outcome);
            if(outcome==1)VMS_EXEC(g_notify_paused)=true;
            else if(outcome!=0)VMS_EXEC(g_notify_destroyed)=true;
            log_line("LIFECYCLE async_start_completed outcome="+std::to_string(outcome));
        }
        else die("native MIDlet."+name);
        return;
    }
    if (native_match(NativeClassId::C26,"javax/microedition/midlet/MIDlet") && name == "getAppProperty") {
        std::string k = read_string(arg(1).ref);
        auto it = VMS_CL(g_props).find(k);
        ret_ref(it == VMS_CL(g_props).end() ? 0 : make_string(it->second));
        return;
    }
    if (native_match(NativeClassId::C27,"javax/microedition/rms/RecordStore")) {
        
        CityRmsSpan rms_trace(name,name=="listRecordStores"?0:arg(0).ref);
        auto traced_stat=[&](const char* path,struct stat* st){
            CityRmsPhase phase(rms_trace,"stat");const int rc=stat(path,st),e=errno;
            phase.end(rc,rc?e:0,rc?-1:int64_t(st->st_size));errno=e;return rc;};
        auto traced_mkdir=[&](const char* path,int mode){
            CityRmsPhase phase(rms_trace,"mkdir");const int rc=mkdir(path,mode),e=errno;
            phase.end(rc,rc?e:0);errno=e;return rc;};
        auto traced_open=[&](const char* path,const char* mode){
            CityRmsPhase phase(rms_trace,"open_file");FILE* f=std::fopen(path,mode);const int e=errno;
            phase.end(f?0:-1,f?0:e);errno=e;return f;};
        auto traced_write=[&](const void* ptr,size_t unit,size_t count,FILE* out){
            CityRmsPhase phase(rms_trace,"write",int64_t(unit*count));const size_t got=std::fwrite(ptr,unit,count,out);const int e=errno;
            phase.end(got==count?0:-1,got==count?0:e,int64_t(got*unit));errno=e;return got;};
        auto traced_flush=[&](FILE* out){
            CityRmsPhase phase(rms_trace,"flush");const int rc=std::fflush(out),e=errno;
            phase.end(rc,rc?e:0);errno=e;return rc;};
        auto traced_close=[&](FILE* out){
            CityRmsPhase phase(rms_trace,"close_file");const int rc=std::fclose(out),e=errno;
            phase.end(rc,rc?e:0);errno=e;return rc;};
        auto traced_rename=[&](const char* from,const char* to){
            CityRmsPhase phase(rms_trace,"rename");const int rc=std::rename(from,to),e=errno;
            phase.end(rc,rc?e:0);errno=e;return rc;};
        auto traced_remove=[&](const char* path){
            CityRmsPhase phase(rms_trace,"remove");const int rc=std::remove(path),e=errno;
            phase.end(rc,rc?e:0);errno=e;return rc;};
        auto traced_peek=[&](std::ifstream& in){
            CityRmsPhase phase(rms_trace,"peek_validate");const int rc=in.peek(),e=errno;
            phase.end(rc,in.bad()?e:0);errno=e;return rc;};

        // These namespaces organize storage by JAR; they are not a security
        // boundary against an untrusted JAR or other filesystem users.
        auto rms_error = [&](const std::string& message) {
            // MIDP listing and free-space queries do not declare I/O failures.
            // Fail conservatively without inventing stores or available space.
            if (name == "listRecordStores") { log_line("RMS list: " + message); ret_ref(0); }
            else if (name == "availableStoreSize") { log_line("RMS space: " + message); ret_i(0); }
            else throw_new("javax/microedition/rms/RecordStoreException", message);
        };
        if (VMS_PLAT(g_rms_suite).size() != 64 || VMS_PLAT(g_rms_suite).find_first_not_of("0123456789abcdef") != std::string::npos) {
            rms_error("invalid RMS suite identity"); return;
        }
        auto ensure_directory = [&](const std::string& directory) {
            if (traced_mkdir(directory.c_str(), 0777) != 0 && errno != EEXIST) {
                rms_error("cannot create RMS directory"); return false;
            }
            struct stat st;
            if (traced_stat(directory.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
                rms_error("RMS directory is unavailable or not a directory"); return false;
            }
            return true;
        };
        // Return absent separately from every inspection error. A failed stat
        // must never authorize migration, replacement, or resurrection.
        auto inspect_file = [&](const std::string& filename) {
            struct stat st;
            if (traced_stat(filename.c_str(), &st) == 0) {
                if (!S_ISREG(st.st_mode)) { rms_error("RMS path is not a regular file"); return -1; }
                return 1;
            }
            if (errno == ENOENT) return 0;
            rms_error("cannot inspect RMS file"); return -1;
        };
        std::string directory;
#ifdef __SWITCH__
        directory = "sdmc:/switch/j2me/rms";
#else
        const char* override_directory = std::getenv("J2ME_RMS_DIR");
        directory = override_directory && *override_directory ? override_directory : "/tmp/j2me-rms";
#endif
        if(r4::state.active)directory=r4::state.dir+"/rms";
        std::string suite_directory = directory + "/suites/" + VMS_PLAT(g_rms_suite);
        // The full existing directory proves that its ancestors are traversable.
        // Recheck it on every operation, without a cache, instead of repeating
        // mkdir + stat for each ancestor during every small synchronous commit.
        struct stat suite_stat;
        if (traced_stat(suite_directory.c_str(), &suite_stat) == 0) {
            if (!S_ISDIR(suite_stat.st_mode)) { rms_error("RMS directory is unavailable or not a directory"); return; }
        } else {
            if (errno != ENOENT) { rms_error("cannot inspect RMS directory"); return; }
#ifdef __SWITCH__
            if (!ensure_directory("sdmc:/switch/j2me")) return;
#endif
            if (!ensure_directory(directory) || !ensure_directory(directory + "/suites") || !ensure_directory(suite_directory)) return;
        }
        if (name == "listRecordStores") {
            std::vector<std::string> names;
            if (!j2me_rms::list_stores(suite_directory, names) || names.empty()) { ret_ref(0); return; }
            int array = alloc_array("[Ljava/lang/String;", names.size());
            ret_ref(array);
            for (size_t i = 0; i < names.size(); ++i) {
                int text = make_string(names[i]); // allocation may grow VMS_HEAP(g_heap)
                VMS_HEAP(g_heap)[array].elems.set(i, Value::of_ref(text));
            }
            return;
        }
        std::string store_name = read_string(arg(0).ref);
        std::string encoded;
        const char* hex = "0123456789abcdef";
        for (unsigned char c : store_name) { encoded.push_back(hex[c >> 4]); encoded.push_back(hex[c & 15]); }
        // Hex every byte, including ASCII, so exact UTF-16/CESU-8 names stay
        // distinct even on FAT's case-insensitive filesystem.
        std::string path = suite_directory + "/r" + encoded + ".bin";
        // fsdev rename need not replace a destination. Recover the old snapshot
        // if a two-rename replacement was interrupted between those renames.
        std::string previous = path + ".previous";
        std::string tombstone = path + ".deleted";
        if (name == "availableStoreSize") {
            uint64_t bytes = 0, block = 1, legacy_backup = 0;
            if (!j2me_rms::free_bytes(suite_directory, bytes, block)) { ret_i(0); return; }
            struct stat target, backup;
            if (traced_stat(path.c_str(), &target) != 0 || !S_ISREG(target.st_mode) || target.st_size < 0) { ret_i(0); return; }
            std::ifstream in;
            {CityRmsPhase phase(rms_trace,"open_stream");in.open(path, std::ios::binary);const int e=errno;phase.end(in?0:-1,in?0:e);errno=e;}
            CityRmsCloseProbe close_in(rms_trace,in);
            unsigned char header[4] = {};
            if (!in) { ret_i(0); return; }
            {CityRmsPhase phase(rms_trace,"read",4);in.read(reinterpret_cast<char*>(header), 4);const int e=errno;phase.end(in.bad()?-1:0,in.bad()?e:0,in.gcount());errno=e;}
            if (in.bad()) { ret_i(0); return; }
            bool modern = in.gcount() == 4 && header[0] == 0xd2 && header[1] == 0x4d && header[2] == 0x53 && header[3] == 0x32;
            if (!modern) {
                int status = traced_stat((path + ".v72.bak").c_str(), &backup);
                if (status == 0 && !S_ISREG(backup.st_mode)) { ret_i(0); return; }
                if (status != 0) {
                    if (errno != ENOENT) { ret_i(0); return; }
                    legacy_backup = uint64_t(target.st_size);
                }
            }
            // An estimate of growth at this instant, capped by the existing
            // 32-bit snapshot format; another writer may consume free space.
            ret_i(j2me_rms::growth_available(bytes, block, arg(1).i, legacy_backup));
            return;
        }
        struct stat recovery_stat;
        int target_status = inspect_file(path);
        if (target_status < 0) return;
        if (target_status == 0) {
            int previous_status = inspect_file(previous);
            if (previous_status < 0) return;
            if (previous_status == 1) {
                if (traced_rename(previous.c_str(), path.c_str()) != 0) {
                    rms_error("cannot recover previous RMS snapshot"); return;
                }
                target_status = 1;
                log_line("RMS recover " + path);
            }
        }
        if (name == "loadStore") {
            if (target_status == 0 && VMS_PLAT(g_rms_import_legacy)) {
                int deleted_status = inspect_file(tombstone);
                if (deleted_status < 0) return;
                if (deleted_status == 0) {
                    std::string v73, v72;
                    bool plain_name = true;
                    for (unsigned char c : store_name) {
                        bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
                        if (!plain) plain_name = false;
                        v72.push_back(std::isalnum(c) ? (char)c : '_');
                    }
                    v73 = plain_name ? store_name : "%" + encoded;
                    // A legacy .previous is a read-only source when its main
                    // snapshot is absent. Never recover by renaming old files.
                    auto legacy_source = [&](const std::string& old_path) {
                        int main_status = inspect_file(old_path);
                        if (main_status < 0) return std::string();
                        int old_previous_status = inspect_file(old_path + ".previous");
                        if (old_previous_status < 0) return std::string();
                        if (main_status == 1) return old_path;
                        return old_previous_status == 1 ? old_path + ".previous" : std::string();
                    };
                    std::string source = legacy_source(directory + "/" + v73 + ".bin");
                    if (VMS_EXEC(g_thrown)) return;
                    if (v72 != v73) {
                        std::string other = legacy_source(directory + "/" + v72 + ".bin");
                        if (VMS_EXEC(g_thrown)) return;
                        if (!source.empty() && !other.empty()) { rms_error("ambiguous legacy RMS filenames"); return; }
                        if (source.empty()) source = other;
                    }
                    if (!source.empty()) {
                        struct stat source_stat;
                        if (traced_stat(source.c_str(), &source_stat) != 0 || !S_ISREG(source_stat.st_mode) || source_stat.st_size < 0 || (uint64_t)source_stat.st_size > INT32_MAX) {
                            rms_error("invalid legacy RMS file size or type"); return;
                        }
                        std::ifstream input;
            {CityRmsPhase phase(rms_trace,"open_stream");input.open(source, std::ios::binary);const int e=errno;phase.end(input?0:-1,input?0:e);errno=e;}
            CityRmsCloseProbe close_input(rms_trace,input);
                        if (!input) { rms_error("cannot open legacy RMS file"); return; }
                        std::string temporary = path + ".tmp";
                        int temporary_status = inspect_file(temporary);
                        if (temporary_status < 0) return;
                        FILE* output = traced_open(temporary.c_str(), "wb");
                        if (!output) { rms_error("cannot create RMS import temporary file"); return; }
                        char block[4096];
                        uint64_t copied = 0;
                        bool failed = false;
                        while (!failed && input) {
                            {CityRmsPhase phase(rms_trace,"read",sizeof(block));input.read(block, sizeof(block));const int e=errno;phase.end(input.bad()?-1:0,input.bad()?e:0,input.gcount());errno=e;}
                            std::streamsize got = input.gcount();
                            if (got > 0) {
                                copied += (uint64_t)got;
                                if (traced_write(block, 1, (size_t)got, output) != (size_t)got) failed = true;
                            }
                        }
                        if (!input.eof() || input.bad() || copied != (uint64_t)source_stat.st_size) failed = true;
                        if (traced_flush(output) != 0) failed = true;
                        if (traced_close(output) != 0) failed = true;
                        // A recovery snapshot or store appearing during import
                        // must be left intact, including on overwriting rename.
                        if (!failed) {
                            int current = inspect_file(path);
                            int prior = inspect_file(previous);
                            int deleted = inspect_file(tombstone);
                            if (current != 0 || prior != 0 || deleted != 0) failed = true;
                        }
                        if (!failed && traced_rename(temporary.c_str(), path.c_str()) != 0) failed = true;
                        if (failed) {
                            traced_remove(temporary.c_str());
                            if (!VMS_EXEC(g_thrown)) rms_error("cannot commit legacy RMS import");
                            return;
                        }
                        log_line("RMS import " + source + " -> " + path);
                    }
                }
            }
            struct stat st;
            if (traced_stat(path.c_str(), &st) != 0) {
                if (errno == ENOENT) ret_ref(0);
                else throw_new("javax/microedition/rms/RecordStoreException", "cannot inspect RMS file");
                return;
            }
            if (!S_ISREG(st.st_mode) || st.st_size < 0 || (uint64_t)st.st_size > INT32_MAX) {
                throw_new("javax/microedition/rms/RecordStoreException", "invalid RMS file size or type"); return;
            }
            std::ifstream in;
            {CityRmsPhase phase(rms_trace,"open_stream");in.open(path, std::ios::binary);const int e=errno;phase.end(in?0:-1,in?0:e);errno=e;}
            CityRmsCloseProbe close_in(rms_trace,in);
            if (!in) { throw_new("javax/microedition/rms/RecordStoreException", "cannot open RMS file"); return; }
            CityRmsPhase allocate_native(rms_trace,"allocate_native",st.st_size);
            std::vector<uint8_t> buf((size_t)st.st_size);allocate_native.end(0,0,buf.size());
            if (!buf.empty()) {CityRmsPhase phase(rms_trace,"read",(std::streamsize)buf.size());in.read(reinterpret_cast<char*>(buf.data()), (std::streamsize)buf.size());const int e=errno;phase.end(in.bad()?-1:0,in.bad()?e:0,in.gcount());errno=e;}
            if ((!buf.empty() && in.gcount() != (std::streamsize)buf.size()) || in.bad() || traced_peek(in) != std::char_traits<char>::eof()) {
                throw_new("javax/microedition/rms/RecordStoreException", "cannot read complete RMS file"); return;
            }
            CityRmsPhase allocate_java(rms_trace,"allocate_java",buf.size());
            int arr = alloc_array("[B", buf.size());allocate_java.end(0,0,buf.size());
            if (!buf.empty()) {CityRmsPhase copy(rms_trace,"copy",buf.size());std::memcpy(VMS_HEAP(g_heap)[arr].elems.byte_data(), buf.data(), buf.size());copy.end(0,0,buf.size());}
            log_line("RMS load " + std::to_string(buf.size()));
            ret_ref(arr);
        } else if (name == "saveStore") {
            int arr = arg(1).ref;
            if (arr <= 0 || VMS_HEAP(g_heap)[arr].array_desc != "[B") {
                throw_new("java/lang/IllegalArgumentException", "RMS requires byte[]"); return;
            }
            // 0.1.72 cannot read RMS2 snapshots. Preserve the exact first legacy
            // snapshot once, before replacing it, so a device can be rolled back.
            struct stat existing;
            if (traced_stat(path.c_str(), &existing) == 0) {
                if (!S_ISREG(existing.st_mode)) { throw_new("javax/microedition/rms/RecordStoreException", "RMS target is not a file"); return; }
                std::ifstream original;
            {CityRmsPhase phase(rms_trace,"open_stream");original.open(path, std::ios::binary);const int e=errno;phase.end(original?0:-1,original?0:e);errno=e;}
            CityRmsCloseProbe close_original(rms_trace,original);
                if (!original) { throw_new("javax/microedition/rms/RecordStoreException", "cannot inspect RMS format"); return; }
                unsigned char header[4] = {};
                {CityRmsPhase phase(rms_trace,"read",4);original.read(reinterpret_cast<char*>(header), 4);const int e=errno;phase.end(original.bad()?-1:0,original.bad()?e:0,original.gcount());errno=e;}
                bool modern = original.gcount() == 4 && header[0] == 0xd2 && header[1] == 0x4d && header[2] == 0x53 && header[3] == 0x32;
                if (original.bad()) { throw_new("javax/microedition/rms/RecordStoreException", "cannot read RMS format"); return; }
                if (!modern) {
                    std::string backup = path + ".v72.bak";
                    struct stat saved;
                    if (traced_stat(backup.c_str(), &saved) == 0) {
                        if (!S_ISREG(saved.st_mode)) { throw_new("javax/microedition/rms/RecordStoreException", "legacy RMS backup is not a file"); return; }
                    } else if (errno != ENOENT) {
                        throw_new("javax/microedition/rms/RecordStoreException", "cannot inspect legacy RMS backup"); return;
                    } else {
                        std::string backup_tmp = backup + ".tmp";
                        FILE* backup_out = traced_open(backup_tmp.c_str(), "wb");
                        if (!backup_out) { throw_new("javax/microedition/rms/RecordStoreException", "cannot create legacy RMS backup"); return; }
                        original.clear(); original.seekg(0);
                        char block[4096];
                        bool backup_failed = !original;
                        while (!backup_failed && original) {
                            {CityRmsPhase phase(rms_trace,"read",sizeof(block));original.read(block, sizeof(block));const int e=errno;phase.end(original.bad()?-1:0,original.bad()?e:0,original.gcount());errno=e;}
                            std::streamsize got = original.gcount();
                            if (got > 0 && traced_write(block, 1, (size_t)got, backup_out) != (size_t)got) backup_failed = true;
                        }
                        if (!original.eof() || original.bad()) backup_failed = true;
                        if (traced_flush(backup_out) != 0) backup_failed = true;
                        if (traced_close(backup_out) != 0) backup_failed = true;
                        if (!backup_failed) {
                            // Never replace an existing rollback backup.
                            if (traced_stat(backup.c_str(), &saved) == 0) {
                                if (!S_ISREG(saved.st_mode)) backup_failed = true;
                                traced_remove(backup_tmp.c_str());
                            }
                            else if (errno != ENOENT || traced_rename(backup_tmp.c_str(), backup.c_str()) != 0) backup_failed = true;
                        }
                        if (backup_failed) {
                            traced_remove(backup_tmp.c_str());
                            throw_new("javax/microedition/rms/RecordStoreException", "cannot preserve legacy RMS backup"); return;
                        }
                        log_line("RMS legacy backup " + backup);
                    }
                }
            } else if (errno != ENOENT) {
                throw_new("javax/microedition/rms/RecordStoreException", "cannot inspect RMS target"); return;
            }
            std::string temporary = path + ".tmp";
            FILE* out = traced_open(temporary.c_str(), "wb");
            if (!out) {
                throw_new(errno == ENOSPC ? "javax/microedition/rms/RecordStoreFullException" : "javax/microedition/rms/RecordStoreException", "cannot open RMS temporary file"); return;
            }
            size_t length = VMS_HEAP(g_heap)[arr].elems.size();
            bool failed = length && traced_write(VMS_HEAP(g_heap)[arr].elems.read_bytes(), 1, length, out) != length;
            int failure_errno = failed ? errno : 0;
            if (traced_flush(out) != 0) { failed = true; failure_errno = errno; }
            if (traced_close(out) != 0) { failed = true; failure_errno = errno; }
            if (!failed && traced_rename(temporary.c_str(), path.c_str()) != 0) {
                failure_errno = errno;
                failed = true;
                if (failure_errno == EEXIST && traced_stat(path.c_str(), &existing) == 0 && S_ISREG(existing.st_mode)) {
                    // Keep the only old copy until the new complete file is in place.
                    struct stat prior = {};
                    int prior_status = traced_stat(previous.c_str(), &prior);
                    bool ready = prior_status != 0 && errno == ENOENT;
                    if (prior_status == 0 && S_ISREG(prior.st_mode)) ready = traced_remove(previous.c_str()) == 0;
                    if (ready && traced_rename(path.c_str(), previous.c_str()) == 0) {
                        if (traced_rename(temporary.c_str(), path.c_str()) == 0) {
                            failed = false;
                            if (traced_remove(previous.c_str()) != 0) log_line("RMS previous snapshot retained " + previous);
                        } else {
                            failure_errno = errno;
                            if (traced_rename(previous.c_str(), path.c_str()) != 0) log_line("RMS recovery required " + previous);
                        }
                    }
                }
            }
            if (failed) {
                traced_remove(temporary.c_str());
                throw_new(failure_errno == ENOSPC ? "javax/microedition/rms/RecordStoreFullException" : "javax/microedition/rms/RecordStoreException", "cannot commit RMS snapshot"); return;
            }
            log_line("RMS save " + path + " bytes=" + std::to_string(length));
        } else if (name == "deleteStore") {
            // Even a reset of a not-yet-imported store must block a subsequent
            // legacy import in this launch. Preserve the public NotFound result.
            const bool missing = target_status == 0;
            // Commit a permanent import barrier before removing either copy.
            // Keep it after a later create/save: it blocks only old global
            // migration, not new suite snapshots or .previous recovery.
            int deleted_status = inspect_file(tombstone);
            if (deleted_status < 0) return;
            if (deleted_status == 0) {
                std::string deleted_temporary = tombstone + ".tmp";
                if (inspect_file(deleted_temporary) < 0) return;
                FILE* deleted_out = traced_open(deleted_temporary.c_str(), "wb");
                if (!deleted_out) { rms_error("cannot create RMS deletion marker"); return; }
                const char marker[] = "deleted\n";
                bool failed = traced_write(marker, 1, sizeof(marker) - 1, deleted_out) != sizeof(marker) - 1;
                if (traced_flush(deleted_out) != 0) failed = true;
                if (traced_close(deleted_out) != 0) failed = true;
                if (!failed) {
                    int current = inspect_file(tombstone);
                    if (current < 0) failed = true;
                    else if (current == 1) traced_remove(deleted_temporary.c_str());
                    else if (traced_rename(deleted_temporary.c_str(), tombstone.c_str()) != 0) failed = true;
                }
                if (failed) {
                    traced_remove(deleted_temporary.c_str());
                    if (!VMS_EXEC(g_thrown)) rms_error("cannot commit RMS deletion marker");
                    return;
                }
            }
            if (missing) {
                throw_new("javax/microedition/rms/RecordStoreNotFoundException", "RMS store does not exist"); return;
            }
            // A leftover replacement journal must not resurrect a deleted store.
            if (traced_stat(previous.c_str(), &recovery_stat) == 0) {
                if (!S_ISREG(recovery_stat.st_mode) || traced_remove(previous.c_str()) != 0) {
                    throw_new("javax/microedition/rms/RecordStoreException", "cannot delete previous RMS snapshot"); return;
                }
            } else if (errno != ENOENT) {
                throw_new("javax/microedition/rms/RecordStoreException", "cannot inspect previous RMS snapshot"); return;
            }
            if (traced_remove(path.c_str()) != 0) {
                throw_new(errno == ENOENT ? "javax/microedition/rms/RecordStoreNotFoundException" : "javax/microedition/rms/RecordStoreException", "cannot delete RMS file"); return;
            }
            log_line("RMS delete " + path);
        } else die("native RecordStore." + name);
        return;
    }
    die("unimplemented native " + cn + "." + name + m->desc);
}

