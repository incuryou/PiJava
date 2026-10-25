import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Display;

// Actual Java loops: remainder waits are frame pacing, condition waits are not.
public class WaitFrameProbe {
    public static volatile int updates, paints, returned, interrupted, lockErrors;
    static int mode, work, sink;
    static Base canvas;
    static Thread worker;
    static volatile boolean ready;
    static class Base extends Canvas implements Runnable {
        protected void paint(Graphics g) {paints++;g.setColor(updates);g.fillRect(0,0,12,12);}
        void draw() {
            burn();
            updates++;repaint();serviceRepaints();
        }
        public void run() {}
    }
    public static void configureWork(int iterations) {work=iterations;burn();}
    static void burn() {for(int n=0;n<work;n++)sink=sink*33+n;}
    static class Local extends Base {
        public void run() {
            while(true) {
                long start=System.currentTimeMillis();
                draw();
                long elapsed=System.currentTimeMillis()-start;
                if(elapsed<47)try {synchronized(this){wait(47-elapsed);}}catch(InterruptedException e){interrupted++;}
                else Thread.yield();
            }
        }
    }
    static class Field extends Base {
        long elapsed;
        public void run() {
            while(true) {
                elapsed=System.currentTimeMillis();
                draw();
                elapsed=System.currentTimeMillis()-elapsed;
                if(elapsed<63)try {synchronized(this){wait(63-elapsed);}}catch(InterruptedException e){interrupted++;}
                else Thread.yield();
            }
        }
    }
    static class NoDraw extends Base {
        public void run() {
            while(true) {
                long start=System.currentTimeMillis();
                updates++;
                long elapsed=System.currentTimeMillis()-start;
                if(elapsed<47)try {synchronized(this){wait(47-elapsed);}}catch(InterruptedException e){}
                else Thread.yield();
            }
        }
    }
    static class Condition extends Base {
        public void run() {
            while(true) {
                long start=System.currentTimeMillis();draw();
                long elapsed=System.currentTimeMillis()-start;
                if(elapsed<47)try {synchronized(this){while(!ready) {wait(47-elapsed);returned++;}}}catch(InterruptedException e){}
                else Thread.yield();
            }
        }
    }
    static class Fixed extends Base {
        public void run() {while(true){draw();try{synchronized(this){wait(47);}}catch(InterruptedException e){}}}
    }
    static class Infinite extends Base {
        public void run() {draw();try{synchronized(this){wait(0);returned++;}}catch(InterruptedException e){interrupted++;}}
    }
    static class Second extends Base {
        public void run() {
            while(true) {
                long start=System.currentTimeMillis();draw();
                long elapsed=System.currentTimeMillis()-start;
                if(elapsed<47)try {synchronized(this){wait(47-elapsed);wait(47);}}catch(InterruptedException e){}
                else Thread.yield();
            }
        }
    }
    static class Stale extends Base {
        public void run() {
            while(true) {
                draw();long start=System.currentTimeMillis();
                long elapsed=System.currentTimeMillis()-start;
                if(elapsed<47)try {synchronized(this){wait(47-elapsed);}}catch(InterruptedException e){}
                else Thread.yield();
            }
        }
    }
    static class Guarded extends Base {
        public void run() {
            while(true) {
                long start=System.currentTimeMillis();draw();
                if(!ready) {
                    long elapsed=System.currentTimeMillis()-start;
                    if(elapsed<47)try{synchronized(this){wait(47-elapsed);}}catch(InterruptedException e){}
                    else Thread.yield();
                }
            }
        }
    }
    static class Painter implements Runnable {
        public void run() {while(true){canvas.repaint();canvas.serviceRepaints();try{Thread.sleep(47);}catch(InterruptedException e){}}}
    }
    public static void signal(int action) {
        synchronized(canvas) {
            int before=updates,exceptions=interrupted;
            if(action==1)worker.interrupt();else if(action==2)canvas.notifyAll();else canvas.notify();
            try {Thread.sleep(30);}catch(InterruptedException e){lockErrors++;}
            if(updates!=before||interrupted!=exceptions)lockErrors++;
            if(!Thread.holdsLock(canvas))lockErrors++;
        }
    }
    public static void start(int requested) {
        mode=requested;
        updates=paints=returned=interrupted=lockErrors=0;ready=false;
        canvas=mode==1?new Field():(mode==4||mode==16)?new NoDraw():mode==5?new Condition():mode==6?new Fixed():mode==7?new Infinite():mode==14?new Second():mode==15?new Stale():mode==17?new Guarded():new Local();
        Display.getDisplay(null).setCurrent(canvas);
        worker=new Thread(canvas);worker.start();
        if(mode==16)new Thread(new Painter()).start();
    }
}
