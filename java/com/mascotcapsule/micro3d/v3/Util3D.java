package com.mascotcapsule.micro3d.v3;

public class Util3D {
    private Util3D() {}
    public static final int sqrt(int p) {
        long magnitude = p < 0 ? -(long) p : p;
        return (int) Math.sqrt((double) magnitude);
    }
    public static final int sin(int p) { return sine(p, 1024); }
    public static final int cos(int p) { return sine(p + 1024, 1024); }
    static int sine(int p, int scale) {
        return (int) Math.floor(Math.sin((p & 4095) * (Math.PI / 2048.0)) * scale + 0.5);
    }
}
