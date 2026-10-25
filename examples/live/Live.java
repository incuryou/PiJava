import java.util.Enumeration;
import java.util.Vector;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Command;
import javax.microedition.lcdui.CommandListener;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Displayable;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.List;
import javax.microedition.midlet.MIDlet;

public class Live extends MIDlet {
    public void startApp() { Display.getDisplay(this).setCurrent(new LiveCanvas()); }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class LiveCanvas extends Canvas {
    boolean picked;
    String mark(boolean ok, String name) { return ok ? name : name + " NG"; }
    public void paint(Graphics g) {
        g.setColor(0x142033);
        g.fillRect(0, 0, getWidth(), getHeight());
        String hex = mark(Integer.parseInt("ff", 16) == 255, "HEX");
        String flt = mark(String.valueOf(1.5f).startsWith("1.5"), "FLT");
        String dbl = mark(String.valueOf(2.5).startsWith("2.5"), "DBL");
        String round = mark(Math.round(1.6f) == 2 && Math.round(-1.6f) == -2, "ROUND");
        String hash = mark("A".hashCode() == 65, "HASH");
        Vector list = new Vector();
        list.addElement("Q");
        Enumeration items = list.elements();
        String en = mark(items.hasMoreElements() && "Q".equals(items.nextElement()) && !items.hasMoreElements(), "ENUM");
        List menu = new List("L", List.IMPLICIT);
        menu.append("Go", null);
        menu.setCommandListener(new CommandListener() {
            public void commandAction(Command c, Displayable d) { picked = true; }
        });
        menu.keyPressed(8);
        String soft = mark(picked, "SOFT");
        Thread.yield();
        String yield = mark(Thread.currentThread().isAlive(), "YIELD");
        Object obj = new Object();
        boolean waitOk = false;
        synchronized (obj) {
            obj.notify(); obj.notifyAll();
            try { obj.wait(1); waitOk = Thread.holdsLock(obj); }
            catch (InterruptedException e) { }
        }
        String wait = mark(waitOk && !Thread.holdsLock(obj), "WAIT");
        g.setColor(0xF2F2F2);
        g.drawString("LIVE", 8, 12, 0);
        g.drawString(hex, 56, 12, 0);
        g.drawString(flt, 104, 12, 0);
        g.drawString(dbl, 152, 12, 0);
        g.drawString(round, 8, 28, 0);
        g.drawString(hash, 72, 28, 0);
        g.drawString(en, 128, 28, 0);
        g.drawString(soft, 8, 44, 0);
        g.drawString(yield, 64, 44, 0);
        g.drawString(wait, 128, 44, 0);
    }
}
