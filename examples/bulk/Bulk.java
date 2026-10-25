import java.io.ByteArrayOutputStream;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.midlet.MIDlet;

public class Bulk extends MIDlet {
    public void startApp() { Display.getDisplay(this).setCurrent(new BulkCanvas()); }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class BulkCanvas extends Canvas {
    public void paint(Graphics g) {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        byte[] chunk = new byte[] { 65, 66, 67, 68 };
        out.write(chunk, 1, 2);
        byte[] got = out.toByteArray();
        boolean ok = out.size() == 2 && got.length == 2 && got[0] == 66 && got[1] == 67;
        g.setColor(0x142033);
        g.fillRect(0, 0, getWidth(), getHeight());
        g.setColor(0xF2F2F2);
        g.drawString(ok ? "BULK" : "BULK NG", 8, 16, 0);
    }
}
