import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;
import javax.microedition.midlet.MIDlet;
import com.nokia.mid.ui.DirectGraphics;
import com.nokia.mid.ui.DirectUtils;
import com.samsung.util.Vibration;
import com.motorola.multimedia.Vibrator;
import com.siemens.mp.game.Light;

public class Film extends MIDlet {
    public void startApp() { Display.getDisplay(this).setCurrent(new FilmCanvas()); }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class FilmCanvas extends Canvas {
    public void paint(Graphics g) {
        Vibration.isSupported();
        Vibrator.stop();
        Light.setLightOff();
        g.setColor(0x142033);
        g.fillRect(0, 0, getWidth(), getHeight());
        try { g.drawImage(Image.createImage("/dot.gif"), 16, 48, 0); }
        catch (java.io.IOException ignored) {}
        DirectGraphics dg = DirectUtils.getDirectGraphics(g);
        int[] hole = new int[1];
        int[] edge = new int[1];
        dg.getPixels(hole, false, 0, 1, 19, 51, 1, 1, 0);
        dg.getPixels(edge, false, 0, 1, 16, 48, 1, 1, 0);
        int holeRgb = hole[0] & 0xffffff;
        int edgeRed = (edge[0] >> 16) & 255;
        g.setColor(0xF2F2F2);
        g.drawString("FILM", 16, 16, 0);
        g.drawString(holeRgb == 0x142033 && edgeRed > 200 ? "HOLE" : "NG", 16, 28, 0);
    }
}
