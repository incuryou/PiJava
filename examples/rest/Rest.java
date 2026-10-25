import com.motorola.multimedia.Vibrator;
import com.nokia.mid.ui.DeviceControl;
import com.nokia.mid.ui.DirectUtils;
import com.nokia.mid.ui.FullCanvas;
import com.samsung.util.AudioClip;
import com.samsung.util.Vibration;
import com.siemens.mp.game.Light;
import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.util.Calendar;
import java.util.Date;
import java.util.Hashtable;
import javax.microedition.io.HttpConnection;
import javax.microedition.lcdui.Alert;
import javax.microedition.lcdui.Choice;
import javax.microedition.lcdui.ChoiceGroup;
import javax.microedition.lcdui.Font;
import javax.microedition.lcdui.Form;
import javax.microedition.lcdui.Gauge;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;
import javax.microedition.lcdui.ImageItem;
import javax.microedition.lcdui.List;
import javax.microedition.lcdui.StringItem;
import javax.microedition.lcdui.TextBox;
import javax.microedition.lcdui.TextField;
import javax.microedition.lcdui.Ticker;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.game.GameCanvas;
import javax.microedition.lcdui.game.LayerManager;
import javax.microedition.lcdui.game.Sprite;
import javax.microedition.media.Manager;
import javax.microedition.media.Player;
import javax.microedition.midlet.MIDlet;
import javax.microedition.rms.RecordStore;

public class Rest extends MIDlet {
    public void startApp() { Display.getDisplay(this).setCurrent(new RestCanvas()); }
    public void pauseApp() {}
    public void destroyApp(boolean unconditional) {}
}

class RestGame extends GameCanvas {
    RestGame() { super(true); }
    public void paint(Graphics g) {}
}

class RestCanvas extends Canvas {
    String mark(boolean ok, String name) { return ok ? name : name + " NG"; }

    boolean classes() {
        String[] names = new String[] {
            "java.lang.Byte", "java.lang.Short", "java.lang.Boolean", "java.lang.Long",
            "java.util.Date", "java.util.Calendar", "java.util.Hashtable",
            "java.io.PrintStream", "java.io.DataInputStream",
            "javax.microedition.lcdui.Form", "javax.microedition.lcdui.List",
            "javax.microedition.lcdui.Alert", "javax.microedition.lcdui.TextBox",
            "javax.microedition.lcdui.game.Sprite", "javax.microedition.lcdui.game.LayerManager",
            "javax.microedition.media.Player", "javax.microedition.media.control.VolumeControl",
            "javax.microedition.rms.RecordStore", "javax.microedition.io.HttpConnection",
            "javax.wireless.messaging.TextMessage", "javax.wireless.messaging.MessageConnection",
            "com.nokia.mid.ui.FullCanvas", "com.nokia.mid.ui.DirectUtils",
            "com.samsung.util.AudioClip", "com.samsung.util.Vibration",
            "com.siemens.mp.game.Light", "com.motorola.multimedia.Vibrator"
        };
        int i = 0;
        while (i < names.length) {
            try { Class.forName(names[i]); }
            catch (ClassNotFoundException e) { return false; }
            i = i + 1;
        }
        return true;
    }

