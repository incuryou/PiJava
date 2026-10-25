package javax.microedition.m3g;
import javax.microedition.lcdui.Image;
/** Frozen pixel-body oracle for stage239. Runs on this VM, not host HotSpot. */
public final class M3GSpan239Checks {
    public static Graphics3D g;
    public static short[] depths; // Connected to g.depthBuffer by the native harness.
    public static int[] ip = new int[11];
    public static float[] fp = new float[6];
    public static int alphaFactor;
    static int rng;
    static int next() { rng=rng*1664525+1013904223;return rng; }
    public static int setupPool() { return M3GTexture238Checks.setup(); }
    public static int setup(int seed) {
        M3GTexture238Checks.sample(seed);
        g=Graphics3D.getInstance();
        if(g.getTarget()!=null)g.releaseTarget();
        g.bindTarget(Image.createImage(16,4).getGraphics(),true,0);
        rng=seed*37+17;
        for(int i=0;i<64;i++)g.rasterData[i]=next();
        int x=(next()>>>1)%16;
        ip[0]=16*((next()>>>1)%4)+x;ip[1]=x;ip[2]=(next()>>>1)%4;
        ip[3]=1+(next()>>>1)%(8-(x&7));ip[4]=next();
        ip[5]=(seed^(seed>>>6))&63;ip[6]=(next()>>>1)%257;
        ip[7]=next();ip[8]=next();ip[9]=next()&0x00ff00ff;ip[10]=next()&255;
        fp[0]=((next()>>>1)%100000)-50000;
        fp[1]=((next()>>>1)%10000-5000)/256.0f;
        fp[2]=((next()>>>1)%100+1)/128.0f;fp[3]=((next()>>>1)%256-128)/8192.0f;
        fp[4]=((next()>>>1)%256+1)/256.0f;fp[5]=((next()>>>1)%256-128)/8192.0f;
        alphaFactor=(next()>>>1)%257;
        g.deltaA=next();g.deltaR=next();g.deltaG=next();g.deltaB=next();
        g.stepA=next();g.stepR=next();g.stepG=next();g.stepB=next();
        int mode=(seed/64)%6;
        g.compBlender=mode==0?null:mode==1?Graphics3DPipelines.CompositingBlenders.ALPHA:
            mode==2?Graphics3DPipelines.CompositingBlenders.ALPHA_ADD:
            mode==3?Graphics3DPipelines.CompositingBlenders.MODULATE:
            mode==4?Graphics3DPipelines.CompositingBlenders.MODULATE_X2:
            Graphics3DPipelines.CompositingBlenders.PASSTHROUGH;
        // Fixed conversion boundaries supplement the 12,000 random cases.
        if(seed>=12000 && seed<12032){
            float[] edges={Float.intBitsToFloat(0x7fc00000),Float.intBitsToFloat(0x7f800000),Float.intBitsToFloat(0xff800000),
                0.0f,-0.0f,1.0e-30f,-1.0e-30f,32767.75f,32768.0f,-32768.0f,
                -32768.75f,65536.0f,2147483648.0f,-2147483648.0f,1.0e30f,-1.0e30f};
            int n=seed-12000;ip[5]=13|(n>=16?32:0);ip[6]=0;
            fp[0]=edges[n&15];fp[1]=(n&1)==0?0.25f:-0.25f;
        }
        if((ip[5]&2)==0)Graphics3D.ACTIVE_TEXTURE_UNITS=0;
        g.texMode=g.getTexturingMode();
        g.spanZ239=-123.25f;g.spanPw239=-23.25f;g.spanInvPw239=-3.25f;g.spanFog239=-17;
        return 1;
    }
    public static int custom(int which) {
        if(which==0)g.compBlender=new Graphics3DPipelines.CompositingBlender(){
            public int blend(int bg,int fg,int a){return bg^fg^a;}};
        if(which==1)g.texMode=new Graphics3D.TexturingMode(){
            public void applyTexture(float inv,int x,int y){Graphics3D.paintPixel^=x+y;}};
        if(which==2)Graphics3D.texWrappers[0]=new Graphics3DPipelines.TextureWrapper(){
            public int wrap(int s,int t,int w,int h){return 0;}};
        return 1;
    }
    public static int reference() {
        int index=ip[0],x=ip[1],y=ip[2],count=ip[3],def=ip[4],flags=ip[5],threshold=ip[6];
        int fog=ip[7],fogStep=ip[8],fogRB=ip[9],fogG=ip[10];
        float z=fp[0],zStep=fp[1],pw=fp[2],pwStep=fp[3],inv=fp[4],invStep=fp[5];
        for(int k=0;k<count;k++,index++,x++,z+=zStep,pw+=pwStep,inv+=invStep,fog+=fogStep) {
            if(k==count-1){g.spanZ239=z;g.spanPw239=pw;g.spanInvPw239=inv;g.spanFog239=fog;}
            if(depths[index]<(short)z) {
                if((flags&1)!=0){g.deltaA+=g.stepA;g.deltaR+=g.stepR;g.deltaG+=g.stepG;g.deltaB+=g.stepB;}
                if((flags&2)!=0)for(byte i=0;i<Graphics3D.ACTIVE_TEXTURE_UNITS;i++){
                    Graphics3D.curS[i]+=Graphics3D.stepS[i];Graphics3D.curT[i]+=Graphics3D.stepT[i];}
                continue;
            }
            if((flags&1)!=0){
                Graphics3D.paintPixel=((g.deltaA>>16&255)<<24)|((g.deltaR>>16&255)<<16)|
                    ((g.deltaG>>16&255)<<8)|(g.deltaB>>16&255);
                g.deltaA+=g.stepA;g.deltaR+=g.stepR;g.deltaG+=g.stepG;g.deltaB+=g.stepB;
            }else Graphics3D.paintPixel=def;
            g.texMode.applyTexture(inv,x,y);
            int alpha=((Graphics3D.paintPixel>>>24)*alphaFactor)>>8;
            if(alpha<threshold)continue;
            if(alpha==0)Graphics3D.paintPixel|=0xff000000;
            if((flags&4)!=0)depths[index]=(short)z;
            if((flags&8)==0)continue;
            if((flags&32)!=0){
                int amount=fog>>8,pixRB=Graphics3D.paintPixel&0x00ff00ff,pixG=(Graphics3D.paintPixel>>8)&255;
                int rb=(fogRB+(((pixRB-fogRB)*amount)>>8))&0x00ff00ff;
                int gg=fogG+(((pixG-fogG)*amount)>>8);
                Graphics3D.paintPixel=(Graphics3D.paintPixel&0xff000000)|rb|(gg<<8);
            }
            if((flags&16)!=0){
                int d=(((x^y)&1)<<3)|((y&1)<<2)|(((x^y)&2)<<1)|((y&2)>>1);
                int r=((Graphics3D.paintPixel>>16)&255)+d;
                int gg=((Graphics3D.paintPixel>>8)&255)+d,b=(Graphics3D.paintPixel&255)+d;
                r=(r|((255-r)>>31))&255;gg=(gg|((255-gg)>>31))&255;b=(b|((255-b)>>31))&255;
                Graphics3D.paintPixel=(Graphics3D.paintPixel&0xff000000)|(r<<16)|(gg<<8)|b;
            }
            g.rasterData[index]=g.compBlender==null?Graphics3D.paintPixel:
                g.compBlender.blend(g.rasterData[index],Graphics3D.paintPixel,alpha);
        }
        return Graphics3D.paintPixel;
    }
}
