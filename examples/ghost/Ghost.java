import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;
import javax.microedition.lcdui.Display;
import javax.microedition.midlet.MIDlet;
import com.nokia.mid.ui.DirectGraphics;
import com.nokia.mid.ui.DirectUtils;

public class Ghost extends MIDlet {
    public void startApp() { Display.getDisplay(this).setCurrent(new GhostCanvas()); }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class GhostCanvas extends Canvas {
    public void paint(Graphics g) {
        g.setColor(0x142033);
        g.fillRect(0, 0, getWidth(), getHeight());
        int[] rgb = new int[64];
        int i = 0;
        while (i < 64) {
            int x = i - (i / 8) * 8;
            int y = i / 8;
            rgb[i] = (x >= 3 && x <= 4 && y >= 3 && y <= 4) ? 0x00FF00FF : 0xFFFF0000;
            i = i + 1;
        }
        g.drawImage(Image.createRGBImage(rgb, 8, 8, true), 16, 48, 0);
        try { g.drawImage(Image.createImage("/idx.png"), 40, 48, 0); }
        catch (java.io.IOException ignored) {}
        DirectGraphics dg = DirectUtils.getDirectGraphics(g);
        int[] hole = new int[1];
        int[] edge = new int[1];
        int[] png = new int[1];
        dg.getPixels(hole, false, 0, 1, 19, 51, 1, 1, 0);
        dg.getPixels(edge, false, 0, 1, 16, 48, 1, 1, 0);
        dg.getPixels(png, false, 0, 1, 43, 51, 1, 1, 0);
        int holeRgb = hole[0] & 0xffffff;
        int edgeRed = (edge[0] >> 16) & 255;
        int pngRgb = png[0] & 0xffffff;
        g.setColor(0xF2F2F2);
        g.drawString("GHOST", 16, 16, 0);
        g.drawString(holeRgb == 0x142033 && edgeRed > 200 ? "HOLE" : "NG", 16, 28, 0);
        g.drawString(pngRgb == 0x142033 ? "PNG" : "PNG NG", 16, 40, 0);
    }
}
