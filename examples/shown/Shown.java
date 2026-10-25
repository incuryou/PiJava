import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Ticker;
import javax.microedition.midlet.MIDlet;

public class Shown extends MIDlet {
    public void startApp() { Display.getDisplay(this).setCurrent(new ShownCanvas()); }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class ShownCanvas extends Canvas {
    public void paint(Graphics g) {
        setTitle("T");
        setTicker(new Ticker("x"));
        g.setColor(0x142033);
        g.fillRect(0, 0, getWidth(), getHeight());
        g.setColor(0xF2F2F2);
        g.drawString(isShown() && "T".equals(getTitle()) && getTicker() != null ? "SHOWN" : "SHOWN NG", 8, 16, 0);
    }
}
