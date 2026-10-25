import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Display;

public class ConditionalWaitProbe {
    public static volatile int ticks, paints, stats, interrupted, lockErrors;
    static volatile boolean ready;
    static Base canvas;
    static Thread worker;
    static int mode, businessPeriod=53;
    static class Base extends Canvas implements Runnable {
        protected void paint(Graphics g) {paints++;g.setColor(ticks);g.fillRect(0,0,12,12);}
        public void run() {}
        void draw() {repaint();serviceRepaints();}
    }
    static class Frame extends Base {
        public void run() {
            long statisticDeadline=0;
            while(true) {
                long start=System.currentTimeMillis();
                if(ready)continue;
                switch(ticks&3) {case 0:ticks++;break;case 2:ticks++;break;default:ticks++;}
                if(mode!=2&&(mode!=1||(ticks&1)!=0))draw();
                if(System.currentTimeMillis()>=statisticDeadline) {
                    statisticDeadline=System.currentTimeMillis()+1000;stats++;
                }
                long elapsed=System.currentTimeMillis()-start;
                int period=53;
                if(elapsed<period)try{synchronized(this){wait(period-elapsed);}}catch(InterruptedException e){interrupted++;}
                Thread.yield();
            }
        }
    }
    static class Business extends Base {
        public void run() {
            while(true) {
                long start=System.currentTimeMillis();ticks++;draw();
                long elapsed=System.currentTimeMillis()-start;int period=53;
                if(!ready) {
                    if(elapsed<period)try{synchronized(this){wait(period-elapsed);}}catch(InterruptedException e){interrupted++;}
                }
                Thread.yield();
            }
        }
    }
    static class SideEffect extends Base {
        public void run() {
            while(true) {
                long start=System.currentTimeMillis();ticks++;draw();
                long elapsed=System.currentTimeMillis()-start;int period=businessPeriod++;
                if(elapsed<period)try{synchronized(this){wait(period-elapsed);}}catch(InterruptedException e){interrupted++;}
                Thread.yield();
            }
        }
    }
    static class Statistics extends Base {
        public void run() {
            while(true) {
                ticks++;draw();long earlier=System.currentTimeMillis();stats++;
                long elapsed=System.currentTimeMillis()-earlier;int period=53;
                if(elapsed<period)try{synchronized(this){wait(period-elapsed);}}catch(InterruptedException e){interrupted++;}
                Thread.yield();
            }
        }
    }
    // The outer local-variable threshold intentionally uses the legacy
    // fallback learner. The inner source candidate never paints or waits.
    static class Fallback extends Base {
        void temporary() {
            int rounds=0;
            while(rounds++<2) {
                long start=System.currentTimeMillis();stats++;
                if(mode==9)continue;
                long elapsed=System.currentTimeMillis()-start;int period=53;
                if(elapsed<period)try{synchronized(this){wait(period-elapsed);}}catch(InterruptedException e){interrupted++;}
                Thread.yield();
            }
        }
        public void run() {
            long previous=System.currentTimeMillis();
            while(true) {
                long gate=67;
                while(System.currentTimeMillis()-previous<gate)Thread.yield();
                previous=System.currentTimeMillis();ticks++;draw();temporary();
            }
        }
    }
    public static void signal(int action) {
        synchronized(canvas) {
            int before=ticks,exceptions=interrupted;
            if(action==1)worker.interrupt();else if(action==2)canvas.notifyAll();else canvas.notify();
            try{Thread.sleep(30);}catch(InterruptedException e){lockErrors++;}
            if(ticks!=before||interrupted!=exceptions||!Thread.holdsLock(canvas))lockErrors++;
        }
    }
    public static void start(int requested) {
        mode=requested;ready=false;ticks=paints=stats=interrupted=lockErrors=0;
        canvas=mode==9?new Fallback():mode==3?new Business():mode==4?new SideEffect():mode==5?new Statistics():new Frame();
        Display.getDisplay(null).setCurrent(canvas);worker=new Thread(canvas);worker.start();
    }
}
