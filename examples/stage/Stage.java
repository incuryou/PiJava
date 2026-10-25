import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.game.Sprite;
import javax.microedition.lcdui.game.TiledLayer;
import javax.microedition.midlet.MIDlet;
import com.nokia.mid.ui.DirectGraphics;
import com.nokia.mid.ui.DirectUtils;

public class Stage extends MIDlet {
    public void startApp() { Display.getDisplay(this).setCurrent(new StageCanvas()); }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class StageCanvas extends Canvas {
    public void paint(Graphics g) {
        g.setColor(0x142033);
        g.fillRect(0, 0, getWidth(), getHeight());
        int[] sheet = new int[128];
        int i = 0;
        while (i < 64) { sheet[i] = 0xFF1E3A34; i = i + 1; }
        while (i < 128) { sheet[i] = 0xFFE07A3D; i = i + 1; }
        Image tiles = Image.createRGBImage(sheet, 16, 8, true);
        Image copy = Image.createImage(tiles);
        TiledLayer layer = new TiledLayer(2, 1, copy, 8, 8);
        int anim = layer.createAnimatedTile(2);
        layer.setCell(0, 0, 1);
        layer.setCell(1, 0, anim);
        layer.setPosition(16, 160);
        layer.paint(g);
        int[] frames = new int[128];
        i = 0;
        while (i < 64) { frames[i] = 0xFF2244AA; i = i + 1; }
        while (i < 128) { frames[i] = 0xFF3DDC97; i = i + 1; }
        Sprite sp = new Sprite(Image.createRGBImage(frames, 8, 16, true), 8, 8);
        sp.setFrame(1);
        sp.setTransform(Sprite.TRANS_MIRROR);
        sp.setPosition(48, 160);
        sp.paint(g);
        byte[] px = new byte[64];
        i = 0;
        while (i < 64) { px[i] = (byte) 0xE0; i = i + 1; }
        DirectGraphics dg = DirectUtils.getDirectGraphics(g);
        dg.drawPixels(px, null, 0, 8, 16, 48, 8, 8, 0, 332);
        int[] back = new int[1];
        dg.getPixels(back, false, 0, 1, 17, 49, 1, 1, 0);
        int red = (back[0] >> 16) & 255;
        dg.fillTriangle(16, 80, 88, 80, 52, 130, 0xFFC6A15B);
        g.setColor(0xF2F2F2);
        g.drawString("STAGE", 16, 16, 0);
        g.drawString(layer.getAnimatedTile(anim) == 2 ? "TILE 2" : "TILE NG", 16, 28, 0);
        g.drawString(sp.getFrame() == 1 ? "FRAME 1" : "FRAME NG", 16, 40, 0);
        g.drawString(red > 200 ? "RED" : "NG", 96, 40, 0);
    }
}
