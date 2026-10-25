import java.util.Random;
import java.util.Vector;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.midlet.MIDlet;

public class Play extends MIDlet {
    public static MIDlet me;
    public void startApp() {
        me = this;
        Display.getDisplay(this).setCurrent(new PlayCanvas());
    }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class PlayCanvas extends Canvas {
    boolean ran, queued;
    public void paint(Graphics g) {
        g.setColor(0x142033);
        g.fillRect(0, 0, getWidth(), getHeight());
        double root = Math.sqrt(9.0);
        String sqrt = root > 2.9 && root < 3.1 ? "SQRT" : "SQRT NG";
        String abs = Math.abs(-8) == 8 ? "ABS" : "ABS NG";
        String lng = Math.abs(-3L) == 3L ? "LONG" : "LONG NG";
        float back = Float.intBitsToFloat(Float.floatToIntBits(1.5f));
        String fbit = back == 1.5f ? "FBIT" : "FBIT NG";
        double dback = Double.longBitsToDouble(Double.doubleToLongBits(2.5));
        String dbit = dback == 2.5 ? "DBIT" : "DBIT NG";
        float mag = Math.abs(-1.5f);
        String fabs = mag > 1.4f && mag < 1.6f ? "FABS" : "FABS NG";
        String up = "ab".toUpperCase().equals("AB") ? "UP" : "UP NG";
        Vector list = new Vector();
        list.addElement("Q");
        String vec = "Q".equals(list.elementAt(0)) ? "VEC" : "VEC NG";
        int roll = new Random(3L).nextInt(5);
        String rnd = roll >= 0 && roll < 5 ? "RND" : "RND NG";
        g.setClip(1, 2, 10, 12);
        String clip = g.getClipWidth() == 10 && g.getClipHeight() == 12 ? "CLIP" : "CLIP NG";
        g.setClip(0, 0, getWidth(), getHeight());
        if (!queued) {
            queued = true;
            Display.getDisplay(Play.me).callSerially(new Runnable() {
                public void run() { ran = true; repaint(); }
            });
        }
        String ser = ran ? "SER" : "SER WAIT";
        g.setColor(0xF2F2F2);
        g.drawString("PLAY", 8, 12, 0);
        g.drawString(sqrt, 56, 12, 0);
        g.drawString(abs, 112, 12, 0);
        g.drawString(lng, 160, 12, 0);
        g.drawString(fbit, 8, 28, 0);
        g.drawString(dbit, 64, 28, 0);
        g.drawString(fabs, 120, 28, 0);
        g.drawString(up, 176, 28, 0);
        g.drawString(vec, 8, 44, 0);
        g.drawString(rnd, 56, 44, 0);
        g.drawString(clip, 104, 44, 0);
        g.drawString(ser, 160, 44, 0);
    }
}
