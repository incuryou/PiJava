import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.util.Stack;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Command;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;
import javax.microedition.lcdui.game.TiledLayer;
import javax.microedition.midlet.MIDlet;

public class Mix extends MIDlet {
    public void startApp() { Display.getDisplay(this).setCurrent(new MixCanvas()); }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class MixCanvas extends Canvas {
    public void paint(Graphics g) {
        g.setColor(0x142033);
        g.fillRect(0, 0, getWidth(), getHeight());
        String wide = Wide.sum() == 11 ? "WIDE" : "WIDE NG";
        String png = "PNG NG";
        try {
            Image img = Image.createImage("/dot.png");
            int[] px = new int[1];
            img.getRGB(px, 0, 1, 0, 0, 1, 1);
            if ((px[0] & 0xffffff) == 0xff0000) png = "PNG";
        } catch (Exception e) { png = "PNG NG"; }
        String out = "OUT NG";
        try {
            ByteArrayOutputStream raw = new ByteArrayOutputStream();
            DataOutputStream data = new DataOutputStream(raw);
            data.writeInt(65);
            int n = new DataInputStream(new ByteArrayInputStream(raw.toByteArray())).readInt();
            if (n == 65) out = "OUT";
        } catch (Exception e) { out = "OUT NG"; }
        Stack stack = new Stack();
        stack.push("Z");
        String stk = "Z".equals(stack.pop()) ? "STK" : "STK NG";
        String prop = "MIDP-2.0".equals(System.getProperty("microedition.profiles")) ? "PROP" : "PROP NG";
        String th = Thread.currentThread() != null ? "TH" : "TH NG";
        Image blank = Image.createImage(8, 8);
        TiledLayer layer = new TiledLayer(1, 1, blank, 8, 8);
        layer.setCell(0, 0, 3);
        String tile = layer.getCell(0, 0) == 3 ? "TILE" : "TILE NG";
        Command cmd = new Command("Go", Command.OK, 1);
        String command = "Go".equals(cmd.getLabel()) ? "CMD" : "CMD NG";
        g.setColor(0xF2F2F2);
        g.drawString("MIX", 8, 12, 0);
        g.drawString(wide, 8, 28, 0);
        g.drawString(png, 56, 28, 0);
        g.drawString(out, 104, 28, 0);
        g.drawString(stk, 152, 28, 0);
        g.drawString(prop, 8, 44, 0);
        g.drawString(th, 56, 44, 0);
        g.drawString(tile, 96, 44, 0);
        g.drawString(command, 144, 44, 0);
    }
}