    public void paint(Graphics g) {
        g.setColor(0x142033);
        g.fillRect(0, 0, getWidth(), getHeight());
        String box = "BOX NG";
        try {
            boolean ok = Byte.parseByte("7") == 7 && Short.parseShort("8") == 8
                && Boolean.TRUE.booleanValue() && new Long(9L).longValue() == 9L;
            box = mark(ok, "BOX");
        } catch (Exception e) { box = "BOX NG"; }
        int year = Calendar.getInstance().get(Calendar.YEAR);
        String cal = mark(year >= 2020 && year <= 2100, "CAL");
        Hashtable map = new Hashtable();
        map.put("k", "v");
        String hash = mark("v".equals(map.get("k")) && map.size() == 1, "MAP");
        String buf = mark(new StringBuffer("A").append("B").toString().equals("AB"), "BUF");
        Font font = Font.getFont(Font.FACE_SYSTEM, Font.STYLE_BOLD, Font.SIZE_MEDIUM);
        String fnt = mark(font.stringWidth("AB") == 16 && font.charWidth('A') == 8 && font.getHeight() == 8, "FONT");
        String ui = "UI NG";
        try {
            Form form = new Form("F");
            form.append("x");
            List list = new List("L", List.IMPLICIT);
            list.append("a", null);
            Alert alert = new Alert("A", "body", null, null);
            TextField field = new TextField("t", "hi", 8, TextField.ANY);
            StringItem item = new StringItem("s", "i");
            Gauge gauge = new Gauge("g", false, 10, 3);
            Ticker ticker = new Ticker("tick");
            ChoiceGroup choice = new ChoiceGroup("c", Choice.EXCLUSIVE);
            choice.append("z", null);
            TextBox boxText = new TextBox("b", "7", 8, TextField.NUMERIC);
            ImageItem imageItem = new ImageItem("i", Image.createImage(2, 2), ImageItem.LAYOUT_LEFT, "alt");
            boolean ok = form.size() == 1 && list.size() == 1 && alert.getTimeout() == Alert.FOREVER
                && "hi".equals(field.getString()) && "i".equals(item.getText()) && gauge.getValue() == 3
                && "tick".equals(ticker.getString()) && choice.size() == 1 && "7".equals(boxText.getString())
                && imageItem.getImage() != null;
            ui = mark(ok, "UI");
        } catch (Exception e) { ui = "UI NG"; }
        String spr = "SPR NG";
        try {
            Image img = Image.createImage(8, 8);
            Sprite sprite = new Sprite(img, 8, 8);
            sprite.setFrame(0);
            LayerManager layers = new LayerManager();
            layers.append(sprite);
            RestGame game = new RestGame();
            int keys = game.getKeyStates();
            spr = mark(sprite.getFrame() == 0 && layers.getSize() == 1 && keys >= 0, "SPR");
        } catch (Exception e) { spr = "SPR NG"; }
        String med = "MED NG";
        try {
            // Real SMF 0 fixture, one note lasting half a second at the default tempo.
            byte[] midi = {77,84,104,100,0,0,0,6,0,0,0,1,0,96,77,84,114,107,0,0,0,12,
                0,(byte)0x90,60,80,96,(byte)0x80,60,0,0,(byte)0xff,0x2f,0};
            Player player = Manager.createPlayer(new ByteArrayInputStream(midi), "audio/midi");
            player.start();
            player.stop();
            med = mark(player.getState() == Player.PREFETCHED && player.getDuration() == 500000, "MED");
            player.close();
        } catch (Exception e) { med = "MED NG"; }
        String rms = "RMS NG";
        try {
            RecordStore store = RecordStore.openRecordStore("rest", true);
            int id = store.addRecord(new byte[] { 90 }, 0, 1);
            byte[] back = store.getRecord(id);
            store.closeRecordStore();
            rms = mark(back != null && back.length == 1 && back[0] == 90, "RMS");
        } catch (Exception e) { rms = "RMS NG"; }
        String nok = "NOK NG";
        try {
            DeviceControl.setLights(0, 50);
            boolean ok = FullCanvas.KEY_UP == -1 && DirectUtils.getDirectGraphics(g) != null
                && DirectUtils.createImage(2, 2, 0xff000000) != null;
            nok = mark(ok, "NOK");
        } catch (Exception e) { nok = "NOK NG"; }
        Vibration.start(1, 1);
        Vibration.stop();
        Light.setLightOff();
        Vibrator.stop();
        new AudioClip(AudioClip.TYPE_MIDI, "a.mid").stop();
        String oem = mark(!Vibration.isSupported() && AudioClip.TYPE_MIDI == 3, "OEM");
        String io = "IO NG";
        try {
            byte[] raw = new byte[] { 0, 7 };
            int n = new DataInputStream(new ByteArrayInputStream(raw)).readShort();
            io = mark(n == 7, "IO");
        } catch (Exception e) { io = "IO NG"; }
        Runtime runtime = Runtime.getRuntime();
        String rt = mark(runtime.freeMemory() > 0 && runtime.totalMemory() >= runtime.freeMemory(), "RT");
        String cls = mark(classes(), "CLS");
        String date = mark(new Date(0L).getTime() == 0L && HttpConnection.HTTP_OK == 200, "DATE");
        g.setColor(0xF2F2F2);
        g.drawString("REST", 8, 8, 0);
        g.drawString(box, 56, 8, 0);
        g.drawString(cal, 104, 8, 0);
        g.drawString(hash, 152, 8, 0);
        g.drawString(buf, 8, 24, 0);
        g.drawString(fnt, 56, 24, 0);
        g.drawString(ui, 112, 24, 0);
        g.drawString(spr, 160, 24, 0);
        g.drawString(med, 8, 40, 0);
        g.drawString(rms, 56, 40, 0);
        g.drawString(nok, 104, 40, 0);
        g.drawString(oem, 152, 40, 0);
        g.drawString(io, 8, 56, 0);
        g.drawString(rt, 56, 56, 0);
        g.drawString(cls, 104, 56, 0);
        g.drawString(date, 152, 56, 0);
    }
}
