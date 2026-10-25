import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;

// A stateful loop with two genuine clock definitions of one static anchor.
public class AliasedFieldGateProbe extends Canvas implements Runnable {
    static long anchor, sideClock;
    static volatile long volatileAnchor;
    public static class AnchorParent {public static long tick;}
    public static class AnchorChild extends AnchorParent {}
    public static class UnloadedAlias extends AnchorParent {
        static {foreignUpdates++;}
    }
    static int updates, paints, mode, phase, foreignUpdates;
    static AliasedFieldGateProbe canvas;
    protected void paint(Graphics g) {paints++;g.setColor(updates);g.fillRect(0,0,8,8);}
    public static void start(int selected) {
        mode=selected;updates=paints=phase=foreignUpdates=0;anchor=sideClock=0;
        if(mode>=7)AnchorParent.tick=0;
        if(mode==7||mode==8)new AnchorChild();
        canvas=new AliasedFieldGateProbe();Display.getDisplay(null).setCurrent(canvas);
        new Thread(canvas).start();
        if(mode==3)new Thread(new Runnable(){public void run(){
            for(;;){canvas.repaint();canvas.serviceRepaints();foreignUpdates++;
                try{Thread.sleep(13);}catch(Exception ignored){}}
        }}).start();
    }
    public static void changeCanvas(){mode=1;Display.getDisplay(null).setCurrent(new AliasedFieldGateProbe());}
    public void run(){
        // Loading begins as sleep-only drawing, then enters the field gate.
        for(int i=0;i<3;i++){
            if(mode!=1&&mode!=3){canvas.repaint();canvas.serviceRepaints();}
            try{Thread.sleep(24);}catch(Exception ignored){}
        }
        if(mode==5)businessLoop();
        else if(mode==6)volatileLoop();
        else if(mode==7)inheritedLoop();
        else if(mode==8)extraInheritedLoop();
        else if(mode==9)unloadedAliasLoop();
        else loop();
    }
    static void volatileLoop() {
        long now=0,elapsed=0;
        for(;;) {
            while((elapsed=(now=System.currentTimeMillis())-volatileAnchor)<37) {}
            volatileAnchor=now;
            sideClock=(now=System.currentTimeMillis())-volatileAnchor;
            volatileAnchor=now;
            updates++;canvas.repaint();canvas.serviceRepaints();
            try{Thread.sleep(24);}catch(Exception ignored){}
        }
    }
    static void inheritedLoop() {
        long now=0,elapsed=0;
        for(;;) {
            while((elapsed=(now=System.currentTimeMillis())-AnchorParent.tick)<37) {}
            AnchorParent.tick=now;
            sideClock=(now=System.currentTimeMillis())-AnchorParent.tick;
            AnchorChild.tick=now;
            updates++;canvas.repaint();canvas.serviceRepaints();
            try{Thread.sleep(24);}catch(Exception ignored){}
        }
    }
    static void extraInheritedLoop() {
        long now=0,elapsed=0;
        for(;;) {
            while((elapsed=(now=System.currentTimeMillis())-AnchorParent.tick)<37) {}
            AnchorParent.tick=now;
            sideClock=(now=System.currentTimeMillis())-AnchorParent.tick;
            AnchorParent.tick=now;
            // A third definition is outside the two-clock certificate even
            // when this business bookkeeping preserves the numeric value.
            AnchorChild.tick=AnchorParent.tick+0;
            updates++;canvas.repaint();canvas.serviceRepaints();
            try{Thread.sleep(24);}catch(Exception ignored){}
        }
    }
    static void unloadedAliasLoop() {
        long now=0,elapsed=0;
        for(;;) {
            while((elapsed=(now=System.currentTimeMillis())-AnchorParent.tick)<37) {}
            AnchorParent.tick=now;
            sideClock=(now=System.currentTimeMillis())-AnchorParent.tick;
            AnchorParent.tick=now;
            if(updates<0)UnloadedAlias.tick=AnchorParent.tick+0;
            updates++;canvas.repaint();canvas.serviceRepaints();
            try{Thread.sleep(24);}catch(Exception ignored){}
        }
    }
    static void businessLoop() {
        long now=0,elapsed=0;
        for(;;) {
            while((elapsed=(now=System.currentTimeMillis())-anchor)<37) {}
            anchor=now;
            sideClock=(now=System.currentTimeMillis())-anchor;
            anchor=now+1; // A business adjustment is not an unmodified clock.
            updates++;canvas.repaint();canvas.serviceRepaints();
            try{Thread.sleep(24);}catch(Exception ignored){}
        }
    }
    // Static worker deliberately has no Canvas in local slot zero.
    static void loop() {
        long now=0,elapsed=0;
        for(;;) {
            while((elapsed=(now=System.currentTimeMillis())-anchor)<37) {}
            anchor=now;
            sideClock=(now=System.currentTimeMillis())-anchor;
            anchor=now;
            elapsed=37;
            updates++;
            if(mode!=1&&mode!=3) {canvas.repaint();canvas.serviceRepaints();}
            if(mode==2&&updates==12) {
                phase=1;long loading=System.currentTimeMillis();
                while(System.currentTimeMillis()-loading<450) {}
                phase=2;
            }
            long delay=mode==4&&updates%11==0?91:Math.max(61-elapsed,0);
            try{Thread.sleep(delay);}catch(Exception ignored){}
        }
    }
}
