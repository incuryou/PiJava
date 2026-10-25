import java.io.IOException;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;
import javax.microedition.midlet.MIDlet;

public class Miss extends MIDlet {
    public void startApp() { Display.getDisplay(this).setCurrent(new MissCanvas()); }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class MissCanvas extends Canvas {
    public void paint(Graphics g) {
        String token = "MISS NG";
        try {
            Image.createImage("/no-such.png");
        } catch (IOException e) {
            token = "MISS";
        }
        g.setColor(0x142033);
        g.fillRect(0, 0, getWidth(), getHeight());
        g.setColor(0xF2F2F2);
        g.drawString(token, 8, 16, 0);
    }
}
