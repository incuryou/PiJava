package javax.microedition.m3g;
import javax.microedition.lcdui.Image;
import javax.microedition.lcdui.Graphics;
/** Real rendering entry points, clipped 2D and Image2D offscreen targets. */
public final class M3GSpan239FrameChecks {
    public static Graphics3D g;
    public static int[] frame(int seed) {
        g=Graphics3D.getInstance();
        if(g.getTarget()!=null)g.releaseTarget();
        Graphics3D.paintPixel=0x719357ab;
        for(int i=0;i<4;i++){Graphics3D.curS[i]=0;Graphics3D.curT[i]=0;}
        g.deltaA=g.deltaR=g.deltaG=g.deltaB=0;
        Image target=Image.createImage(64,64);
        Image2D offscreen=null;
        int hints=((seed&1)!=0?Graphics3D.DITHER:0)|((seed&2)!=0?Graphics3D.ANTIALIAS:0);
        if(seed%5==0){offscreen=new Image2D(Image2D.RGBA,64,64);g.bindTarget(offscreen,true,hints);}
        else {
            Graphics gg=target.getGraphics();
            if((seed&4)!=0){gg.translate(3,-2);gg.setClip(2,5,55,52);}
            g.bindTarget(gg,true,hints);
        }
        if((seed&8)!=0)g.setViewport(-2,3,62,58);
        Camera camera=new Camera();camera.setPerspective(65,1,1,25);g.setCamera(camera,null);
        Background bg=new Background();bg.setColor(0x81395773);g.clear(bg);
        int units=(seed/4)%4;units=units==3?4:units;
        VertexArray pos=new VertexArray(4,3,2);pos.set(0,4,new short[]{-3,-2,-4,3,-2,-6,-3,3,-5,3,3,-7});
        VertexBuffer vertices=new VertexBuffer();vertices.setPositions(pos,1,null);vertices.setDefaultColor(0xc8d09bb7);
        if((seed&16)!=0){VertexArray c=new VertexArray(4,4,1);c.set(0,4,new byte[]{-1,0,80,-1,0,-1,90,30,10,80,-1,120,-1,-1,20,0});vertices.setColors(c);}
        Appearance app=new Appearance();
        PolygonMode poly=new PolygonMode();poly.setCulling(PolygonMode.CULL_NONE);app.setPolygonMode(poly);
        for(int u=0;u<units;u++){
            VertexArray uv=new VertexArray(4,2,2);uv.set(0,4,new short[]{-22,-11,61,-11,-11,71,49,59});vertices.setTexCoords(u,uv,1,null);
            int w=seed%3==0?7:seed%3==1?8:1,h=seed%3==0?5:8;
            int fmt=96+(seed+u)%5,channels=fmt==96||fmt==97?1:fmt==98?2:fmt==99?3:4;
            byte[] data=new byte[w*h*channels];for(int i=0;i<data.length;i++)data[i]=(byte)(i*31+seed*17+u*23);
            Texture2D tex=new Texture2D(new Image2D(fmt,w,h,data));
            tex.setFiltering(208+(seed+u)%3,209+(seed+u)%2);
            tex.setWrapping((seed&32)!=0?241:240,(seed&64)!=0?241:240);
            tex.setBlending(224+(seed+u)%5);tex.setBlendColor(0x798da7+seed);
            app.setTexture(u,tex);
        }
        if((seed&128)!=0){Fog fog=new Fog();fog.setColor(0x738491);
            if((seed&64)!=0){fog.setMode(Fog.EXPONENTIAL);fog.setDensity(0.15f);}
            else {fog.setMode(Fog.LINEAR);fog.setLinear(2,9);}app.setFog(fog);}
        CompositingMode comp=new CompositingMode();comp.setBlending(64+(seed/3)%5);
        comp.setAlphaThreshold((seed%4)/4.0f);comp.setDepthTestEnable((seed&32)==0);
        comp.setDepthWriteEnable((seed&64)==0);comp.setColorWriteEnable(seed%17!=0);app.setCompositingMode(comp);
        TriangleStripArray tri=new TriangleStripArray(0,new int[]{4});
        g.render(vertices,tri,app,null);
        Transform trans=new Transform();trans.postTranslate(0.5f,-0.25f,(seed&1)!=0?-1:1);
        g.render(vertices,tri,app,trans);
        g.releaseTarget();
        int[] pixels=new int[4096];
        if(offscreen!=null)System.arraycopy(offscreen.image,0,pixels,0,4096);
        else target.getRGB(pixels,0,64,0,0,64,64);
        return pixels;
    }
}
