package com.nokia.mid.ui;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;
public class DirectUtils {
    public static DirectGraphics getDirectGraphics(Graphics g) { return new DirectGraphics(g); }
    public static native Image createImage(int width,int height,int argb);
    public static native Image createImage(byte[] data,int offset,int length);
    public static Image getImage(byte[] data, int offset, int length) { return Image.createImage(data, offset, length); }
}
