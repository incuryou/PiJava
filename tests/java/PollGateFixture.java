import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;

public class PollGateFixture extends Canvas implements Runnable {
    public static int updates, auxiliary, mode, paints, paintStage, lastUpdate, lastStage, helperFailures, escaped;
    protected void paint(Graphics g) {
        paints++; lastUpdate=updates; lastStage=paintStage;
        g.setColor(updates*2+paintStage); g.fillRect(0,0,8,8);
    }
    public static void start(int selected) {
        updates=auxiliary=paints=paintStage=lastUpdate=lastStage=helperFailures=escaped=0; mode=selected;
        PollGateFixture c=new PollGateFixture();
        Display.getDisplay(null).setCurrent(c); new Thread(c).start();
    }
    public void run() {
        long previous=System.currentTimeMillis(), previousAux=previous;
        for (;;) {
            if(mode==0)Thread.yield();
            if(System.currentTimeMillis()-previousAux>=100) {
                previousAux=System.currentTimeMillis(); auxiliary++;
            }
            if(System.currentTimeMillis()-previous<50)continue;
            previous=System.currentTimeMillis(); updates++;
            if(mode==3 && updates>=8) {
                for(;;) {paintStage=2; repaint(); serviceRepaints(); Thread.yield();}
            }
            if(mode==4 && updates==8 && escaped==0) {
                escaped=1; long detour=System.currentTimeMillis();
                // Leave the frame gate long enough to expire the host bound,
                // then return without an explicit sleep or a method exit.
                while(System.currentTimeMillis()-detour<600) {
                    paintStage=2; repaint(); serviceRepaints(); Thread.yield();
                }
            }
            if((mode==2 && updates>=8) || (mode==4 && updates>=16)) {
                paintStage=1; repaint(); serviceRepaints();
                if(lastUpdate!=updates || lastStage!=1)helperFailures++;
                paintStage=2; repaint(); // Async final frame must follow the helper.
            } else {paintStage=2; repaint(); serviceRepaints();}
        }
    }
}
