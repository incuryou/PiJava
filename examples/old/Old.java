import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.midlet.MIDlet;

public class Old extends MIDlet {
    public void startApp() { Display.getDisplay(this).setCurrent(new OldCanvas()); }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class OldCanvas extends Canvas {
    public void paint(Graphics g) {
        g.setColor(0x142033);
        g.fillRect(0, 0, getWidth(), getHeight());
        char[] buf = new char[2];
        "OK".getChars(0, 2, buf, 0);
        String got = "";
        got = got + buf[0];
        got = got + buf[1];
        g.setColor(0xF2F2F2);
        g.drawString("OLD", 16, 16, 0);
        g.drawString(Jsr.value() == 1 ? "JSR" : "JSR NG", 16, 32, 0);
        g.drawChar('A', 56, 32, 0);
        g.drawString(got.equals("OK") ? "GET" : "GET NG", 80, 32, 0);
    }
}
