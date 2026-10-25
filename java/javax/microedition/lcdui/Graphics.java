package javax.microedition.lcdui;
public class Graphics {
    public static final int HCENTER = 1;
    public static final int VCENTER = 2;
    public static final int LEFT = 4;
    public static final int RIGHT = 8;
    public static final int TOP = 16;
    public static final int BOTTOM = 32;
    public static final int BASELINE = 64;
    public native void copyArea(int x, int y, int width, int height, int destX, int destY, int anchor);
    public int getDisplayColor(int color) { return color & 0xffffff; }
    public int getRedComponent() { return (getColor()>>16)&255; }
    public int getGreenComponent() { return (getColor()>>8)&255; }
    public int getBlueComponent() { return getColor()&255; }
    public int getGrayScale() { return (getRedComponent()*76+getGreenComponent()*150+getBlueComponent()*29)/255; }
    public void setGrayScale(int value) {
        if(value<0||value>255)throw new IllegalArgumentException();
        setColor(value,value,value);
    }
    public native void setColor(int rgb);
    public native void setColor(int r, int g, int b);
    public native void fillRect(int x, int y, int width, int height);
    public native void drawRect(int x, int y, int width, int height);
    public native void drawLine(int x1, int y1, int x2, int y2);
    public native void drawString(String str, int x, int y, int anchor);
    public native void fillArc(int x, int y, int width, int height, int startAngle, int arcAngle);
    public native void drawRegion(Image src, int x_src, int y_src, int width, int height, int transform, int x_dest, int y_dest, int anchor);
    public native void drawImage(Image img, int x, int y, int anchor);
    public native void setClip(int x, int y, int width, int height);
    public native void clipRect(int x, int y, int width, int height);
    public native int getClipX();
    public native int getClipY();
    public native int getClipWidth();
    public native int getClipHeight();
    public native void translate(int x, int y);
    public native int getTranslateX();
    public native int getTranslateY();
    public native int getColor();
    public native void setFont(Font font);
    public native Font getFont();
    public void drawSubstring(String str, int offset, int len, int x, int y, int anchor) {
        if (str == null) throw new NullPointerException();
        if (offset < 0 || len < 0 || offset > str.length() - len) throw new StringIndexOutOfBoundsException();
        drawString(str.substring(offset, offset + len), x, y, anchor);
    }
    public void drawChar(char ch, int x, int y, int anchor) {
        drawString(String.valueOf(ch), x, y, anchor);
    }
    public void drawChars(char[] data, int offset, int length, int x, int y, int anchor) {
        if (data == null) throw new NullPointerException();
        if (offset < 0 || length < 0 || offset > data.length - length) throw new ArrayIndexOutOfBoundsException();
        StringBuilder sb = new StringBuilder();
        int i = 0;
        while (i < length) { sb.append(data[offset + i]); i = i + 1; }
        drawString(sb.toString(), x, y, anchor);
    }
    public native void drawRoundRect(int x, int y, int width, int height, int arcWidth, int arcHeight);
    public native void fillRoundRect(int x, int y, int width, int height, int arcWidth, int arcHeight);
    public native void drawRGB(int[] rgbData, int offset, int scanlength, int x, int y, int width, int height, boolean processAlpha);
    public static final int SOLID = 0;
    public static final int DOTTED = 1;
    private int strokeStyle;
    public void setStrokeStyle(int style) {
        if (style != SOLID && style != DOTTED) throw new IllegalArgumentException();
        strokeStyle = style;
    }
    public int getStrokeStyle() { return strokeStyle; }
    public native void drawArc(int x, int y, int width, int height, int startAngle, int arcAngle);
    public native void fillTriangle(int x1, int y1, int x2, int y2, int x3, int y3);
}
