package com.mascotcapsule.micro3d.v3;

// Owned numeric consumer, deliberately not a Graphics3D/renderer replacement.
public final class Micro3DConsumer {
    public static int projection(FigureLayout f) {
        return f.projectionMode*100000 + f.zNear*100 + f.zFar;
    }
    public static int shade(Light light,Vector3D normal) {
        Vector3D dir=new Vector3D(light.getParallelLightDirection());dir.unit();
        Vector3D n=new Vector3D(normal);n.unit();
        int cosine=Vector3D.innerProduct(dir,n)/4096;
        return light.getAmbientIntensity()+(int)((long)light.getParallelLightIntensity()*Math.max(0,cosine)/4096);
    }
    public static int scene(FigureLayout layout,Light light) {
        Vector3D p=layout.getAffineTrans().transform(new Vector3D(3,4,5));
        int sx=(int)((long)p.x*layout.getScaleX()/512)+layout.getCenterX();
        int sy=(int)((long)p.y*layout.getScaleY()/512)+layout.getCenterY();
        return sx*10000+sy*100+shade(light,new Vector3D(0,0,4096));
    }
}
