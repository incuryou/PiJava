package com.mascotcapsule.micro3d.v3;

public class Vector3D {
    public int x, y, z;
    public Vector3D() {}
    public Vector3D(int x, int y, int z) { set(x, y, z); }
    public Vector3D(Vector3D v) { set(v); }
    public final int getX() { return x; }
    public final int getY() { return y; }
    public final int getZ() { return z; }
    public final void setX(int x) { this.x = x; }
    public final void setY(int y) { this.y = y; }
    public final void setZ(int z) { this.z = z; }
    public final void set(int x, int y, int z) { this.x=x; this.y=y; this.z=z; }
    public final void set(Vector3D v) { set(v.x, v.y, v.z); }
    public final void unit() {
        double length = Math.sqrt((double)x*x + (double)y*y + (double)z*z);
        if (length == 0) return;
        set((int)(x * 4096.0 / length), (int)(y * 4096.0 / length), (int)(z * 4096.0 / length));
    }
    public final int innerProduct(Vector3D v) { return innerProduct(this, v); }
    public static final int innerProduct(Vector3D a, Vector3D b) {
        return a.x*b.x + a.y*b.y + a.z*b.z;
    }
    public final void outerProduct(Vector3D v) {
        int nx=y*v.z-z*v.y, ny=z*v.x-x*v.z, nz=x*v.y-y*v.x;
        set(nx,ny,nz);
    }
    public static final Vector3D outerProduct(Vector3D a, Vector3D b) {
        return new Vector3D(a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x);
    }
}
