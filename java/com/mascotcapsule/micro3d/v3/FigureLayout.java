package com.mascotcapsule.micro3d.v3;

public class FigureLayout {
    private AffineTrans transform;
    private AffineTrans[] transforms;
    private int selected,scaleX,scaleY,centerX,centerY,parallelWidth,parallelHeight;
    // Package state for the future Graphics3D consumer; never native handles.
    int projectionMode,zNear,zFar,angle,perspectiveWidth,perspectiveHeight;
    public FigureLayout() { this(null,512,512,0,0); }
    public FigureLayout(AffineTrans trans,int sx,int sy,int cx,int cy) {
        setAffineTrans(trans);setScale(sx,sy);setCenter(cx,cy);
    }
    public final AffineTrans getAffineTrans() { return transforms==null?transform:transforms.length==0?null:transforms[selected]; }
    public final void setAffineTrans(AffineTrans trans) { transform=trans;transforms=null;selected=0; }
    public final void setAffineTrans(AffineTrans[] trans) {
        if(trans==null)throw new NullPointerException();
        for(int i=0;i<trans.length;i++)if(trans[i]==null)throw new NullPointerException();
        transforms=trans;transform=null;selected=0;
    }
    public final void setAffineTransArray(AffineTrans[] trans) { setAffineTrans(trans); }
    public final void selectAffineTrans(int idx) {
        if(idx<0 || idx>=(transforms==null?1:transforms.length))throw new IllegalArgumentException();
        selected=idx;
    }
    public final int getScaleX() { return scaleX; }
    public final int getScaleY() { return scaleY; }
    public final void setScale(int sx,int sy) { scaleX=sx;scaleY=sy;projectionMode=0; }
    public final int getCenterX() { return centerX; }
    public final int getCenterY() { return centerY; }
    public final void setCenter(int cx,int cy) { centerX=cx;centerY=cy; }
    public final int getParallelWidth() { return parallelWidth; }
    public final int getParallelHeight() { return parallelHeight; }
    public final void setParallelSize(int w,int h) {
        if(w<0 || h<0)throw new IllegalArgumentException();
        parallelWidth=w;parallelHeight=h;projectionMode=1;
    }
    private static void clips(int near,int far) {
        if(near<1 || near>=32767 || far<2 || far>=32768 || near>=far)throw new IllegalArgumentException();
    }
    public final void setPerspective(int near,int far,int viewAngle) {
        clips(near,far);if(viewAngle<=0 || viewAngle>=2048)throw new IllegalArgumentException();
        zNear=near;zFar=far;angle=viewAngle;projectionMode=2;
    }
    public final void setPerspective(int near,int far,int width,int height) {
        clips(near,far);if(width<0 || height<0)throw new IllegalArgumentException();
        zNear=near;zFar=far;perspectiveWidth=width;perspectiveHeight=height;projectionMode=3;
    }
}
