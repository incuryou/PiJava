package com.mascotcapsule.micro3d.v3;

public class AffineTrans {
    public int m00,m01,m02,m03,m10,m11,m12,m13,m20,m21,m22,m23;
    public AffineTrans() {}
    public AffineTrans(AffineTrans a) { set(a); }
    public AffineTrans(int[] a) { set(a); }
    public AffineTrans(int[] a,int offset) { set(a,offset); }
    public AffineTrans(int[][] a) { set(a); }
    public AffineTrans(int a,int b,int c,int d,int e,int f,int g,int h,int i,int j,int k,int l) { set(a,b,c,d,e,f,g,h,i,j,k,l); }
    public final void set(int a,int b,int c,int d,int e,int f,int g,int h,int i,int j,int k,int l) {
        m00=a;m01=b;m02=c;m03=d;m10=e;m11=f;m12=g;m13=h;m20=i;m21=j;m22=k;m23=l;
    }
    public final void set(AffineTrans a) { set(a.m00,a.m01,a.m02,a.m03,a.m10,a.m11,a.m12,a.m13,a.m20,a.m21,a.m22,a.m23); }
    private static void range(int[] a,int offset) {
        int length=a.length;
        if (offset<0 || offset>length-12) throw new IllegalArgumentException();
    }
    public final void set(int[] a) { set(a,0); }
    public final void set(int[] a,int offset) {
        range(a,offset);
        set(a[offset],a[offset+1],a[offset+2],a[offset+3],a[offset+4],a[offset+5],a[offset+6],a[offset+7],a[offset+8],a[offset+9],a[offset+10],a[offset+11]);
    }
    public final void set(int[][] a) {
        if(a.length<3)throw new IllegalArgumentException();
        for(int i=0;i<3;i++)if(a[i]==null)throw new NullPointerException();
        for(int i=0;i<3;i++)if(a[i].length<4)throw new IllegalArgumentException();
        set(a[0][0],a[0][1],a[0][2],a[0][3],a[1][0],a[1][1],a[1][2],a[1][3],a[2][0],a[2][1],a[2][2],a[2][3]);
    }
    public final void get(int[] a) { get(a,0); }
    public final void get(int[] a,int offset) {
        range(a,offset);
        a[offset]=m00;a[offset+1]=m01;a[offset+2]=m02;a[offset+3]=m03;
        a[offset+4]=m10;a[offset+5]=m11;a[offset+6]=m12;a[offset+7]=m13;
        a[offset+8]=m20;a[offset+9]=m21;a[offset+10]=m22;a[offset+11]=m23;
    }
    public final void setIdentity() { set(4096,0,0,0,0,4096,0,0,0,0,4096,0); }
    private static int dot(int a,int b,int c,int x,int y,int z) {
        return (int)(((long)a*x+(long)b*y+(long)c*z)>>12);
    }
    public final Vector3D transform(Vector3D v) {
        return new Vector3D(dot(m00,m01,m02,v.x,v.y,v.z)+m03,
                            dot(m10,m11,m12,v.x,v.y,v.z)+m13,
                            dot(m20,m21,m22,v.x,v.y,v.z)+m23);
    }
    public final Vector3D transPoint(Vector3D v) { return transform(v); }
    public final void mul(AffineTrans a) { mul(this,a); }
    public final void mul(AffineTrans a,AffineTrans b) {
        // Evaluate every argument before set(), including when this aliases a or b.
        set(dot(a.m00,a.m01,a.m02,b.m00,b.m10,b.m20),dot(a.m00,a.m01,a.m02,b.m01,b.m11,b.m21),dot(a.m00,a.m01,a.m02,b.m02,b.m12,b.m22),dot(a.m00,a.m01,a.m02,b.m03,b.m13,b.m23)+a.m03,
            dot(a.m10,a.m11,a.m12,b.m00,b.m10,b.m20),dot(a.m10,a.m11,a.m12,b.m01,b.m11,b.m21),dot(a.m10,a.m11,a.m12,b.m02,b.m12,b.m22),dot(a.m10,a.m11,a.m12,b.m03,b.m13,b.m23)+a.m13,
            dot(a.m20,a.m21,a.m22,b.m00,b.m10,b.m20),dot(a.m20,a.m21,a.m22,b.m01,b.m11,b.m21),dot(a.m20,a.m21,a.m22,b.m02,b.m12,b.m22),dot(a.m20,a.m21,a.m22,b.m03,b.m13,b.m23)+a.m23);
    }
    public final void multiply(AffineTrans a) { mul(a); }
    public final void multiply(AffineTrans a,AffineTrans b) { mul(a,b); }
    // Internal renderer fp12 arithmetic adapted from MascotME (MIT),
    // Copyright (c) 2026 Roman Lahin and Yury Kharchenko. See licenses/mascotme.txt.
    // Keep its rounded arithmetic separate from the existing public mul contract.
    final void mulA2(AffineTrans a,AffineTrans b) {
        set(roundedDot(a.m00,a.m01,a.m02,b.m00,b.m10,b.m20),
            roundedDot(a.m00,a.m01,a.m02,b.m01,b.m11,b.m21),
            roundedDot(a.m00,a.m01,a.m02,b.m02,b.m12,b.m22),
            roundedDot(a.m00,a.m01,a.m02,b.m03,b.m13,b.m23)+a.m03,
            roundedDot(a.m10,a.m11,a.m12,b.m00,b.m10,b.m20),
            roundedDot(a.m10,a.m11,a.m12,b.m01,b.m11,b.m21),
            roundedDot(a.m10,a.m11,a.m12,b.m02,b.m12,b.m22),
            roundedDot(a.m10,a.m11,a.m12,b.m03,b.m13,b.m23)+a.m13,
            roundedDot(a.m20,a.m21,a.m22,b.m00,b.m10,b.m20),
            roundedDot(a.m20,a.m21,a.m22,b.m01,b.m11,b.m21),
            roundedDot(a.m20,a.m21,a.m22,b.m02,b.m12,b.m22),
            roundedDot(a.m20,a.m21,a.m22,b.m03,b.m13,b.m23)+a.m23);
    }
    private static int roundedDot(int a,int b,int c,int x,int y,int z) {
        return (int)(((long)a*x+(long)b*y+(long)c*z+2048)>>12);
    }
    final void scale(Vector3D v) {
        m00=(int)(((long)m00*v.x+2048)>>12);m01=(int)(((long)m01*v.y+2048)>>12);m02=(int)(((long)m02*v.z+2048)>>12);
        m10=(int)(((long)m10*v.x+2048)>>12);m11=(int)(((long)m11*v.y+2048)>>12);m12=(int)(((long)m12*v.z+2048)>>12);
        m20=(int)(((long)m20*v.x+2048)>>12);m21=(int)(((long)m21*v.y+2048)>>12);m22=(int)(((long)m22*v.z+2048)>>12);
    }
    final boolean isIdentity() {
        return m00==4096&&m01==0&&m02==0&&m03==0&&m10==0&&m11==4096&&m12==0&&m13==0&&m20==0&&m21==0&&m22==4096&&m23==0;
    }
    public final void rotationX(int r) {
        int c=Util3D.sine(r+1024,4096),s=Util3D.sine(r,4096);
        set(4096,0,0,m03,0,c,-s,m13,0,s,c,m23);
    }
    public final void rotationY(int r) {
        int c=Util3D.sine(r+1024,4096),s=Util3D.sine(r,4096);
        set(c,0,s,m03,0,4096,0,m13,-s,0,c,m23);
    }
    public final void rotationZ(int r) {
        int c=Util3D.sine(r+1024,4096),s=Util3D.sine(r,4096);
        set(c,-s,0,m03,s,c,0,m13,0,0,4096,m23);
    }
    public final void setRotationX(int r) { rotationX(r); }
    public final void setRotationY(int r) { rotationY(r); }
    public final void setRotationZ(int r) { rotationZ(r); }
    private static int fixed(double value) { return (int)Math.floor(value*4096.0+0.5); }
    public final void setRotation(Vector3D v,int r) {
        double length=Math.sqrt((double)v.x*v.x+(double)v.y*v.y+(double)v.z*v.z);
        if(length==0)throw new IllegalArgumentException();
        double x=v.x/length,y=v.y/length,z=v.z/length;
        double c=Util3D.sine(r+1024,4096)/4096.0,s=Util3D.sine(r,4096)/4096.0,t=1-c;
        set(fixed(t*x*x+c),fixed(t*x*y-s*z),fixed(t*x*z+s*y),m03,
            fixed(t*x*y+s*z),fixed(t*y*y+c),fixed(t*y*z-s*x),m13,
            fixed(t*x*z-s*y),fixed(t*y*z+s*x),fixed(t*z*z+c),m23);
    }
    public final void rotationV(Vector3D v,int r) { setRotation(v,r); }
    public final void lookAt(Vector3D pos,Vector3D look,Vector3D up) {
        // Native v3 callers supply a line-of-sight direction (often normalized
        // to4096), independently of the camera position. This is verified by
        // actual game call sites; the archived English "reference point" label
        // is ambiguous. Screen Y points down, so right = forward cross up.
        if(pos==null||look==null||up==null)throw new NullPointerException();
        Vector3D forward=new Vector3D(look);
        if((forward.x|forward.y|forward.z)==0)throw new IllegalArgumentException();
        forward.unit();
        Vector3D right=Vector3D.outerProduct(forward,up);
        if((right.x|right.y|right.z)==0)throw new IllegalArgumentException();
        right.unit();Vector3D vertical=Vector3D.outerProduct(forward,right);vertical.unit();
        set(right.x,right.y,right.z,-dot(right.x,right.y,right.z,pos.x,pos.y,pos.z),
            vertical.x,vertical.y,vertical.z,-dot(vertical.x,vertical.y,vertical.z,pos.x,pos.y,pos.z),
            forward.x,forward.y,forward.z,-dot(forward.x,forward.y,forward.z,pos.x,pos.y,pos.z));
    }
    public final void setViewTrans(Vector3D pos,Vector3D look,Vector3D up) { lookAt(pos,look,up); }
}
