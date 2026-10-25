import java.io.ByteArrayInputStream;
import javax.microedition.media.Control;
import javax.microedition.media.Manager;
import javax.microedition.media.Player;
import javax.microedition.media.control.RateControl;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.midlet.MIDlet;

public final class RateCompat234Checks extends MIDlet {
    private static Player player;
    private static RateControl rate;
    private static int assertions;
    protected void startApp() {}
    protected void pauseApp() {}
    protected void destroyApp(boolean unconditional) {}
    private static void check(boolean value, String label) {
        assertions++; if (!value) throw new RuntimeException(label);
    }
    public static MIDlet setup(int requested) throws Exception {
        RateCompat234Checks app = new RateCompat234Checks();
        Display.getDisplay(app).setCurrent(new Canvas() { protected void paint(Graphics g) {} });
        player = Manager.createPlayer(new ByteArrayInputStream(NativeMediaChecks.wav()), "audio/wav");
        player.realize(); rate=(RateControl)player.getControl("RateControl");
        if (rate == null) throw new RuntimeException("real RateControl missing");
        rate.setRate(requested); player.start(); return app;
    }
    public static long time() { return player.getMediaTime(); }
    public static int value() { return rate.getRate(); }
    public static int change() { return rate.setRate(50000); }
    public static int close() { player.close(); return player.getState(); }
    public static int closedControl() {
        try { rate.setRate(100000); return 0; } catch (IllegalStateException expected) { return 1; }
    }
    public static int run() throws Exception {
        assertions=0;
        Player p=Manager.createPlayer(new ByteArrayInputStream(NativeMediaChecks.wav()), "audio/wav");
        boolean rejected=false;
        try { p.getControl("RateControl"); } catch (IllegalStateException expected) { rejected=true; }
        check(rejected,"unrealized control lookup rejected");
        p.realize(); RateControl r=(RateControl)p.getControl("RateControl");
        check(r!=null && r==p.getControl("javax.microedition.media.control.RateControl"),"rate names share identity");
        check(r.getRate()==100000 && r.getMinRate()==50000 && r.getMaxRate()==200000,"rate range includes normal speed");
        check(r.setRate(1)==50000 && r.getRate()==50000,"low rate clamps");
        check(r.setRate(300000)==200000 && r.getRate()==200000,"high rate clamps");
        check(r.setRate(100000)==100000,"normal speed is exact");
        boolean listed=false; Control[] all=p.getControls();
        for(int i=0;i<all.length;i++) if(all[i]==r) listed=true;
        check(listed && p.getControl("UnsupportedControl")==null,"control discovery is truthful");
        p.start(); check(r.setRate(75000)==75000,"started player accepts rate change");
        p.stop(); p.deallocate(); check(r.getRate()==75000,"deallocate retains rate");
        p.close(); rejected=false;
        try { r.getRate(); } catch(IllegalStateException expected) { rejected=true; }
        check(rejected,"retained closed control cannot report stale live state");
        rejected=false; try { r.setRate(100000); } catch(IllegalStateException expected) { rejected=true; }
        check(rejected,"retained closed control cannot bind another player");
        int i=0; while(i<80) {
            p=Manager.createPlayer(new ByteArrayInputStream(NativeMediaChecks.wav()),"audio/wav");
            p.realize(); check(((RateControl)p.getControl("RateControl")).getRate()==100000,"closed registry slots reusable");
            p.close(); i++;
        }
        return assertions;
    }
}
