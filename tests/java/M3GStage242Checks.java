package javax.microedition.m3g;
/** Deterministic geometry regression fixtures. Not a commercial-game benchmark. */
public final class M3GStage242Checks {
    private static int rng;
    static int next(){rng=rng*1664525+1013904223;return rng;}
    static float f(){return ((next()>>>8)%20001-10000)*0.0001f;}
    static VertexArray attrs(int n,int c,int type,int seed,boolean norm){
        VertexArray a=new VertexArray(n,c,type);rng=seed;
        if(type==1){byte[] v=new byte[n*c];for(int i=0;i<v.length;i++)v[i]=(byte)(norm?(next()>>>24):(next()%65));a.set(0,n,v);}
        else{short[] v=new short[n*c];for(int i=0;i<v.length;i++)v[i]=(short)(norm?(next()>>>16):(next()%1001));a.set(0,n,v);}
        return a;
    }
    static VertexBuffer base(int n,int c,int type,int seed){
        VertexBuffer vb=new VertexBuffer();vb.setPositions(attrs(n,c,type,seed,false),type==1?0.035f:0.0025f,new float[]{0.25f,-0.125f,0.0625f});
        if(c==3 && seed%5!=0)vb.setNormals(attrs(n,3,(seed/2)%2+1,seed+37,true));
        vb.setDefaultColor(0xffaf7139);if(seed%3!=0)vb.setColors(attrs(n,seed%2+3,1,seed+43,true));
        return vb;
    }
    static SkinnedMesh makeSkin(VertexBuffer vb,Appearance app,int seed,Group[] bone){
        Group skeleton=new Group();int n=vb.getVertexCount();
        for(int i=0;i<bone.length;i++){bone[i]=new Group();(i>0&&seed%2==0?bone[i-1]:skeleton).addChild(bone[i]);
            bone[i].setTranslation(i*0.013f,-i*0.017f,i*0.003f);}
        SkinnedMesh skin=new SkinnedMesh(vb,new TriangleStripArray(0,new int[]{n}),app,skeleton);
        for(int i=0;i<bone.length;i++){int first=(i*13+seed)%n,count=Math.max(1,(n-first)/(i%3+1));
            skin.addTransform(bone[i],i==1&&seed%11==0?2147483647:1+i*97,first,count);}
        for(int i=0;i<bone.length;i++){bone[i].setTranslation(i*0.09f,0.12f,-0.05f);bone[i].setOrientation((seed+i*7)%73,1,2,3);
            if(seed%7==0)bone[i].setScale(1.2f,-0.75f,0.8f);}
        return skin;
    }
    static void append(int[] out,int[] cursor,VertexArray a){
        if(a==null){out[cursor[0]++]=-1;return;}out[cursor[0]++]=a.getComponentCount();out[cursor[0]++]=a.getComponentType();
        if(a.getComponentType()==1){byte[] b=new byte[a.getVertexCount()*a.getComponentCount()];a.get(0,a.getVertexCount(),b);for(int i=0;i<b.length;i++)out[cursor[0]++]=b[i];}
        else{short[] b=new short[a.getVertexCount()*a.getComponentCount()];a.get(0,a.getVertexCount(),b);for(int i=0;i<b.length;i++)out[cursor[0]++]=b[i];}
    }
    public static int[] pose(int seed){
        int[] sizes={3,4,17,127,128,129,257,513};int n=sizes[seed%sizes.length],c=3,type=seed%2+1;
        VertexBuffer vb=base(n,c,type,seed);Group[] bone=new Group[1+(seed/3)%16];SkinnedMesh skin=makeSkin(vb,new Appearance(),seed,bone);
        int[] out=new int[n*c*9+100];int[] cursor={0};
        for(int frame=0;frame<3;frame++){
            bone[0].setTranslation(frame*0.33f,0,-frame*0.125f);
            if(frame==2)vb.setPositions(vb.getPositions(null),seed%13==0?-0.025f:0.02f,new float[]{0.1f,0.2f,-0.3f});
            VertexBuffer posed=skin.getPosedVertexBuffer();float[] sb=new float[4];append(out,cursor,posed.getPositions(sb));append(out,cursor,posed.getNormals());
            for(int i=0;i<4;i++)out[cursor[0]++]=Float.floatToRawIntBits(sb[i]);
        }
        append(out,cursor,vb.getPositions(null));append(out,cursor,vb.getNormals());out[cursor[0]++]=skin.getVertexBuffer()==vb?1:0;
        int[] result=new int[cursor[0]];System.arraycopy(out,0,result,0,result.length);return result;
    }
    public static int[] scene(int seed){
        Graphics3D g=Graphics3D.getInstance();if(g.getTarget()!=null)g.releaseTarget();
        Image2D out=new Image2D(100,96,96);g.bindTarget(out,true,seed%3==0?6:0);g.resetLights();
        World world=new World();Camera cam=new Camera();cam.setPerspective(58,1,1,30);world.addChild(cam);world.setActiveCamera(cam);
        Background bg=new Background();bg.setColor(0xff1b2233);world.setBackground(bg);
        for(int i=0;i<4;i++){Light l=new Light();l.setMode(128+i);l.setColor(0x815337+i*0x102031);l.setIntensity(0.4f+i*0.25f);
            l.setTranslation(i-1,2,0);l.setOrientation(17*i,1,0,0);if(i==3){l.setSpotAngle(75);l.setSpotExponent(seed%13);}world.addChild(l);}
        Appearance a=new Appearance();Material m=new Material();m.setColor(Material.AMBIENT,0x204080);m.setColor(Material.DIFFUSE,0xffbda371);
        m.setColor(Material.SPECULAR,seed%4==0?0x91abcf:0x101010);m.setShininess(seed%33);m.setVertexColorTrackingEnable(seed%2==0);a.setMaterial(m);
        PolygonMode pm=new PolygonMode();pm.setCulling(160+seed%3);pm.setShading(seed%4==0?164:165);pm.setTwoSidedLightingEnable(seed%2!=0);pm.setLocalCameraLightingEnable(seed%3==0);pm.setPerspectiveCorrectionEnable(seed%2==0);a.setPolygonMode(pm);
        if(seed%2!=0){Fog fog=new Fog();fog.setColor(0x394f87);fog.setLinear(2,12);a.setFog(fog);}
        VertexBuffer vb=base(17+seed%19,3,seed%2+1,seed+3);
        if(seed%3==0){VertexArray uv=attrs(vb.getVertexCount(),2,2,seed+11,false);vb.setTexCoords(0,uv,0.002f,null);
            byte[] bytes=new byte[17*19*4];rng=seed;for(int i=0;i<bytes.length;i++)bytes[i]=(byte)(next()>>>24);Texture2D tex=new Texture2D(new Image2D(100,17,19,bytes));tex.setFiltering(Texture2D.FILTER_LINEAR,Texture2D.FILTER_LINEAR);a.setTexture(0,tex);}
        Group[] bones=new Group[1+seed%8];SkinnedMesh skin=makeSkin(vb,a,seed,bones);skin.setTranslation(0,0,seed%7==0?-1.1f:-4);world.addChild(skin);
        g.clear(bg);g.render(world);bones[0].setOrientation(53,0,1,0);skin.setTranslation(0.2f,0.1f,-3.5f);g.render(world);
        int[] pixels=new int[96*96];System.arraycopy(out.image,0,pixels,0,pixels.length);g.releaseTarget();return pixels;
    }
    public static int[] transform(int seed){
        Group root=new Group(),a=new Group(),b=new Group();root.addChild(a);root.addChild(b);rng=seed;
        for(int i=0;i<seed%11;i++){Group c=new Group();a.addChild(c);a=c;a.setTranslation(f(),f(),f());a.setOrientation(f()*75,1,2,3);a.setScale(0.7f+f()*0.2f,1.1f,1.3f);}
        for(int i=0;i<(seed/7)%9;i++){Group c=new Group();b.addChild(c);b=c;b.setTranslation(f(),f(),f());b.setOrientation(f()*67,2,1,-1);}
        Transform out=new Transform();int[] data=new int[16*9];int at=0;Node[] x={a,b,root,a};Node[] y={b,a,a,a};
        for(int j=0;j<4;j++){if(!x[j].getTransformTo(y[j],out))return new int[]{-999};float[] mat=new float[16];out.get(mat);for(int i=0;i<16;i++)data[at++]=Float.floatToRawIntBits(mat[i]);}
        for(Transform t:new Transform[]{a.thisToRoot,a.targetToRoot,a.temp,b.thisToRoot,b.targetToRoot}){float[] mat=new float[16];t.get(mat);for(int i=0;i<16;i++)data[at++]=Float.floatToRawIntBits(mat[i]);}
        return data;
    }
    public static int[] inverse(int seed){
        rng=seed;float[] m=new float[16];for(int i=0;i<16;i++)m[i]=f()+(i%5==0?2:0);
        int[] edge={0,0x80000000,1,0x80000001,0x007fffff,0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,0x7fc00001,0x7fa00001,0xffc01234};
        if(seed%3==0)for(int i=0;i<16;i++)m[i]=i%5==0?Float.intBitsToFloat(edge[(seed/3)%edge.length]):0;
        Transform t=new Transform();t.set(m);int[] data=new int[17];try{t.invert();}catch(ArithmeticException e){data[0]=1;}
        t.get(m);for(int i=0;i<16;i++)data[i+1]=Float.floatToRawIntBits(m[i]);return data;
    }
    public static int overrideCalls;
    public static class CustomGroup extends Group {
        public void getCompositeTransform(Transform out){overrideCalls++;super.getCompositeTransform(out);out.postTranslate(0.125f,0.25f,-0.5f);}
    }
    public static class ShadowGroup extends Group {
        public Node parent=null;
        private Transform translate=new Transform();
        ShadowGroup(){translate.postTranslate(123,456,789);}
    }
    public static class CustomVertexArray extends VertexArray {
        CustomVertexArray(){super(17,3,2);}
        public int getVertexCount(){overrideCalls++;return super.getVertexCount();}
        public int getComponentCount(){overrideCalls++;return super.getComponentCount();}
        public int getComponentType(){overrideCalls++;return super.getComponentType();}
        public void get(int first,int count,short[] values){overrideCalls++;super.get(first,count,values);}
    }
    public static int[] customPose(int seed){
        overrideCalls=0;VertexBuffer vb=new VertexBuffer();VertexArray p=new CustomVertexArray();short[] a=new short[51];rng=seed;for(int i=0;i<a.length;i++)a[i]=(short)(next()%1001);p.set(0,17,a);vb.setPositions(p,0.01f,null);
        vb.setNormals(attrs(17,3,2,seed+1,true));Group[] bone=new Group[3];SkinnedMesh mesh=makeSkin(vb,new Appearance(),seed,bone);
        int[] result=new int[210];int[] cursor={0};VertexBuffer pose=mesh.getPosedVertexBuffer();append(result,cursor,pose.getPositions(null));append(result,cursor,pose.getNormals());result[cursor[0]++]=overrideCalls;return result;
    }
    public static int[] treeFallback(int seed){
        overrideCalls=0;Group root=new Group(),a=new Group(),b=new Group();root.addChild(a);root.addChild(b);
        int depth=seed%2==0?129:3;
        for(int i=0;i<depth;i++){Group c=seed%2!=0&&i==1?(seed%4==3?new ShadowGroup():new CustomGroup()):new Group();a.addChild(c);a=c;c.setTranslation(0.001f*i,0.125f,-0.25f);}
        Transform t=new Transform();a.getTransformTo(b,t);float[] m=new float[16];t.get(m);int[] data=new int[17];data[0]=overrideCalls;
        for(int i=0;i<16;i++)data[i+1]=Float.floatToRawIntBits(m[i]);return data;
    }
    public static float[] lightEye,lightPos,lightDir;
    public static VertexArray lightNorm;
    public static Transform lightMatrix;
    public static Material lightMaterial;
    public static M3GVector<Light> testLights;
    public static int[] lightTris,lightOut;
    public static void lightingSetup(int seed){
        rng=seed;lightEye=new float[28];for(int i=0;i<28;i++)lightEye[i]=f()*4;
        lightNorm=attrs(7,3,seed%2+1,seed+17,true);if(seed%23==0){if(seed%2==0)lightNorm.set(0,7,new byte[21]);else lightNorm.set(0,7,new short[21]);}
        lightMatrix=new Transform();lightMatrix.postRotate(seed%83,1,2,-1);lightMatrix.postScale(1.1f,0.7f,1.2f);
        lightMaterial=new Material();lightMaterial.setVertexColorTrackingEnable(seed%2==0);
        testLights=new M3GVector<Light>();int n=seed%37==0?32:seed%9;lightPos=new float[n*4];lightDir=new float[n*4];
        for(int i=0;i<n;i++){Light l=new Light();l.setMode(128+(seed+i)%4);l.setColor(next());l.setIntensity(0.1f+(next()&127)*0.015f);
            l.setAttenuation(0.8f,0.07f,0.01f);l.setSpotAngle(30+seed%50);l.setSpotExponent(seed%16);l.setRenderingEnable((seed+i)%7!=0);l.setScope(i%3+1);
            testLights.addElement(l);for(int j=0;j<3;j++){lightPos[i*4+j]=f()*5;lightDir[i*4+j]=f();}}
        lightTris=new int[]{seed%7,(seed+1)%7,(seed+2)%7};lightOut=new int[]{next(),next(),next()};
    }
    public static int[] assembly(int seed){
        int units=new int[]{0,1,2,4}[seed%4];Graphics3D.ACTIVE_TEXTURE_UNITS=(byte)units;int n=3+seed%13;VertexBuffer vb=base(n,3,seed%2+1,seed+3);rng=seed;
        float[] v=new float[n*4];float[][] t=new float[4][];
        for(int i=0;i<n;i++){v[4*i]=f()*2;v[4*i+1]=f()*2;v[4*i+2]=f()*2;v[4*i+3]=0.5f+Math.abs(f());}
        for(int u=0;u<units;u++){if(u==2&&seed%3==0)continue;t[u]=new float[n*4];for(int i=0;i<n*4;i++)t[u][i]=f();}
        int[] indices=new int[(n-2)*3];for(int i=0;i<n-2;i++){indices[i*3]=i+(i%2);indices[i*3+1]=i+1-(i%2);indices[i*3+2]=i+2;}
        int[] count={0};Triangle[] triangles=Triangle.fromVertAndTris(v,t,null,165,false,false,null,null,null,null,null,null,-1,indices,count,160+seed%3,vb,seed%2==0,seed%3!=0,seed%2!=0);
        int[] data=new int[1+count[0]*(23+units*6)];int at=0;data[at++]=count[0];
        for(int i=0;i<count[0];i++){Triangle a=triangles[i];float[] values={a.xA(),a.yA(),a.zA(),a.wA(),a.xB(),a.yB(),a.zB(),a.wB(),a.xC(),a.yC(),a.zC(),a.wC(),a.iwA(),a.iwB(),a.iwC()};
            for(int j=0;j<values.length;j++)data[at++]=Float.floatToRawIntBits(values[j]);
            data[at++]=a.hasVertexColors()?1:0;data[at++]=a.hasVertexColors()?a.colorA():0;data[at++]=a.hasVertexColors()?a.colorB():0;data[at++]=a.hasVertexColors()?a.colorC():0;
            data[at++]=seed%2!=0&&a.edgeABBoundary?1:0;data[at++]=seed%2!=0&&a.edgeBCBoundary?1:0;data[at++]=seed%2!=0&&a.edgeCABoundary?1:0;
            for(int u=0;u<units;u++){float[] tex=t[u]==null?new float[6]:new float[]{a.sA(u),a.tA(u),a.sB(u),a.tB(u),a.sC(u),a.tC(u)};for(int j=0;j<6;j++)data[at++]=Float.floatToRawIntBits(tex[j]);}
        }
        Graphics3D.ACTIVE_TEXTURE_UNITS=0;return data;
    }
    public static int errors(){int bits=0;Group root=new Group(),bone=new Group();root.addChild(bone);VertexBuffer vb=base(3,3,2,1);SkinnedMesh skin=new SkinnedMesh(vb,new TriangleStripArray(0,new int[]{3}),new Appearance(),root);
        skin.addTransform(bone,1,0,4);try{skin.getPosedVertexBuffer();}catch(IllegalStateException e){bits|=1;}
        Transform t=new Transform();t.set(new float[16]);try{t.invert();}catch(ArithmeticException e){bits|=2;}
        try{root.getTransformTo(null,t);}catch(NullPointerException e){bits|=4;}if(!root.getTransformTo(new Group(),t))bits|=8;
        try{root.getCompositeTransform(null);}catch(NullPointerException e){bits|=16;}
        return bits;
    }
}
