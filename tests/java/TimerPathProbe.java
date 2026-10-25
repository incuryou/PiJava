import java.util.Timer;
import java.util.TimerTask;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Display;
public class TimerPathProbe extends Canvas {
    public static volatile int updates, paints;
    static Timer timer;
    static TimerPathProbe canvas;
    static int mode;
    static int gateMs=70;
    static final Object plain=new Object();
    protected void paint(Graphics g) {paints++;g.setColor(updates);g.fillRect(0,0,16,16);}
    static final class Tick extends TimerTask {
        public void run() {
            if(mode==8){updates++;return;}
            if(mode==15&&updates>=20)while(true){updates++;canvas.repaint();canvas.serviceRepaints();}
            if(mode==9)try{synchronized(plain){plain.wait(70);}}catch(Exception e){}
            if(mode==7&&updates>=20)while(true){try{Thread.sleep(70);}catch(Exception e){}updates++;canvas.repaint();canvas.serviceRepaints();}
            // Start with a light Timer tick, then enter a scene with its own
            // variable clock gate. These later ticks overrun the Timer period.
            if((mode==4 && updates>=1)||(mode==14 && updates>=20)) {
                long previous=System.currentTimeMillis();
                while(System.currentTimeMillis()-previous<gateMs)Thread.yield();
            }
            if(mode==2) {
                long previous=System.currentTimeMillis();
                while(System.currentTimeMillis()-previous<20)Thread.yield();
            }
            if(mode==3)try{Thread.sleep(18);}catch(Exception e){}
            updates++;canvas.repaint();canvas.serviceRepaints();
            if(mode==6){canvas.repaint();canvas.serviceRepaints();}
        }
    }
    static final class Background extends TimerTask {public void run(){System.currentTimeMillis();}}
    public static void start(int requested) {
        mode=requested;updates=paints=0;canvas=new TimerPathProbe();
        Display.getDisplay(null).setCurrent(canvas);timer=new Timer();
        int period=mode==3?10:mode==11?20:mode==12?70:mode==13?110:41;
        if(mode==5)timer.scheduleAtFixedRate(new Tick(),1,period);else timer.schedule(new Tick(),1,period);
        if(mode==1)timer.schedule(new Background(),2,10);
    }
}
