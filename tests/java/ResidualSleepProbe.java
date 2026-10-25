import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import java.util.Timer;
import java.util.TimerTask;
public class ResidualSleepProbe extends Canvas implements Runnable {
    static volatile int updates;
    static volatile long requested;
    static int work, sink;
    static int mode;
    private long startTime, endTime;
    private short period;
    public static void start(int m) {
        mode=m;updates=0;ResidualSleepProbe c=new ResidualSleepProbe();
        c.period=(short)(m==4?13:m==3?31:22);
        Display.getDisplay(null).setCurrent(c);
        if(m==4)new Timer().schedule(new Tick(c),1,41);else new Thread(c).start();
    }
    static class Tick extends TimerTask {
        final ResidualSleepProbe canvas;
        Tick(ResidualSleepProbe c){canvas=c;}
        public void run(){canvas.tick();}
    }
    public static void configureWork(int iterations){work=iterations;burn();}
    static void burn(){for(int n=0;n<work;n++)sink=sink*33+n;}
    // A Canvas helper can be nested inside TimerTask.run; its receiver alone
    // cannot establish that this is an ordinary frame-loop thread.
    void tick() {
        startTime=System.currentTimeMillis();burn();updates++;
        repaint();serviceRepaints();
        endTime=System.currentTimeMillis();
        requested=period-(endTime-startTime);
        try {if(endTime-startTime<period)Thread.sleep(period-(endTime-startTime));}catch(Exception e){}
    }
    protected void paint(Graphics g) {g.setColor(updates);g.fillRect(0,0,12,12);}
    public void run() {
        for(;;) {
            startTime=System.currentTimeMillis();
            updates++;
            if(mode!=2){repaint();serviceRepaints();}
            endTime=System.currentTimeMillis();
            try {
                if(mode==1)Thread.sleep(6);
                else if(endTime-startTime<period)Thread.sleep(period-(endTime-startTime));
            }catch(Exception e){}
        }
    }
}
