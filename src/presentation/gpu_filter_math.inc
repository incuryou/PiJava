// Shared C++ / GLSL integer contract. Keep every divisor constant for UAM's
// integer division lowering. Filter IDs come from display_filter::Filter.
GPU_INLINE uint gpu_filter_source(uint coordinate,uint size,uint extent,uint preset){
    uint n=preset==5u?(2u*coordinate+1u)*size:2u*coordinate*size;
    return extent==480u?n/960u:n/1280u;
}
GPU_INLINE uint gpu_filter_channel(uint value,uint channel,uint x,uint y,uint preset,uint gain){
    if(preset==3u){
        if(channel!=x%3u)value=value*3u/4u;
        if((y&1u)!=0u)value=value*7u/8u;
    }else if(preset==4u){
        if((y&1u)!=0u)value=value*3u/4u;
    }else if(preset==5u){
        value=(value*gain+2048u)/4096u;
        value=value>255u?255u:value;
    }
    return value;
}

struct GpuFilterAxis {uint base;uint fraction;};
GPU_INLINE GpuFilterAxis gpu_filter_bilinear_axis(uint coordinate,uint size,uint extent){
    GpuFilterAxis axis;axis.base=0u;axis.fraction=0u;
    int n=int((2u*coordinate+1u)*size)-int(extent);
    if(n<=0)return axis;
    uint numerator=uint(n);
    axis.base=extent==480u?numerator/960u:numerator/1280u;
    axis.fraction=extent==480u?numerator%960u:numerator%1280u;
    if(axis.base>=size-1u){axis.base=size-1u;axis.fraction=0u;}
    return axis;
}
GPU_INLINE uint gpu_filter_bilinear_color(uint a,uint b,uint c,uint d,uint fx,uint fy){
    uint wa=(960u-fx)*(1280u-fy),wb=fx*(1280u-fy);
    uint wc=(960u-fx)*fy,wd=fx*fy,color=0xff000000u;
    // Weights sum to 1228800: even full-white sum=313344000 fits uint32.
    for(uint shift=0u;shift<24u;shift+=8u){
        uint sum=((a>>shift)&255u)*wa+((b>>shift)&255u)*wb+
                 ((c>>shift)&255u)*wc+((d>>shift)&255u)*wd;
        color|=((sum+614400u)/1228800u)<<shift;
    }
    return color;
}
GPU_INLINE uint gpu_filter_scale2x_color(uint b,uint d,uint e,uint f,uint h,uint vx,uint vy){
    if(b==h||d==f)return e;
    if((vy&1u)==0u)return (vx&1u)==0u?(d==b?d:e):(b==f?f:e);
    return (vx&1u)==0u?(d==h?d:e):(h==f?f:e);
}
// GPU_PIXEL_SOURCE supplies the C++ input pointer; GLSL reads its SSBO via
// GPU_PIXEL_READ. Both compile the same coordinates, fetches and arithmetic.
GPU_INLINE uint gpu_filter_pixel(GPU_PIXEL_SOURCE uint x,uint y,uint w,uint h,uint preset,uint gain){
    if(preset==1u){
        GpuFilterAxis ax=gpu_filter_bilinear_axis(x,w,480u),ay=gpu_filter_bilinear_axis(y,h,640u);
        uint right=ax.base+1u<w?ax.base+1u:w-1u,bottom=ay.base+1u<h?ay.base+1u:h-1u;
        return gpu_filter_bilinear_color(GPU_PIXEL_READ(ax.base,ay.base),GPU_PIXEL_READ(right,ay.base),
            GPU_PIXEL_READ(ax.base,bottom),GPU_PIXEL_READ(right,bottom),ax.fraction,ay.fraction);
    }
    if(preset==2u){
        uint vx=x*w*2u/480u,vy=y*h*2u/640u,sx=vx/2u,sy=vy/2u;
        uint left=sx>0u?sx-1u:0u,top=sy>0u?sy-1u:0u;
        uint right=sx+1u<w?sx+1u:w-1u,bottom=sy+1u<h?sy+1u:h-1u;
        return gpu_filter_scale2x_color(GPU_PIXEL_READ(sx,top),GPU_PIXEL_READ(left,sy),GPU_PIXEL_READ(sx,sy),
            GPU_PIXEL_READ(right,sy),GPU_PIXEL_READ(sx,bottom),vx,vy);
    }
    uint sx=gpu_filter_source(x,w,480u,preset),sy=gpu_filter_source(y,h,640u,preset);
    uint c=GPU_PIXEL_READ(sx,sy),color=0xff000000u;
    for(uint channel=0u;channel<3u;++channel)
        color|=gpu_filter_channel((c>>(channel*8u))&255u,channel,x,y,preset,gain)<<(channel*8u);
    return color;
}

