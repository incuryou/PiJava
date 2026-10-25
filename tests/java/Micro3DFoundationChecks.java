import com.mascotcapsule.micro3d.v3.*;
import javax.microedition.midlet.MIDlet;
import javax.microedition.lcdui.*;

public class Micro3DFoundationChecks extends MIDlet {
    private static int checks;
    private static Light heldLight;
    private static Vector3D heldDirection;
    private static FigureLayout heldLayout;
    private static AffineTrans heldMatrix;
    private static AffineTrans[] heldArray;
    private static void ok(boolean value) { ++checks;if(!value)throw new RuntimeException("M1 check "+checks); }
    private static void vec(Vector3D v,int x,int y,int z) { ok(v.x==x);ok(v.y==y);ok(v.z==z); }
    private static void identity(AffineTrans a) { int[] v=new int[12];a.get(v);for(int i=0;i<12;i++)ok(v[i]==(i==0||i==5||i==10?4096:0)); }
    public static int baseline() {
        try { Class.forName("com.mascotcapsule.micro3d.v3.Vector3D");return 1; }
        catch(ClassNotFoundException e) { return 0; }
    }
    public static int run() {
        checks=0;
        Vector3D v=new Vector3D();vec(v,0,0,0);v.unit();vec(v,0,0,0);
        v.set(3,4,0);Vector3D copy=new Vector3D(v);v.unit();vec(v,2457,3276,0);vec(copy,3,4,0);
        v.set(-3,0,0);v.unit();vec(v,-4096,0,0);
        v.setX(7);v.setY(8);v.setZ(9);ok(v.getX()==7&&v.getY()==8&&v.getZ()==9);copy.set(v);v.x=1;vec(copy,7,8,9);
        ok(Vector3D.innerProduct(new Vector3D(1,2,3),new Vector3D(4,5,6))==32);
        ok(new Vector3D(1,2,3).innerProduct(new Vector3D(4,5,6))==32);
        vec(Vector3D.outerProduct(new Vector3D(1,0,0),new Vector3D(0,1,0)),0,0,1);
        v.set(1,2,3);v.outerProduct(new Vector3D(4,5,6));vec(v,-3,6,-3);v.outerProduct(v);vec(v,0,0,0);
        ok(Util3D.sqrt(0)==0&&Util3D.sqrt(16)==4&&Util3D.sqrt(-17)==4&&Util3D.sqrt(Integer.MIN_VALUE)==46340);
        ok(Util3D.sin(0)==0&&Util3D.sin(1024)==1024&&Util3D.sin(2048)==0&&Util3D.sin(3072)==-1024);
        ok(Util3D.cos(0)==1024&&Util3D.cos(1024)==0&&Util3D.sin(512)==724&&Util3D.sin(-512)==-724);
        ok(Util3D.sin(1)==2&&Util3D.sin(-1)==-2&&Util3D.sin(Integer.MIN_VALUE)==0&&Util3D.cos(Integer.MAX_VALUE)==1024);
        for(int i=-4096;i<4096;i+=73) { ok(Util3D.sin(i)==Util3D.sin(i+4096));ok(Util3D.cos(i)==Util3D.sin(i+1024)); }
        Light light=new Light();vec(light.getDirection(),0,0,4096);ok(light.getDirIntensity()==4096&&light.getAmbientIntensity()==0);
        Vector3D dir=new Vector3D(0,0,4096);light=new Light(dir,2048,256);ok(light.getParallelLightDirection()==dir);dir.x=5;ok(light.getDirection().x==5);
        light.setDirIntensity(-7);light.setAmbIntensity(8192);ok(light.getParallelLightIntensity()==-7&&light.getAmbIntensity()==8192);
        light.setAmbientIntensity(16);light.setParallelLightIntensity(4096);dir.set(0,0,4096);light.setDirection(dir);ok(Micro3DConsumer.shade(light,dir)==4112);
        dir.z=-4096;ok(Micro3DConsumer.shade(light,new Vector3D(0,0,4096))==16);light.setParallelLightDirection(new Vector3D(0,0,4096));
        AffineTrans a=new AffineTrans();int[] all=new int[12];a.get(all);for(int i=0;i<12;i++)ok(all[i]==0);
        a.setIdentity();identity(a);vec(a.transform(new Vector3D(11,-3,7)),11,-3,7);vec(a.transPoint(new Vector3D(2,3,4)),2,3,4);
        a.m03=10;a.m13=20;a.m23=30;a.rotationZ(1024);vec(a.transform(new Vector3D(1,2,3)),8,21,33);ok(a.m03==10&&a.m13==20&&a.m23==30);
        a.setRotationX(1024);vec(a.transform(new Vector3D(1,2,3)),11,17,32);
        a.setRotationY(1024);vec(a.transform(new Vector3D(1,2,3)),13,22,29);
        a.setRotationZ(0);vec(a.transform(new Vector3D(1,2,3)),11,22,33);
        a.setRotation(new Vector3D(0,0,12),1024);vec(a.transform(new Vector3D(1,2,3)),8,21,33);
        a.rotationV(new Vector3D(0,0,1),0);vec(a.transform(new Vector3D(1,2,3)),11,22,33);
        AffineTrans t=new AffineTrans();t.setIdentity();t.m03=3;AffineTrans scale=new AffineTrans();scale.setIdentity();scale.m00=8192;
        a.mul(scale,t);vec(a.transform(new Vector3D(1,0,0)),8,0,0);
        a.mul(t,scale);vec(a.transform(new Vector3D(1,0,0)),5,0,0);
        a.set(scale);a.mul(a,t);vec(a.transform(new Vector3D(1,0,0)),8,0,0);
        a.set(t);a.mul(scale,a);vec(a.transform(new Vector3D(1,0,0)),8,0,0);
        a.set(t);a.multiply(scale);vec(a.transform(new Vector3D(1,0,0)),5,0,0);
        a.multiply(scale,t);vec(a.transform(new Vector3D(1,0,0)),8,0,0);
        a.set(t);a.mul(a,a);vec(a.transform(new Vector3D(1,0,0)),7,0,0);
        int[] seq=new int[14];for(int i=0;i<14;i++)seq[i]=i;a.set(seq,1);a.get(all);for(int i=0;i<12;i++)ok(all[i]==i+1);
        AffineTrans b=new AffineTrans(seq,2);b.get(seq,1);ok(seq[1]==2&&seq[12]==13);a=new AffineTrans(all);b=new AffineTrans(a);ok(b.m23==12);
        a.set(new int[][]{{4096,0,0,0},{0,4096,0,0},{0,0,4096,0}});identity(a);
        b=new AffineTrans(new int[][]{{4096,0,0,0},{0,4096,0,0},{0,0,4096,0}});identity(b);
        a=new AffineTrans(4096,0,0,2,0,4096,0,3,0,0,4096,4);vec(a.transform(new Vector3D(1,1,1)),3,4,5);
        Vector3D pos=new Vector3D(10,20,30),look=new Vector3D(10,20,40),up=new Vector3D(0,4096,13);
        a.lookAt(pos,look,up);vec(a.transform(pos),0,0,0);vec(a.transform(look),0,0,10);ok(up.z==13);
        a.setViewTrans(new Vector3D(),new Vector3D(0,0,1),new Vector3D(0,1,0));identity(a);
        a.m00=2048;vec(a.transform(new Vector3D(-1,0,0)),-1,0,0); // Explicit arithmetic-shift policy.
        FigureLayout layout=new FigureLayout();ok(layout.getAffineTrans()==null&&layout.getScaleX()==512&&layout.getScaleY()==512&&layout.getCenterX()==0&&layout.getCenterY()==0);
        a.setIdentity();layout=new FigureLayout(a,1024,512,10,20);ok(layout.getAffineTrans()==a&&layout.getScaleX()==1024&&layout.getCenterX()==10);
        AffineTrans[] arr={a,b};layout.setAffineTransArray(arr);layout.selectAffineTrans(1);ok(layout.getAffineTrans()==b);arr[1]=t;ok(layout.getAffineTrans()==t);
        layout.setAffineTrans(scale);layout.selectAffineTrans(0);ok(layout.getAffineTrans()==scale);layout.setAffineTrans((AffineTrans)null);ok(layout.getAffineTrans()==null);
        layout.setAffineTrans(new AffineTrans[0]);ok(layout.getAffineTrans()==null);
        layout.setParallelSize(0,2);ok(layout.getParallelWidth()==0&&layout.getParallelHeight()==2&&Micro3DConsumer.projection(layout)==100000);
        layout.setPerspective(1,32767,1);ok(Micro3DConsumer.projection(layout)==232867);
        layout.setPerspective(32766,32767,0,0);ok(Micro3DConsumer.projection(layout)==3609367);
        layout.setScale(-512,512);ok(Micro3DConsumer.projection(layout)==3309367&&layout.getScaleX()==-512);
        for(int i=0;i<29;i++)ok(bad(i)==(i<13?1:2));
        return checks;
    }
    private static int bad(int which) {
        AffineTrans a=new AffineTrans();a.setIdentity();FigureLayout f=new FigureLayout();
        try {
            switch(which) {
            case 0:new Vector3D((Vector3D)null);break;case 1:new Vector3D().set((Vector3D)null);break;
            case 2:Vector3D.innerProduct(null,new Vector3D());break;case 3:new Vector3D().outerProduct(null);break;
            case 4:new Light(null,0,0);break;case 5:new Light().setDirection(null);break;
            case 6:new AffineTrans((AffineTrans)null);break;case 7:a.set((int[])null);break;case 8:a.transform(null);break;case 9:a.mul(null,a);break;
            case 10:a.set(new int[][]{new int[4],null,new int[4]});break;case 11:f.setAffineTrans((AffineTrans[])null);break;case 12:f.setAffineTrans(new AffineTrans[]{a,null});break;
            case 13:a.set(new int[11]);break;case 14:a.get(new int[12],Integer.MAX_VALUE);break;case 15:a.set(new int[12],-1);break;case 16:a.set(new int[][]{new int[4],new int[4],new int[3]});break;
            case 17:a.setRotation(new Vector3D(),1);break;case 18:a.lookAt(new Vector3D(),new Vector3D(),new Vector3D(0,1,0));break;
            case 19:a.lookAt(new Vector3D(),new Vector3D(0,0,1),new Vector3D(0,0,2));break;
            case 20:f.selectAffineTrans(-1);break;case 21:f.selectAffineTrans(1);break;case 22:f.setParallelSize(-1,0);break;
            case 23:f.setPerspective(0,2,1);break;case 24:f.setPerspective(1,32768,1);break;case 25:f.setPerspective(1,2,2048);break;
            case 26:f.setPerspective(2,2,1);break;case 27:f.setPerspective(1,2,-1,0);break;case 28:f.setPerspective(32767,32768,0,0);break;
            }
        } catch(NullPointerException e) { return 1; } catch(IllegalArgumentException e) { return 2; }
        return 0;
    }
    public static int signature() {
        int hash=1;for(int i=0;i<4096;i++)hash=(hash*31+Util3D.sin(i))*31+Util3D.cos(i);
        FigureLayout f=new FigureLayout();AffineTrans a=new AffineTrans();a.setIdentity();a.m03=10;f.setAffineTrans(a);f.setScale(1024,512);f.setCenter(7,8);
        int scene=Micro3DConsumer.scene(f,new Light(new Vector3D(0,0,4096),2048,128));ok(scene==333376);return hash^scene;
    }
    public static int keep() {
        heldMatrix=new AffineTrans();heldMatrix.setIdentity();heldMatrix.m03=10;
        heldDirection=new Vector3D(0,0,4096);heldLight=new Light(heldDirection,2048,128);
        heldArray=new AffineTrans[]{heldMatrix};heldLayout=new FigureLayout();heldLayout.setAffineTrans(heldArray);heldLayout.setCenter(7,8);
        heldLayout.setPerspective(1,100,1024);return 1;
    }
    public static int verify() {
        return heldLight.getDirection()==heldDirection&&heldDirection.z==4096&&heldLayout.getAffineTrans()==heldMatrix&&heldArray[0]==heldMatrix&&heldMatrix.m03==10&&heldLayout.getCenterX()==7&&Micro3DConsumer.projection(heldLayout)==200200?57:0;
    }
    public static int mutate() { heldDirection.z=0;heldMatrix.m03=99;heldLayout.setCenter(99,99);return 1; }
    public static int release() { heldLight=null;heldDirection=null;heldLayout=null;heldMatrix=null;heldArray=null;return 1; }
    public static int discard() { for(int i=0;i<150;i++){Light l=new Light();FigureLayout f=new FigureLayout(new AffineTrans(),512,512,0,0);l.setDirection(new Vector3D(i,1,2));f.getAffineTrans().rotationZ(i);}return 1; }
    protected void startApp() { keep();Display.getDisplay(this).setCurrent(new Canvas(){protected void paint(Graphics g){g.setColor(0x123456);g.fillRect(0,0,getWidth(),getHeight());}}); }
    protected void pauseApp() {}
    protected void destroyApp(boolean unconditional) {}
}
