import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;
import javax.microedition.lcdui.game.Sprite;
import javax.microedition.midlet.MIDlet;
import javax.microedition.rms.RecordStore;

public class Save extends MIDlet {
    public void startApp() { Display.getDisplay(this).setCurrent(new SaveCanvas()); }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class SaveCanvas extends Canvas {
    public void paint(Graphics g) {
        g.setColor(0x142033);
        g.fillRect(0, 0, getWidth(), getHeight());
        String got = "NG";
        try {
        RecordStore rs = RecordStore.openRecordStore("note", true);
        if (rs.getNumRecords() == 0) {
            byte[] data = new byte[] { 65, 68, 65 };
            rs.addRecord(data, 0, data.length);
        }
        byte[] back = rs.getRecord(1);
        if (back != null) {
            String text = new String(back);
            if (text.equals("ADA")) got = "ADA";
        }
        rs.closeRecordStore();
        } catch (javax.microedition.rms.RecordStoreException problem) { got = "RMS NG"; }
        int[] px = new int[64];
        int i = 0;
        while (i < 64) { px[i] = 0xFFFFCC33; i = i + 1; }
        Image im = Image.createRGBImage(px, 8, 8, false);
        Sprite a = new Sprite(im);
        Sprite b = new Sprite(im);
        a.setPosition(20, 80);
        b.setPosition(24, 84);
        a.paint(g);
        b.paint(g);
        g.setColor(0xF2F2F2);
        g.drawString("SAVE", 16, 16, 0);
        g.drawString(got, 16, 28, 0);
        g.drawString(a.collidesWith(b, false) ? "HIT" : "MISS", 16, 40, 0);
    }
}
