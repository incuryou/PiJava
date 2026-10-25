import com.nokia.mid.ui.DirectGraphics;
import com.nokia.mid.ui.DirectUtils;
import com.nokia.mid.ui.FullCanvas;
import java.util.Calendar;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.midlet.MIDlet;

public class Nokia extends MIDlet {
    public void startApp() {
        Display.getDisplay(this).setCurrent(new NokiaCanvas());
    }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class NokiaCanvas extends FullCanvas {
    int x;
    public NokiaCanvas() { x = 100; }
    public void paint(Graphics g) {
        DirectGraphics dg = DirectUtils.getDirectGraphics(g);
        dg.setARGBColor(0xff000000);
        int year = Calendar.getInstance().get(Calendar.YEAR);
        g.setColor(0x142033);
        g.fillRect(0, 0, getWidth(), getHeight());
        g.setColor(0xC6A15B);
        g.fillRect(x, 120, 16, 16);
        g.setColor(0xF2F2F2);
        g.drawString("NOKIA", 16, 24, 0);
        g.drawString("X " + x, 16, 48, 0);
        g.drawString("Y " + year, 16, 68, 0);
    }
    public void keyPressed(int code) {
        if (code == KEY_LEFT) x = x - 8;
        if (code == KEY_RIGHT) x = x + 8;
        if (code == KEY_UP) x = x - 1;
        if (code == KEY_DOWN) x = x + 1;
    }
}
