package com.mascotcapsule.micro3d.v3;

public class Light {
    private Vector3D direction;
    private int parallel, ambient;
    public Light() { this(new Vector3D(0,0,4096),4096,0); }
    public Light(Vector3D dir, int dirIntensity, int ambIntensity) {
        setParallelLightDirection(dir); parallel=dirIntensity; ambient=ambIntensity;
    }
    public final Vector3D getParallelLightDirection() { return direction; }
    public final void setParallelLightDirection(Vector3D v) {
        if (v == null) throw new NullPointerException();
        // The API does not specify a defensive copy; retain an ordinary Java reference.
        direction=v;
    }
    public final int getParallelLightIntensity() { return parallel; }
    public final int getAmbientIntensity() { return ambient; }
    public final void setParallelLightIntensity(int p) { parallel=p; }
    public final void setAmbientIntensity(int p) { ambient=p; }
    public final Vector3D getDirection() { return getParallelLightDirection(); }
    public final void setDirection(Vector3D v) { setParallelLightDirection(v); }
    public final int getDirIntensity() { return getParallelLightIntensity(); }
    public final int getAmbIntensity() { return getAmbientIntensity(); }
    public final void setDirIntensity(int p) { setParallelLightIntensity(p); }
    public final void setAmbIntensity(int p) { setAmbientIntensity(p); }
}
