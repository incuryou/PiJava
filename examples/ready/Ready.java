import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.midlet.MIDlet;

public class Ready extends MIDlet {
    public void startApp() { Display.getDisplay(this).setCurrent(new ReadyCanvas()); }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class ReadyCanvas extends Canvas {
    public void paint(Graphics g) {
        g.setColor(0x142033);
        g.fillRect(0, 0, getWidth(), getHeight());
        setFullScreenMode(true);
        float x = 1.5f;
        x = x + 2.5f;
        int n = (int) x;
        double d = 1.25;
        d = d + 1.25;
        String sms = "SMS NG";
        try { Class.forName("javax.wireless.messaging.TextMessage"); sms = "SMS"; }
        catch (Exception e) { sms = "SMS NG"; }
        g.setColor(0xF2F2F2);
        g.drawString("READY", 16, 16, 0);
        g.drawString(n == 4 ? "F4" : "FNG", 16, 32, 0);
        g.drawString(d > 2.0 ? "D2" : "DNG", 56, 32, 0);
        g.drawString(sms, 96, 32, 0);
        g.drawString("FULL", 16, 48, 0);
    }
}
