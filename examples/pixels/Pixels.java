import com.nokia.mid.ui.DirectGraphics;
import com.nokia.mid.ui.DirectUtils;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.midlet.MIDlet;

public class Pixels extends MIDlet {
    public void startApp() { Display.getDisplay(this).setCurrent(new PixCanvas()); }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class PixCanvas extends Canvas {
    public void paint(Graphics g) {
        g.setColor(0x142033);
        g.fillRect(0, 0, getWidth(), getHeight());
        int[] px = new int[64];
        int i = 0;
        while (i < 64) { px[i] = 0xFFC6A15B; i = i + 1; }
        px[0] = 0x00000000;
        DirectGraphics dg = DirectUtils.getDirectGraphics(g);
        dg.drawPixels(px, true, 0, 8, 40, 80, 8, 8, 0, DirectGraphics.TYPE_INT_8888_ARGB);
        int[] back = new int[1];
        dg.getPixels(back, false, 0, 1, 41, 81, 1, 1, DirectGraphics.TYPE_INT_8888_ARGB);
        g.setColor(0xF2F2F2);
        g.drawString("PIXELS", 16, 24, 0);
        g.drawString(back[0] == 0xFFC6A15B ? "GOLD" : "NG", 16, 40, 0);
    }
}
