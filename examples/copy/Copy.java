import java.util.Vector;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.midlet.MIDlet;

public class Copy extends MIDlet {
    public void startApp() { Display.getDisplay(this).setCurrent(new CopyCanvas()); }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class CopyCanvas extends Canvas {
    public void paint(Graphics g) {
        Vector v = new Vector();
        v.addElement("A");
        v.addElement("B");
        Object[] box = new Object[2];
        v.copyInto(box);
        boolean ok = box[0] == "A" || (box[0] != null && box[0].equals("A"));
        ok = ok && v.toArray().length == 2 && v.capacity() >= 2;
        g.setColor(0x142033);
        g.fillRect(0, 0, getWidth(), getHeight());
        g.setColor(0xF2F2F2);
        g.drawString(ok ? "COPY" : "COPY NG", 8, 16, 0);
    }
}
