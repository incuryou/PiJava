import java.util.Timer;
import java.util.TimerTask;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.midlet.MIDlet;

public class Tick extends MIDlet {
    public void startApp() { Display.getDisplay(this).setCurrent(new TickCanvas()); }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class TickCanvas extends Canvas {
    int n;
    public TickCanvas() {
        final TickCanvas self = this;
        Timer timer = new Timer();
        timer.schedule(new TimerTask() {
            public void run() { self.bump(); }
        }, 0, 50);
    }
    void bump() { n = n + 1; repaint(); }
    public void paint(Graphics g) {
        g.setColor(0x142033);
        g.fillRect(0, 0, getWidth(), getHeight());
        g.setColor(0xF2F2F2);
        g.drawString("TICK", 16, 16, 0);
        g.drawString(n >= 2 ? "N 2" : "WAIT", 16, 28, 0);
    }
}
