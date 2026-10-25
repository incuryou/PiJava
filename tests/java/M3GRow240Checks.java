package javax.microedition.m3g;
import javax.microedition.lcdui.Image;
/** Frozen Java row oracle; never calls the new native row method. */
public final class M3GRow240Checks {
    public static Graphics3D g;
    public static short[] depths;
    public static int[] ip=new int[13];
    public static float[] fp=new float[9];
    public static int alphaFactor;
    static float[] invSpan=new float[9];
    static Image target;
    static {for(int i=1;i<=8;i++)invSpan[i]=M3GMath.fastReciprocal((float)i);}
    public static int setupPool(){return M3GSpan239Checks.setupPool();}
    public static int setup(int seed){
        M3GSpan239Checks.setup(seed);
        g=M3GSpan239Checks.g;
        g.releaseTarget();
        if(target==null)target=Image.createImage(1024,1);
        g.bindTarget(target.getGraphics(),true,0);
        System.arraycopy(M3GSpan239Checks.ip,0,ip,0,11);
        System.arraycopy(M3GSpan239Checks.fp,0,fp,0,6);
        int x=seed&7;
        int[] lengths={1,2,7,8,9,15,16,17,31,32,33,63,64,127,128,129,255,256,257,511,512,513,1017,1024};
        int count=lengths[(seed/8)%lengths.length];
        ip[0]=x;ip[1]=x;ip[2]=(seed/8)&1023;ip[3]=count>1024-x?1024-x:count;
        ip[11]=(seed/64)&1;ip[12]=(seed/128&1)==0?Fog.LINEAR:Fog.EXPONENTIAL;
        alphaFactor=M3GSpan239Checks.alphaFactor;
        fp[2]=0.5f+((seed/256)%32)/16.0f;
        fp[3]=(seed%17-8)/32768.0f;
        fp[4]=M3GMath.fastReciprocal(fp[2]);fp[5]=(seed%13-6)/65536.0f;
        fp[6]=1.25f;fp[7]=0.5f;fp[8]=(seed%9)/8.0f;
        for(int i=0;i<4;i++){Graphics3D.stepS[i]/=128.0f;Graphics3D.stepT[i]/=128.0f;}
        if(seed>=12000 && seed<12064){
            float[] edges={Float.intBitsToFloat(0x7fc00000),Float.intBitsToFloat(0x7f800000),Float.intBitsToFloat(0xff800000),
                0.0f,-0.0f,1.0e-30f,-1.0e-30f,32767.75f,32768.0f,-32768.0f,-32768.75f,
                65536.0f,2147483648.0f,-2147483648.0f,1.0e30f,-1.0e30f};
            int n=seed-12000;ip[0]=ip[1]=0;ip[3]=129;ip[5]=45;ip[6]=0;ip[11]=1;
            Graphics3D.ACTIVE_TEXTURE_UNITS=0;g.texMode=g.getTexturingMode();
            if(n<16)fp[0]=edges[n];
            else if(n<32){fp[2]=edges[n&15];fp[3]=0;}
            else if(n<48){fp[8]=edges[n&15];ip[12]=Fog.EXPONENTIAL;}
            else {fp[6]=edges[n&15];ip[12]=Fog.LINEAR;}
        }
        g.rowFog240=-1234567;g.rowFogStep240=7654321;
        return 1;
    }
    public static int custom(int which){return M3GSpan239Checks.custom(which);}
    public static int reference() {
        int index=ip[0],x=ip[1],y=ip[2],count=ip[3],def=ip[4],flags=ip[5],threshold=ip[6];
        int fog=ip[7],fogStep=ip[8],fogRB=ip[9],fogG=ip[10];
        float z=fp[0],zStep=fp[1],pw=fp[2],pwStep=fp[3],inv=fp[4],invStep=fp[5];
        int nextPerspective=0;
        for(int k=0;k<count;k++,index++,x++,z+=zStep,pw+=pwStep,inv+=invStep,fog+=fogStep) {
            if(ip[11]!=0 && k==nextPerspective){
                int spanLen=count-k<8-(x&7)?count-k:8-(x&7);
                float invSpanLen=invSpan[spanLen];
                float denom=pw*(pw+pwStep*spanLen);
                float invDenom=Float.intBitsToFloat(0x7EF127EA-Float.floatToRawIntBits(denom));
                invDenom=invDenom*(2.0f-denom*invDenom);
                invStep=-pwStep*invDenom;
                if((flags&32)!=0){
                    float end=inv+invStep*spanLen;
                    float fs,fe;
                    if(ip[12]==Fog.LINEAR){fs=(fp[6]-inv*fp[7])*256.0f;fe=(fp[6]-end*fp[7])*256.0f;}
                    else{fs=M3GMath.exp(-fp[8]*inv)*256.0f;fe=M3GMath.exp(-fp[8]*end)*256.0f;}
                    fog=(int)((fs<0?0:fs>255?255:fs)*256.0f);
                    fogStep=(int)(((fe<0?0:fe>255?255:fe)-fog*0.00390625f)*invSpanLen*256.0f);
                }
                nextPerspective+=spanLen;
            }
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
        g.rowFog240=fog;g.rowFogStep240=fogStep;
        return Graphics3D.paintPixel;
    }
}
