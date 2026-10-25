package com.nokia.mid.ui;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;
public class DirectGraphics {
    public static final int FLIP_HORIZONTAL = 0x2000;
    public static final int FLIP_VERTICAL = 0x4000;
    public static final int ROTATE_90 = 90;
    public static final int ROTATE_180 = 180;
    public static final int ROTATE_270 = 270;
    public static final int TYPE_USHORT_4444_ARGB = 4444;
    public static final int TYPE_USHORT_1555_ARGB = 1555;
    public static final int TYPE_INT_8888_ARGB = 8888;
    public static final int TYPE_BYTE_1_GRAY=1, TYPE_BYTE_1_GRAY_VERTICAL=-1, TYPE_BYTE_2_GRAY=2, TYPE_BYTE_4_GRAY=4, TYPE_BYTE_8_GRAY=8, TYPE_BYTE_332_RGB=332;
    public static final int TYPE_USHORT_444_RGB=444, TYPE_USHORT_555_RGB=555, TYPE_USHORT_565_RGB=565, TYPE_INT_888_RGB=888;
    Graphics g;
    public native void drawImage(Image image,int x,int y,int anchor,int manipulation);
    public native void getPixels(int[] pixels,int offset,int scanlength,int x,int y,int width,int height,int format);
    public DirectGraphics(Graphics g) { if(g==null)throw new NullPointerException();this.g = g; }
    public native void drawPixels(int[] pixels, boolean transparency, int offset, int scanlength, int x, int y, int width, int height, int manipulation, int format);
    private static int byteBits(int format) {
        if(format==-1||format==1)return 1;
        if(format==2||format==4||format==8)return format;
        if(format==332)return 8;
        throw new IllegalArgumentException("byte pixel format");
    }
    private static int pixelCount(int width,int height) {
        if(width<0||height<0)throw new IllegalArgumentException("pixel dimensions");
        long count=((long)width)*height;
        if(count>16777216L)throw new OutOfMemoryError("pixel conversion too large");
        return (int)count;
    }
    private static long location(int offset,int scan,int row,int col,int format,int bits) {
        if(format==-1) {
            if(scan<=0)throw new IllegalArgumentException("vertical scanlength");
            if(offset<0)throw new ArrayIndexOutOfBoundsException();
            long yy=((long)offset)/scan+row;
            long xx=offset%scan+(long)col;
            return ((yy/8)*scan+xx)*8+(yy%8);
        }
        return (((long)offset)+((long)row)*scan+col)*bits;
    }
    private static void byteRange(byte[] pixels,int offset,int scan,int w,int h,int format,int bits) {
        if(pixels==null)throw new NullPointerException();
        if(w==0||h==0)return;
        // Validate every row before writes: negative stride and vertical packing
        // are deliberately not treated as contiguous byte offsets.
        for(int y=0;y<h;y++) {
            long first=location(offset,scan,y,0,format,bits);
            long last=location(offset,scan,y,w-1,format,bits);
            if(first<0||last<0||first/8>=pixels.length||last/8>=pixels.length)
                throw new ArrayIndexOutOfBoundsException();
        }
    }
    private static int packed(byte[] data,long at,int bits,boolean vertical) {
        int shift=vertical?(int)(at%8):8-bits-(int)(at%8);
        return ((data[(int)(at/8)]&255)>>>shift)&((1<<bits)-1);
    }
    private static void putPacked(byte[] data,long at,int bits,boolean vertical,int value) {
        int shift=vertical?(int)(at%8):8-bits-(int)(at%8);
        int mask=((1<<bits)-1)<<shift;int i=(int)(at/8);
        data[i]=(byte)((data[i]&~mask)|((value<<shift)&mask));
    }
    public void drawPixels(byte[] pixels,byte[] mask,int offset,int scan,int x,int y,int width,int height,int manipulation,int format) {
        int bits=byteBits(format),count=pixelCount(width,height);
        byteRange(pixels,offset,scan,width,height,format,bits);
        if(mask!=null)byteRange(mask,offset,scan,width,height,format,bits);
        int[] argb=new int[count];int maximum=(1<<bits)-1;
        for(int row=0;row<height;row++)for(int col=0;col<width;col++) {
            long at=location(offset,scan,row,col,format,bits);
            int p=packed(pixels,at,bits,format==-1);
            int a=mask==null?255:packed(mask,at,bits,format==-1)*255/maximum;
            int rgb;
            if(format==332)rgb=(((p>>>5)*255/7)<<16)|((((p>>>2)&7)*255/7)<<8)|((p&3)*255/3);
            else {int g=p*255/maximum;rgb=(g<<16)|(g<<8)|g;}
            argb[row*width+col]=(a<<24)|rgb;
        }
        drawPixels(argb,true,0,width,x,y,width,height,manipulation,TYPE_INT_8888_ARGB);
    }
    public void getPixels(byte[] pixels,byte[] mask,int offset,int scan,int x,int y,int width,int height,int format) {
        int bits=byteBits(format),count=pixelCount(width,height);
        byteRange(pixels,offset,scan,width,height,format,bits);
        if(mask!=null)byteRange(mask,offset,scan,width,height,format,bits);
        int[] argb=new int[count];getPixels(argb,0,width,x,y,width,height,TYPE_INT_8888_ARGB);
        int maximum=(1<<bits)-1;
        for(int row=0;row<height;row++)for(int col=0;col<width;col++) {
            int p=argb[row*width+col],r=(p>>>16)&255,g=(p>>>8)&255,b=p&255;
            int c=format==332?((r*7/255)<<5)|((g*7/255)<<2)|(b*3/255):((r*299+g*587+b*114)*maximum+127500)/255000;
            long at=location(offset,scan,row,col,format,bits);
            putPacked(pixels,at,bits,format==-1,c);
            if(mask!=null)putPacked(mask,at,bits,format==-1,((p>>>24)*maximum+127)/255);
        }
    }
    private static void shortRange(short[] pixels,int offset,int scan,int width,int height) {
        if(pixels==null)throw new NullPointerException();
        if(width==0||height==0)return;
        long a=offset,b=((long)offset)+(height-1L)*scan;
        if(a>b){long t=a;a=b;b=t;}
        if(a<0||b+width>pixels.length)throw new ArrayIndexOutOfBoundsException();
    }
    private static void shortFormat(int format) {
        if(format!=4444&&format!=0x4444&&format!=1555&&format!=0x5515&&format!=444&&format!=555&&format!=565)
            throw new IllegalArgumentException("short pixel format");
    }
    public void getPixels(short[] pixels,int offset,int scan,int x,int y,int width,int height,int format) {
        int count=pixelCount(width,height);shortFormat(format);shortRange(pixels,offset,scan,width,height);
        int[] argb=new int[count];getPixels(argb,0,width,x,y,width,height,TYPE_INT_8888_ARGB);
        for(int row=0;row<height;row++)for(int col=0;col<width;col++) {
            int p=argb[row*width+col],a=p>>>24,r=(p>>>16)&255,g=(p>>>8)&255,b=p&255,c;
            if(format==4444||format==0x4444)c=((a>>>4)<<12)|((r>>>4)<<8)|((g>>>4)<<4)|(b>>>4);
            else if(format==444)c=((r>>>4)<<8)|((g>>>4)<<4)|(b>>>4);
            else if(format==565)c=((r>>>3)<<11)|((g>>>2)<<5)|(b>>>3);
            else {c=((r>>>3)<<10)|((g>>>3)<<5)|(b>>>3);if(format!=555)c|=(a>>>7)<<15;}
            pixels[(int)(((long)offset)+((long)row)*scan+col)]=(short)c;
        }
    }
    public void drawPixels(short[] pixels, boolean transparency, int offset, int scanlength, int x, int y, int width, int height, int manipulation, int format) {
        int count=pixelCount(width,height);shortFormat(format);shortRange(pixels,offset,scanlength,width,height);
        int[] argb = new int[count];
        int i = 0;
        while (i < width * height) {
            int row = i / width;
            int col = i - row * width;
            int src = offset + row * scanlength + col;
            int p = pixels[src] & 65535;
            int a = 255, r, gch, bl;
            if (format == 4444 || format == 0x4444) {
                a = ((p >> 12) & 15) * 17;
                r = ((p >> 8) & 15) * 17;
                gch = ((p >> 4) & 15) * 17;
                bl = (p & 15) * 17;
            } else if(format==1555||format==0x5515) {
                a=(p&32768)==0?0:255;r=((p>>10)&31)*255/31;gch=((p>>5)&31)*255/31;bl=(p&31)*255/31;
            } else if(format==444) {
                r=((p>>8)&15)*17;gch=((p>>4)&15)*17;bl=(p&15)*17;
            } else if(format==555) {
                r=((p>>10)&31)*255/31;gch=((p>>5)&31)*255/31;bl=(p&31)*255/31;
            } else if(format==565) {
                r = ((p >> 11) & 31) * 255 / 31;
                gch = ((p >> 5) & 63) * 255 / 63;
                bl = (p & 31) * 255 / 31;
            } else throw new IllegalArgumentException("short pixel format");
            if (!transparency) a = 255;
            argb[i] = (a << 24) | (r << 16) | (gch << 8) | bl;
            i = i + 1;
        }
        drawPixels(argb, transparency, 0, width, x, y, width, height, manipulation, TYPE_INT_8888_ARGB);
    }
    public native void fillPolygon(int[] xPoints,int xOffset,int[] yPoints,int yOffset,int count,int argbColor);
    public native void drawPolygon(int[] xPoints,int xOffset,int[] yPoints,int yOffset,int count,int argbColor);
    public native void fillTriangle(int x1,int y1,int x2,int y2,int x3,int y3,int argbColor);
    public native void getPixels(int[] pixels, boolean transparency, int offset, int scanlength, int x, int y, int width, int height, int format);
    public native void drawTriangle(int x1,int y1,int x2,int y2,int x3,int y3,int argbColor);
    public native void setARGBColor(int argb);
    public int getNativePixelFormat() { return TYPE_INT_8888_ARGB; }
    public native int getAlphaComponent();
}
