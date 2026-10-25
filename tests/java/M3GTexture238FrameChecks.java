package javax.microedition.m3g;
import javax.microedition.lcdui.Image;
/** Full-image differential fixture, including the unchanged fog/depth/dither path. */
public final class M3GTexture238FrameChecks {
    public static int[] frame(int config) {
        Image target=Image.createImage(48,48);
        Graphics3D g=Graphics3D.getInstance();
        int hints=((config&1)!=0?Graphics3D.DITHER:0)|((config&2)!=0?Graphics3D.ANTIALIAS:0);
        g.bindTarget(target.getGraphics(),true,hints);
        Camera camera=new Camera();camera.setPerspective(65,1,1,20);g.setCamera(camera,null);
        Background bg=new Background();bg.setColor(0xff243857);g.clear(bg);
        VertexArray pos=new VertexArray(3,3,2);pos.set(0,3,new short[]{-3,-2,-4,3,-2,-6,0,3,-5});
        VertexArray uv=new VertexArray(3,2,2);uv.set(0,3,new short[]{-2,-1,6,-1,1,7});
        VertexBuffer vertices=new VertexBuffer();vertices.setPositions(pos,1,null);vertices.setDefaultColor(0xc8d09bb7);
        vertices.setTexCoords(0,uv,1,null);
        byte[] rgb=new byte[16*16*4];for(int i=0;i<256;i++){rgb[i*4]=(byte)(i*17);rgb[i*4+1]=(byte)(i*31);rgb[i*4+2]=(byte)(i*7);rgb[i*4+3]=(byte)(96+i%160);}
        Texture2D tex=new Texture2D(new Image2D(Image2D.RGBA,16,16,rgb));
        tex.setWrapping(Texture2D.WRAP_REPEAT,Texture2D.WRAP_REPEAT);
        tex.setFiltering((config&4)!=0?Texture2D.FILTER_LINEAR:Texture2D.FILTER_NEAREST,(config&8)!=0?Texture2D.FILTER_LINEAR:Texture2D.FILTER_NEAREST);
        tex.setBlending(Texture2D.FUNC_MODULATE);
        Appearance a=new Appearance();a.setTexture(0,tex);PolygonMode poly=new PolygonMode();poly.setCulling(PolygonMode.CULL_NONE);a.setPolygonMode(poly);
        if((config&16)!=0){Fog fog=new Fog();fog.setMode(Fog.LINEAR);fog.setLinear(2,9);fog.setColor(0x738491);a.setFog(fog);}
        g.render(vertices,new TriangleStripArray(0,new int[]{3}),a,null);g.releaseTarget();
        int[] pixels=new int[48*48];target.getRGB(pixels,0,48,0,0,48,48);return pixels;
    }
}
