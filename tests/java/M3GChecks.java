import javax.microedition.m3g.*;
import javax.microedition.lcdui.*;
import java.io.*;

/** Runs against the bundled CLDC library and emulator native implementation. */
public class M3GChecks extends javax.microedition.midlet.MIDlet {
    protected void startApp() {}
    protected void pauseApp() {}
    protected void destroyApp(boolean unconditional) {}
    static int checks;
    static final byte[] COMPRESSED_BACKGROUND = new byte[] {120, -100, 99, -47, 96, 96, 96, 96, 103, 64, 0, -29, -76, -103, -1, 65, -76, -126, 2, 3, 26, 96, 100, 4, 0, 70, -125, 2, -89};
    static void check(boolean condition, String label) { if (!condition) throw new IllegalStateException(label); checks++; }
    static boolean close(float a,float b) { return Math.abs(a-b)<0.005f; }
    static void le(ByteArrayOutputStream out,int n) { out.write(n); out.write(n>>8); out.write(n>>16); out.write(n>>24); }
    static byte[] fixture(boolean compressed) {
        ByteArrayOutputStream object = new ByteArrayOutputStream();
        object.write(4); le(object,40); le(object,7); le(object,0); le(object,0);
        object.write(0x33); object.write(0x66); object.write(0x99); object.write(0xff);
        le(object,0); object.write(32); object.write(32); for(int i=0;i<4;i++) le(object,0); object.write(1); object.write(1);
        byte[] body = compressed ? COMPRESSED_BACKGROUND : object.toByteArray();
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        byte[] magic = new byte[] {-85,74,83,82,49,56,52,-69,13,10,26,10};
        out.write(magic,0,magic.length);
        out.write(0); le(out,30); le(out,17); out.write(0); le(out,12);
        out.write(1); out.write(0); out.write(0); le(out,12+30+13+body.length); le(out,45); out.write(0); le(out,0);
        out.write(compressed?1:0); le(out,13+body.length); le(out,45); out.write(body,0,body.length); le(out,0);
        return out.toByteArray();
    }
    static void rejected(byte[] data,String label) {
        try { Loader.load(data,0); check(false,label); } catch(IOException expected) { checks++; }
    }
    static void patch(byte[] data,int index,int n) { for(int i=0;i<4;i++) data[index+i]=(byte)(n>>(i*8)); }
    public static int loader() throws IOException {
        checks=0;
        Object3D[] plain=Loader.load(fixture(false),0);
        check(plain.length==1 && plain[0] instanceof Background,"plain M3G root");
        check(plain[0].getUserID()==7,"loaded object ID");
        check(((Background)plain[0]).getColor()==0xff336699,"loaded RGBA color");
        Object3D[] packed=Loader.load(fixture(true),0);
        check(packed.length==1 && ((Background)packed[0]).getColor()==0xff336699,"zlib M3G section");
        byte[] prefixed=new byte[fixture(false).length+3]; System.arraycopy(fixture(false),0,prefixed,3,prefixed.length-3);
        check(Loader.load(prefixed,3).length==1,"offset M3G");
        byte[] trailing=new byte[fixture(false).length+20]; System.arraycopy(fixture(false),0,trailing,0,fixture(false).length);
        check(Loader.load(trailing,0).length==1,"header file size limits embedded data");
        byte[] bad=fixture(false); patch(bad,47,0x7fffffff); rejected(bad,"oversized section rejected before allocation");
        bad=fixture(false); patch(bad,43,-1); rejected(bad,"negative section rejected");
        bad=fixture(false); patch(bad,43,13); rejected(bad,"section/payload mismatch rejected");
        bad=fixture(false); patch(bad,52,0x7fffffff); rejected(bad,"oversized object rejected");
        bad=fixture(false); patch(bad,64,0x7fffffff); rejected(bad,"oversized user parameter count rejected");
        return checks;
    }
    public static int api() {
        checks=0;
        check(((Integer)Graphics3D.getProperties().get("numTextureUnits")).intValue()==4,"texture unit property");
        Transform t=new Transform(); t.postTranslate(2,3,4); t.postScale(2,3,4);
        float[] v=new float[] {1,1,1,1}; t.transform(v);
        check(close(v[0],4)&&close(v[1],6)&&close(v[2],8),"transform composition");
        t.invert(); t.transform(v); check(close(v[0],1)&&close(v[1],1)&&close(v[2],1),"transform inverse");
        t.setIdentity(); t.postRotate(90,0,0,1); v=new float[] {1,0,0,1}; t.transform(v);
        check(close(v[0],0)&&close(v[1],1),"rotation native trigonometry");
        Camera camera=new Camera(); camera.setPerspective(60,1,0.1f,10);
        check(camera.getProjection(new Transform())==Camera.PERSPECTIVE,"perspective camera");
        Camera copy=(Camera)camera.duplicate(); copy.setParallel(4,1,1,10);
        check(camera.getProjection((float[])null)==Camera.PERSPECTIVE,"camera duplicate independence");
        VertexArray positions=new VertexArray(3,3,2); positions.set(0,3,new short[] {-1,-1,0,1,-1,0,0,1,0});
        VertexArray positionsCopy=(VertexArray)positions.duplicate(); positionsCopy.set(0,1,new short[]{5,6,7});
        short[] raw=new short[9]; positions.get(0,3,raw); check(raw[0]==-1,"vertex duplicate independence");
        TriangleStripArray indices=new TriangleStripArray(0,new int[]{3}); int[] index=new int[3]; indices.getIndices(index);
        check(index[0]==0&&index[1]==1&&index[2]==2,"triangle strip expansion");
        KeyframeSequence seq=new KeyframeSequence(2,3,KeyframeSequence.LINEAR); seq.setDuration(1000);
        seq.setKeyframe(0,0,new float[]{0,0,0}); seq.setKeyframe(1,1000,new float[]{10,20,30});
        AnimationController control=new AnimationController(); control.setActiveInterval(0,1001); control.setPosition(0,0);
        AnimationTrack track=new AnimationTrack(seq,AnimationTrack.TRANSLATION); track.setController(control);
        Group animated=new Group(); animated.addAnimationTrack(track); animated.animate(500); float[] translation=new float[3]; animated.getTranslation(translation);
        check(close(translation[0],5)&&close(translation[1],10)&&close(translation[2],15),"linear translation animation");
        Group root=new Group(); root.addChild(animated); animated.setUserID(77); check(root.find(77)==animated,"scene user ID lookup");
        Group rootCopy=(Group)root.duplicate(); check(rootCopy.getChildCount()==1&&rootCopy.getChild(0)!=animated,"deep scene duplication");
        VertexBuffer vb=new VertexBuffer(); vb.setPositions(positions,1,null);
        Group skeleton=new Group(); Group bone=new Group(); skeleton.addChild(bone);
        SkinnedMesh skin=new SkinnedMesh(vb,indices,new Appearance(),skeleton); skin.addTransform(bone,100,0,3);
        int[] boneIndices=new int[3]; float[] weights=new float[3];
        check(skin.getBoneVertices(bone,boneIndices,weights)==3,"skin bone vertex count");
        check(boneIndices[2]==2&&close(weights[0],1),"skin normalized weights");
        Transform boneTransform=new Transform(); skin.getBoneTransform(bone,boneTransform); v=new float[]{1,2,3,1}; boneTransform.transform(v);
        check(close(v[0],1)&&close(v[1],2)&&close(v[2],3),"skin rest transform");
        check(skin.getSkeleton()==skeleton&&skin.getVertexBuffer()==vb,"skin exposes base geometry");
        KeyframeSequence copiedSeq=(KeyframeSequence)seq.duplicate(); copiedSeq.setKeyframe(0,0,new float[]{8,9,10});
        float[] key=new float[3]; seq.getKeyframe(0,key); check(key[0]==0,"keyframe duplicate independence");
        check(root.getReferences(null)>0,"scene references enumeration");
        Image sourceImage=Image.createRGBImage(new int[]{0xff102030},1,1,true);
        Image2D source2D=new Image2D(Image2D.RGB,sourceImage); check(source2D.getWidth()==1&&source2D.getHeight()==1&&!source2D.isMutable(),"Image2D from LCDUI Image");
        Texture2D texture=new Texture2D(source2D); texture.setWrapping(Texture2D.WRAP_CLAMP,Texture2D.WRAP_REPEAT);
        Texture2D textureCopy=(Texture2D)texture.duplicate(); textureCopy.setWrapping(Texture2D.WRAP_REPEAT,Texture2D.WRAP_CLAMP);
        check(texture.getWrappingS()==Texture2D.WRAP_CLAMP&&texture.getWrappingT()==Texture2D.WRAP_REPEAT,"texture duplicate independence");
        MorphingMesh morph=new MorphingMesh(vb,new VertexBuffer[]{vb},indices,new Appearance()); morph.setWeights(new float[]{0.25f}); float[] morphWeights=new float[1]; morph.getWeights(morphWeights);
        check(close(morphWeights[0],0.25f)&&morph.getMorphTargetCount()==1,"morph weights and target API");
        try { new VertexArray(0,3,2); check(false,"invalid vertex count"); } catch(IllegalArgumentException expected) { checks++; }
        try { new TriangleStripArray(0,new int[]{2}); check(false,"invalid strip length"); } catch(IllegalArgumentException expected) { checks++; }
        t.setIdentity(); t.postScale(0,1,1); try { t.invert(); check(false,"singular matrix inverse"); } catch(ArithmeticException expected) { checks++; }
        return checks;
    }
    static VertexBuffer triangle(int color,int z) {
        VertexArray positions=new VertexArray(3,3,2); positions.set(0,3,new short[] {-1,-1,(short)z,1,-1,(short)z,0,1,(short)z});
        VertexBuffer vertices=new VertexBuffer(); vertices.setPositions(positions,1,null); vertices.setDefaultColor(color); return vertices;
    }
    static int pixel(Image target,int x,int y) { int[] out=new int[1]; target.getRGB(out,0,1,x,y,1,1); return out[0]; }
    static Background background(int color) { Background b=new Background(); b.setColor(color); return b; }
    static Appearance appearance() { Appearance a=new Appearance(); PolygonMode p=new PolygonMode(); p.setCulling(PolygonMode.CULL_NONE); a.setPolygonMode(p); return a; }
    public static int render() {
        checks=0; Graphics3D g3=Graphics3D.getInstance(); Image target=Image.createImage(32,32); Graphics g=target.getGraphics();
        g3.bindTarget(g,true,0); g3.setCamera(new Camera(),null); g3.clear(background(0xff000000));
        g3.render(triangle(0xffff0000,0),new TriangleStripArray(0,new int[]{3}),appearance(),null); g3.releaseTarget();
        check((pixel(target,16,16)&0xffffff)==0xff0000,"untextured triangle render");
        check((pixel(target,0,0)&0xffffff)==0,"triangle clipped silhouette");
        VertexBuffer textured=triangle(0xff00ff00,0); VertexArray uv=new VertexArray(3,2,2); uv.set(0,3,new short[]{0,0,0,0,0,0}); textured.setTexCoords(3,uv,1,null);
        Texture2D tex=new Texture2D(new Image2D(Image2D.RGB,1,1,new byte[]{(byte)255,0,0})); tex.setBlending(Texture2D.FUNC_REPLACE);
        tex.setFiltering(Texture2D.FILTER_BASE_LEVEL,Texture2D.FILTER_NEAREST); Appearance app=appearance(); app.setTexture(3,tex);
        g3.bindTarget(g,true,0); g3.clear(background(0xff000000)); g3.render(textured,new TriangleStripArray(0,new int[]{3}),app,null); g3.releaseTarget();
        check((pixel(target,16,16)&0xffffff)==0xff0000,"sparse texture unit three render");
        g3.bindTarget(g,true,0); g3.clear(background(0xff000000));
        g3.render(triangle(0xffff0000,-1),new TriangleStripArray(0,new int[]{3}),appearance(),null);
        g3.render(triangle(0xff0000ff,1),new TriangleStripArray(0,new int[]{3}),appearance(),null); g3.releaseTarget();
        check((pixel(target,16,16)&0xffffff)==0xff0000,"near geometry occludes far geometry");
        g.setColor(0); g.fillRect(0,0,32,32); g.translate(2,3); g.setClip(3,4,10,11);
        g3.bindTarget(g,true,0); g.translate(-2,-3); g.setClip(0,0,32,32); g3.clear(background(0xff12ab34)); g3.releaseTarget();
        check((pixel(target,5,7)&0xffffff)==0x12ab34,"bind captured translated clip inside");
        check((pixel(target,14,17)&0xffffff)==0x12ab34,"bind captured translated clip far corner");
        check((pixel(target,4,7)&0xffffff)==0&&(pixel(target,15,17)&0xffffff)==0,"captured clip excludes neighboring pixels");
        check(g3.getTarget()==null,"release target state");
        VertexBuffer skinVertices=triangle(0xffff8800,0); Group skeleton=new Group(); Group bone=new Group(); skeleton.addChild(bone);
        SkinnedMesh skin=new SkinnedMesh(skinVertices,new TriangleStripArray(0,new int[]{3}),appearance(),skeleton); skin.addTransform(bone,100,0,3);
        g3.bindTarget(g,true,0); g3.clear(background(0xff000000)); g3.render(skin,null); g3.releaseTarget();
        check((pixel(target,16,16)&0xffffff)==0xff8800,"retained skinned mesh render");
        bone.setTranslation(2,0,0); g3.bindTarget(g,true,0); g3.clear(background(0xff000000)); g3.render(skin,null); g3.releaseTarget();
        check((pixel(target,16,16)&0xffffff)==0,"skin bone translation moves rendered geometry");
        World world=new World(); Camera worldCamera=new Camera(); world.addChild(worldCamera); world.setActiveCamera(worldCamera); world.setBackground(background(0xff000000));
        Mesh mesh=new Mesh(triangle(0xff00ff00,0),new TriangleStripArray(0,new int[]{3}),appearance()); world.addChild(mesh);
        g3.bindTarget(g,true,0); g3.render(world); g3.releaseTarget();
        check((pixel(target,16,16)&0xffffff)==0x00ff00,"retained World renders active camera and mesh");
        Background imageBackground=background(0xff000000);
        imageBackground.setImage(new Image2D(Image2D.RGB,2,2,new byte[] {(byte)255,0,0, 0,(byte)255,0, 0,0,(byte)255, (byte)255,(byte)255,(byte)255}));
        imageBackground.setCrop(0,0,2,2); g3.bindTarget(g,true,0); g3.clear(imageBackground); g3.releaseTarget();
        check((pixel(target,4,4)&0xffffff)==0xff0000 && (pixel(target,28,4)&0xffffff)==0x00ff00 && (pixel(target,4,28)&0xffffff)==0x0000ff && (pixel(target,28,28)&0xffffff)==0xffffff,"native background stretches all four image colors");
        imageBackground.setCrop(-1,-1,4,4); imageBackground.setImageMode(Background.REPEAT,Background.REPEAT);
        g3.bindTarget(g,true,0); g3.clear(imageBackground); g3.releaseTarget();
        check((pixel(target,0,0)&0xffffff)==0xffffff && (pixel(target,8,8)&0xffffff)==0xff0000 && (pixel(target,31,31)&0xffffff)==0xff0000,"native repeating background wraps negative crop origins");
        imageBackground.setImageMode(Background.BORDER,Background.BORDER);
        g3.bindTarget(g,true,0); g3.clear(imageBackground); g3.releaseTarget();
        check((pixel(target,0,0)&0xffffff)==0 && (pixel(target,8,8)&0xffffff)==0xff0000 && (pixel(target,31,31)&0xffffff)==0,"native border background retains clear color outside image");
        g.setColor(0); g.fillRect(0,0,32,32); g.translate(2,3); g.setClip(3,4,10,11); imageBackground.setCrop(0,0,2,2);
        g3.bindTarget(g,true,0); g.translate(-2,-3); g.setClip(0,0,32,32); g3.clear(imageBackground); g3.releaseTarget();
        check((pixel(target,5,7)&0xffffff)==0xff0000 && (pixel(target,14,17)&0xffffff)==0xffffff && (pixel(target,4,7)&0xffffff)==0 && (pixel(target,15,17)&0xffffff)==0,"native image background preserves captured translated clip");
        return checks;
    }
    public static int run() throws IOException { return all(); }
    public static int all() throws IOException { return api()+loader()+render(); }
    public static int loadResource(String name) throws IOException { Object3D[] objects=Loader.load(name); return objects==null?-1:objects.length; }
}