// UAM lowers nonconstant integer division to uncorrected floating point.
// New adaptive calculations have numerator < 2^30, denominator < 2^24,
// and quotient <= 819200;
// estimate with float, then correct either possible one-unit rounding error
// using exact integer products. No runtime integer division reaches the shader.
GPU_INLINE uint gpu_filter_divide(uint numerator,uint denominator){
    uint quotient=uint(float(numerator)/float(denominator));
    uint product=quotient*denominator;
    if(product>numerator)return quotient-1u;
    return numerator-product>=denominator?quotient+1u:quotient;
}
struct GpuFilterViewport {uint x;uint y;uint w;uint h;};
GPU_INLINE GpuFilterViewport gpu_filter_viewport(uint w,uint h,uint adaptive){
    GpuFilterViewport rect;rect.x=400u;rect.y=40u;rect.w=480u;rect.h=640u;
    if(adaptive==0u)return rect;
    if(w*720u>=h*1280u){rect.w=1280u;rect.h=gpu_filter_divide(h*1280u,w);}
    else{rect.h=720u;rect.w=gpu_filter_divide(w*720u,h);}
    if(rect.w==0u)rect.w=1u;if(rect.h==0u)rect.h=1u;
    rect.x=(1280u-rect.w)/2u;rect.y=(720u-rect.h)/2u;return rect;
}
GPU_INLINE GpuFilterAxis gpu_filter_bilinear_axis_extent(uint coordinate,uint size,uint extent){
    GpuFilterAxis axis;axis.base=0u;axis.fraction=0u;
    int n=int((2u*coordinate+1u)*size)-int(extent);
    if(n<=0)return axis;
    uint denominator=2u*extent,numerator=uint(n);
    axis.base=gpu_filter_divide(numerator,denominator);
    axis.fraction=numerator-axis.base*denominator;
    if(axis.base>=size-1u){axis.base=size-1u;axis.fraction=0u;}
    return axis;
}
GPU_INLINE uint gpu_filter_bilinear_color_extent(uint a,uint b,uint c,uint d,uint fx,uint fy,uint output_width,uint output_height){
    uint dx=2u*output_width,dy=2u*output_height;
    uint wa=(dx-fx)*(dy-fy),wb=fx*(dy-fy),wc=(dx-fx)*fy,wd=fx*fy;
    uint denominator=dx*dy,color=0xff000000u;
    // At most 4*1280*720 total weight: full white plus rounding fits uint32.
    for(uint shift=0u;shift<24u;shift+=8u){
        uint sum=((a>>shift)&255u)*wa+((b>>shift)&255u)*wb+
                 ((c>>shift)&255u)*wc+((d>>shift)&255u)*wd;
        color|=gpu_filter_divide(sum+denominator/2u,denominator)<<shift;
    }
    return color;
}
GPU_INLINE uint gpu_filter_pixel_extent(GPU_PIXEL_SOURCE uint x,uint y,uint w,uint h,uint preset,uint gain,uint output_width,uint output_height){
    if(preset==1u){
        GpuFilterAxis ax=gpu_filter_bilinear_axis_extent(x,w,output_width),ay=gpu_filter_bilinear_axis_extent(y,h,output_height);
        uint right=ax.base+1u<w?ax.base+1u:w-1u,bottom=ay.base+1u<h?ay.base+1u:h-1u;
        return gpu_filter_bilinear_color_extent(GPU_PIXEL_READ(ax.base,ay.base),GPU_PIXEL_READ(right,ay.base),
            GPU_PIXEL_READ(ax.base,bottom),GPU_PIXEL_READ(right,bottom),ax.fraction,ay.fraction,output_width,output_height);
    }
    if(preset==2u){
        uint vx=gpu_filter_divide(x*w*2u,output_width),vy=gpu_filter_divide(y*h*2u,output_height),sx=vx/2u,sy=vy/2u;
        uint left=sx>0u?sx-1u:0u,top=sy>0u?sy-1u:0u;
        uint right=sx+1u<w?sx+1u:w-1u,bottom=sy+1u<h?sy+1u:h-1u;
        return gpu_filter_scale2x_color(GPU_PIXEL_READ(sx,top),GPU_PIXEL_READ(left,sy),GPU_PIXEL_READ(sx,sy),
            GPU_PIXEL_READ(right,sy),GPU_PIXEL_READ(sx,bottom),vx,vy);
    }
    uint nx=preset==5u?(2u*x+1u)*w:2u*x*w,ny=preset==5u?(2u*y+1u)*h:2u*y*h;
    uint sx=gpu_filter_divide(nx,2u*output_width),sy=gpu_filter_divide(ny,2u*output_height);
    uint c=GPU_PIXEL_READ(sx,sy),color=0xff000000u;
    for(uint channel=0u;channel<3u;++channel)
        color|=gpu_filter_channel((c>>(channel*8u))&255u,channel,x,y,preset,gain)<<(channel*8u);
    return color;
}
