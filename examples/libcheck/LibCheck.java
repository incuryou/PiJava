import java.util.Hashtable;
import java.util.Vector;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.midlet.MIDlet;

public class LibCheck extends MIDlet {
    public void startApp() { Display.getDisplay(this).setCurrent(new C()); }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}
class C extends Canvas {
    String status;
    public C() {
        String s = "ok";
        try {
            Vector v = new Vector();
            v.addElement("A"); v.addElement("B");
            if (v.size() != 2) s = "vsize";
            Hashtable h = new Hashtable();
            h.put("k", "v");
            if (!"v".equals(h.get("k"))) s = "hash";
            int x = 2;
            switch (x) { case 1: s = "sw"; break; case 2: break; default: s = "def"; }
            int[][] m = new int[2][3];
            m[1][2] = 7;
            if (m[1][2] != 7) s = "arr";
            if (!"  Hi ".trim().equals("Hi")) s = "trim";
            if (!"abcd".substring(1, 3).equals("bc")) s = "sub";
            if (Math.abs(-4) != 4) s = "abs";
        } catch (RuntimeException e) { s = "boom"; }
        try { throw new IllegalArgumentException("nope"); }
        catch (IllegalArgumentException e) { if (e.getMessage() == null) s = "msg"; }
        catch (Exception e) { s = "wrong"; }
        status = s;
    }
    protected void paint(Graphics g) {
        g.setColor(0x142033); g.fillRect(0, 0, 240, 320);
        g.setColor(0xF4E2B0); g.drawString(status, 20, 40, 0);
    }
}
