package javax.microedition.m3g;
/** Differential fixture: public entry points run on the custom VM, not HotSpot. */
public class M3GTexture238Checks {
    static Texture2D[] pool;
    static Graphics3D g;
    public static int[] result = new int[9];
    static int rng;
    static int next(){rng=rng*1664525+1013904223;return rng;}
    static float coord(){return ((next()>>>8)%120000-60000)/256.0f;}
    static float slope(){return ((next()>>>8)%8192-4096)/256.0f;}
    public static int setup(){
        g=Graphics3D.getInstance();pool=new Texture2D[15];
        for(int i=0;i<15;i++){
            int fmt=96+i%5,channels=fmt==96||fmt==97?1:fmt==98?2:fmt==99?3:4;
            int w=i<5?8:i<10?7:1,h=i<5?8:i<10?5:16;
            byte[] data=new byte[w*h*channels];rng=i+100;for(int j=0;j<data.length;j++)data[j]=(byte)(next()>>>16);
            pool[i]=new Texture2D(new Image2D(fmt,w,h,data));
        }
        return pool.length;
    }
    public static int sample(int seed){
        rng=seed;
        int count=(seed%3+3)%3;count=count==0?1:count==1?2:4;
        Graphics3D.ACTIVE_TEXTURE_UNITS=(byte)count;
        Graphics3D.paintPixel=next();
        Graphics3D.dwdx=slope()/128.0f;Graphics3D.dwdy=slope()/128.0f;
        for(int i=0;i<4;i++){
            Graphics3D.curS[i]=coord();Graphics3D.curT[i]=coord();Graphics3D.stepS[i]=slope();Graphics3D.stepT[i]=slope();
            Graphics3D.sStepX[i]=slope();Graphics3D.tStepX[i]=slope();Graphics3D.sStepY[i]=slope();Graphics3D.tStepY[i]=slope();
            Texture2D t=pool[(next()>>>1)%pool.length];Graphics3D.textures[i]=t;t.setBlendColor(next());
            int lf=208+(next()>>>1)%3,filter=209+(next()>>>1)%2;
            t.setFiltering(lf,filter);boolean rs=(next()&4)!=0,rt=(next()&8)!=0;t.setWrapping(rs?241:240,rt?241:240);
            Graphics3D.texRepeatS[i]=rs;Graphics3D.texRepeatT[i]=rt;
            Graphics3D.texWrappers[i]=g.getCoordWrapper(t.getImage().getWidth(),t.getImage().getHeight(),rs,rt,t.isNPOT());
            Graphics3D.texFilter[i]=g.getTextureFilter(filter==209);Graphics3D.mipModes[i]=g.getMipmapMode(lf);
            int blend=(next()>>>1)%11;
            switch(blend){
            case 0:Graphics3D.texBlenders[i]=null;break;
            case 1:Graphics3D.texBlenders[i]=Graphics3DPipelines.TextureBlenders.PASSTHROUGH;break;
            case 2:Graphics3D.texBlenders[i]=Graphics3DPipelines.TextureBlenders.REPLACE_ALPHA;break;
            case 3:Graphics3D.texBlenders[i]=Graphics3DPipelines.TextureBlenders.ADD_RGB;break;
            case 4:Graphics3D.texBlenders[i]=Graphics3DPipelines.TextureBlenders.ADD_RGBA;break;
            case 5:Graphics3D.texBlenders[i]=Graphics3DPipelines.TextureBlenders.ALPHA_MUL;break;
            case 6:Graphics3D.texBlenders[i]=Graphics3DPipelines.TextureBlenders.BLEND_RGBA;break;
            case 7:Graphics3D.texBlenders[i]=Graphics3DPipelines.TextureBlenders.BLEND_RGB;break;
            case 8:Graphics3D.texBlenders[i]=Graphics3DPipelines.TextureBlenders.DECAL_RGBA;break;
            case 9:Graphics3D.texBlenders[i]=Graphics3DPipelines.TextureBlenders.MODULATE_RGBA;break;
            default:Graphics3D.texBlenders[i]=Graphics3DPipelines.TextureBlenders.MODULATE_RGB;
            }
        }
        float pw=((next()>>>1)%500+1)/128.0f;int x=next(),y=next();
        g.getTexturingMode().applyTexture(pw,x,y);
        result[0]=Graphics3D.paintPixel;
        for(int i=0;i<4;i++){result[1+i*2]=Float.floatToRawIntBits(Graphics3D.curS[i]);result[2+i*2]=Float.floatToRawIntBits(Graphics3D.curT[i]);}
        return result[0];
    }
    public static int bench(int n){
        sample(42);
        for(int j=0;j<4;j++){Graphics3D.stepS[j]=0;Graphics3D.stepT[j]=0;}
        int sum=0;
        for(int j=0;j<n;j++){
            Graphics3D.paintPixel=0xA871D39F;
            g.getTexturingMode().applyTexture(1.125f,j&255,(j>>>8)&255);
            sum=sum*31+Graphics3D.paintPixel;
        }
        return sum;
    }
    public static int wrapChecks(){
        int[] sizes={1,2,3,5,7,13,31,511,1023};
        int[] coords={-2147483648,-32000,-32768,-511,-105,-14,-13,-7,-6,-5,-1,0,1,5,7,13,511,32767,2147483647};
        int cases=0;
        for(int w=0;w<sizes.length;w++)for(int h=0;h<sizes.length;h++)for(int j=0;j<coords.length;j++){
            int s=coords[j],t=coords[coords.length-1-j],x=s%sizes[w],y=t%sizes[h];if(x<0)x+=sizes[w];if(y<0)y+=sizes[h];
            int got=Graphics3DPipelines.TextureWrappers.NPOT_REPEAT.wrap(s,t,sizes[w],sizes[h]);
            if(got!=((y<<16)|x))throw new RuntimeException("NPOT wrap boundary");cases++;
        }
        return cases;
    }
    static class OverrideTexture extends Texture2D {
        int calls;
        OverrideTexture(Image2D image){super(image);}
        public Image2D getImage(){calls++;return super.getImage();}
    }
    public static int subclassFallback(){
        OverrideTexture t=new OverrideTexture(pool[0].getImage());
        Graphics3D.ACTIVE_TEXTURE_UNITS=1;Graphics3D.textures[0]=t;
        Graphics3D.texWrappers[0]=Graphics3DPipelines.TextureWrappers.CLAMP;
        Graphics3D.texBlenders[0]=null;Graphics3D.texFilter[0]=g.getTextureFilter(false);Graphics3D.mipModes[0]=null;
        Graphics3D.curS[0]=1;Graphics3D.curT[0]=1;
        g.getTexturingMode().applyTexture(1,0,0);
        return t.calls;
    }
    public static int fallback(){
        Graphics3D.ACTIVE_TEXTURE_UNITS=1;
        Graphics3D.texWrappers[0]=new Graphics3DPipelines.TextureWrapper(){public int wrap(int s,int t,int w,int h){return 0;}};
        Graphics3D.texBlenders[0]=null;Graphics3D.texFilter[0]=g.getTextureFilter(false);Graphics3D.mipModes[0]=null;
        Graphics3D.curS[0]=12;Graphics3D.curT[0]=13;Graphics3D.stepS[0]=1;Graphics3D.stepT[0]=2;
        Graphics3D.paintPixel=0xAABBCCDD;g.getTexturingMode().applyTexture(1.0f,1,1);
        return Graphics3D.paintPixel;
    }
}
