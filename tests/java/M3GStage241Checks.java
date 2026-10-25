package javax.microedition.m3g;
import javax.microedition.lcdui.Image;
import javax.microedition.lcdui.Graphics;
/** Reference Sprite loop copied from the unmodified 0.2.40 source; full public
 * resource and scene entry points exercise allocation, upload, mip and draw. */
public final class M3GStage241Checks {
    public static Graphics3D g;
    public static Image2D spr;
    public static short[] depths;
    public static int[] ip=new int[12];
    public static float[] fp=new float[2];
    public static int pool(){
        g=Graphics3D.getInstance();if(g.getTarget()!=null)g.releaseTarget();
        spr=new Image2D(Image2D.RGBA,2048,1);
        g.bindTarget(new Image2D(Image2D.RGBA,1024,1),true,0);
        return 1;
    }
    public static int blend(int mode){
        if(mode==5)g.compBlender=Graphics3DPipelines.CompositingBlenders.PASSTHROUGH;
        else if(mode==6)g.compBlender=new Graphics3DPipelines.CompositingBlender(){public int blend(int a,int b,int alpha){return a^b;}};
        else if(mode==0)g.compBlender=Graphics3DPipelines.CompositingBlenders.ALPHA;
        else if(mode==1)g.compBlender=Graphics3DPipelines.CompositingBlenders.ALPHA_ADD;
        else if(mode==2)g.compBlender=Graphics3DPipelines.CompositingBlenders.MODULATE;
        else if(mode==3)g.compBlender=Graphics3DPipelines.CompositingBlenders.MODULATE_X2;
        else g.compBlender=null;
        return 1;
    }
    public static int rowReference(){
        int index=ip[0],count=ip[1],texY=ip[2],left=ip[3],width=ip[4],z=ip[5],flags=ip[6];
        int alphaFactor=ip[7],threshold=ip[8],fog=ip[9],fogRB=ip[10],fogG=ip[11];
        float u=fp[0],du=fp[1];
        for(int i=0;i<count;i++,index++,u+=du){
            if((flags&1)!=0 && depths[index]<z)continue;
            int tx=left+(int)u;
            if(tx<left)tx=left;else if(tx>=left+width)tx=left+width-1;
            Graphics3D.paintPixel=spr.image[texY+tx];
            int alpha=((Graphics3D.paintPixel>>>24)*alphaFactor)>>8;
            if(alpha<threshold||alpha==0)continue;
            if((flags&4)!=0){
                int rb=Graphics3D.paintPixel&0x00ff00ff,gg=(Graphics3D.paintPixel>>8)&255;
                int outRB=(fogRB+(((rb-fogRB)*fog)>>8))&0x00ff00ff;
                int outG=fogG+(((gg-fogG)*fog)>>8);
                Graphics3D.paintPixel=(Graphics3D.paintPixel&0xff000000)|outRB|(outG<<8);
            }
            g.rasterData[index]=g.compBlender==null?Graphics3D.paintPixel:g.compBlender.blend(g.rasterData[index],Graphics3D.paintPixel,alpha);
            if((flags&2)!=0)depths[index]=(short)z;
        }
        return Graphics3D.paintPixel;
    }
    static byte[] bytes(int seed,int count){byte[] b=new byte[count];int x=seed;
        for(int i=0;i<count;i++){x=x*1664525+1013904223;b[i]=(byte)(x>>>24);}return b;}
    public static int[] resources(int seed){
        int[] widths={1,1,17,7,64,127,240,1024,2048};
        int[] heights={1,17,1,5,64,65,320,1,1};
        int w=widths[seed%9],h=heights[seed%9],fmt=96+(seed/9)%5;
        int channels=fmt<98?1:fmt-96;Image2D img;
        int type=(seed/45)%4;
        if(type==0)img=new Image2D(fmt,w,h,bytes(seed,w*h*channels));
        else if(type==1){int[] p=new int[w*h];int x=seed;for(int i=0;i<p.length;i++){x=x*1664525+1013904223;p[i]=x;}
            img=new Image2D(fmt,Image.createRGBImage(p,w,h,true));}
        else if(type==2){byte[] pal=bytes(seed,256*channels);if(fmt==100&&(seed&1)!=0)for(int i=3;i<pal.length;i+=4)pal[i]=0;
            img=new Image2D(fmt,w,h,bytes(seed+7,w*h),pal);}
        else {img=new Image2D(fmt,w,h);img.set(0,0,w,h,bytes(seed,w*h*channels));
            int rw=Math.max(1,w/2),rh=Math.max(1,h/2);img.set(w-rw,h-rh,rw,rh,bytes(seed+1,rw*rh*channels));}
        Texture2D tex=new Texture2D(img);
        // Dynamic image updates must not hit a stale cache. The reference
        // explicitly regenerates mipmaps at setImage, and we keep that rule.
        if(img.isMutable()){img.set(0,0,1,1,bytes(seed+11,channels));tex.setImage(img);}
        int levels=1,total=0;for(int n=Math.max(w,h);n>1;n>>=1)levels++;
        for(int l=0;l<levels;l++){Image2D a=tex.getImageForLOD(l);total+=a.image.length+3;}
        int[] result=new int[total];int offset=0;
        for(int l=0;l<levels;l++){Image2D a=tex.getImageForLOD(l);result[offset++]=a.width;result[offset++]=a.height;result[offset++]=a.getFormat();
            System.arraycopy(a.image,0,result,offset,a.image.length);offset+=a.image.length;}
        return result;
    }
    public static int errors(){int mask=0;
        try{new Image2D(100,2,2,new byte[3]);}catch(IllegalArgumentException e){mask|=1;}
        try{new Image2D(100,0,1);}catch(IllegalArgumentException e){mask|=2;}
        try{new Image2D(95,1,1);}catch(IllegalArgumentException e){mask|=4;}
        try{new Image2D(100,1,1,(byte[])null);}catch(NullPointerException e){mask|=8;}
        try{new Image2D(100,1,1,new byte[]{4},new byte[4]);}catch(ArrayIndexOutOfBoundsException e){mask|=16;}
        try{new Image2D(100,1,1,new byte[4]).set(0,0,1,1,new byte[4]);}catch(IllegalStateException e){mask|=32;}
        try{new Image2D(100,2,2).set(1,1,2,1,new byte[8]);}catch(IllegalArgumentException e){mask|=64;}
        try{new Image2D(100,2,2).set(0,0,2,2,new byte[15]);}catch(IllegalArgumentException e){mask|=128;}
        return mask;
    }
    public static int[] scene(int seed){
        g=Graphics3D.getInstance();if(g.getTarget()!=null)g.releaseTarget();Graphics3D.paintPixel=0x719357ab;
        final int w=240,h=320;Image2D off=null;Image out=null;
        int hints=((seed&1)!=0?Graphics3D.DITHER:0)|((seed&2)!=0?Graphics3D.ANTIALIAS:0);
        if((seed&1)==0){off=new Image2D(Image2D.RGBA,w,h);g.bindTarget(off,(seed&16)==0,hints);}
        else {out=Image.createImage(w,h);Graphics gg=out.getGraphics();
            if((seed&4)!=0){gg.translate(5,-3);gg.setClip(7,9,219,293);}g.bindTarget(gg,(seed&16)==0,hints);}
        if((seed&8)!=0)g.setViewport(3,7,227,301);
        Camera cam=new Camera();cam.setPerspective(60,0.75f,1,30);g.setCamera(cam,null);
        Background bg=new Background();bg.setColor(0x897d354f);g.clear(bg);
        for(int layer=0;layer<3;layer++){
            int tw=(seed%3==0?1:seed%3==1?128:127),th=seed%3==0?129:95;
            Image2D tex=new Image2D(100,tw,th,bytes(seed+layer*97,tw*th*4));
            Appearance ap=new Appearance();CompositingMode cm=new CompositingMode();
            cm.setBlending(64+(seed+layer)%5);cm.setAlphaThreshold((seed%4)*0.2f);
            cm.setDepthTestEnable((seed&32)==0);cm.setDepthWriteEnable((seed&64)==0);
            cm.setColorWriteEnable(seed%11!=0);cm.setAlphaWriteEnable(seed%13!=0);ap.setCompositingMode(cm);
            if((seed&2)!=0){Fog fog=new Fog();fog.setColor(0x657d91);
                if((seed&4)!=0){fog.setMode(Fog.EXPONENTIAL);fog.setDensity(0.15f);}else {fog.setMode(Fog.LINEAR);fog.setLinear(1,8);}ap.setFog(fog);}
            Sprite3D sprite=new Sprite3D((seed&1)==0,tex,ap);
            int cw=tw-((seed&8)!=0?1:0),ch=th+((seed&8)!=0?4:0);cw=Math.max(1,cw);
            sprite.setCrop((seed&8)!=0?-2:0,0,(seed&4)!=0?-cw:cw,(seed&16)!=0?-ch:ch);
            sprite.setAlphaFactor((seed%5)*0.25f);
            Transform t=new Transform();t.postTranslate((layer-1)*0.2f,0,-3.5f-layer*0.25f);t.postScale(3,3,1);
            if((seed&32)!=0){Group group=new Group();group.addChild(sprite);g.render(group,t);}else g.render(sprite,t);
        }
        g.releaseTarget();int[] p=new int[w*h];
        if(off!=null)System.arraycopy(off.image,0,p,0,p.length);else out.getRGB(p,0,w,0,0,w,h);return p;
    }
    public static int spin(int n){float a=0.17f;for(int i=0;i<n;i++)a=(a+0.007f)*0.99f;return Float.floatToRawIntBits(a);}
}
