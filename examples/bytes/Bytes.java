import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Display;
import javax.microedition.midlet.MIDlet;
import com.nokia.mid.ui.DirectGraphics;
import com.nokia.mid.ui.DirectUtils;

public class Bytes extends MIDlet {
    public void startApp() { Display.getDisplay(this).setCurrent(new BytesCanvas()); }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class BytesCanvas extends Canvas {
    public void paint(Graphics g) {
        g.setColor(0x142033);
        g.fillRect(0, 0, getWidth(), getHeight());
        byte[] px = new byte[64];
        int i = 0;
        while (i < 64) { px[i] = (byte) 0xE0; i = i + 1; }
        DirectGraphics dg = DirectUtils.getDirectGraphics(g);
        dg.drawPixels(px, null, 0, 8, 16, 48, 8, 8, 0, 332);
        int[] back = new int[1];
        dg.getPixels(back, false, 0, 1, 17, 49, 1, 1, 0);
        int red = (back[0] >> 16) & 255;
        dg.fillTriangle(16, 80, 88, 80, 52, 130, 0xFFC6A15B);
        g.setColor(0xF2F2F2);
        g.drawString("BYTES", 16, 24, 0);
        g.drawString(red > 200 ? "RED" : "NG", 16, 40, 0);
    }
}
