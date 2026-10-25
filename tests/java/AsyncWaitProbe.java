import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Display;
import java.util.Timer;
import java.util.TimerTask;

public class AsyncWaitProbe {
    public static volatile int updates, paints, interrupted, lockErrors, returned, ticks;
    static int mode;
    static Loop canvas;
    static Thread worker;
    static class Loop extends Canvas implements Runnable {
        int period=50;
        int state;
        protected void paint(Graphics g) {paints++;g.setColor(updates);g.fillRect(0,0,12,12);if(mode==16)repaint();}
        public void run() {
            while(true) {
                long start=System.currentTimeMillis();
                switch(state) {case 0: updates++;break;case 2: updates+=2;break;default:updates++;}
                if(mode!=4)repaint();
                long elapsed;
                if((elapsed=System.currentTimeMillis()-start)<period) {
                    try {synchronized(this){wait(period-elapsed);}}catch(InterruptedException e){interrupted++;}
                } else Thread.yield();
            }
        }
    }
    public static void start(int requested) {
        mode=requested;canvas=new Loop();Display.getDisplay(null).setCurrent(canvas);
        if(mode==18)new Timer().schedule(new TimerTask(){public void run(){canvas.run();}},0,50);
        else {worker=new Thread(canvas);worker.start();}
        if(mode==5)new Timer().schedule(new TimerTask(){public void run(){ticks++;}},0,50);
    }
    public static void signal(int action) {
        synchronized(canvas) {
            int before=updates,exceptions=interrupted;
            if(action==1)worker.interrupt();else if(action==2)canvas.notifyAll();else canvas.notify();
            try {Thread.sleep(30);}catch(InterruptedException e){lockErrors++;}
            if(updates!=before||interrupted!=exceptions||!Thread.holdsLock(canvas))lockErrors++;
        }
    }
    public static void request() {canvas.repaint();}
    public static void replace() {Display.getDisplay(null).setCurrent(new Loop());}
    public static void changePeriod() {canvas.period=70;}
}
