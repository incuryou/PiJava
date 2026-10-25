import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
public final class PaintHandoff241Checks extends Canvas implements Runnable {
    static PaintHandoff241Checks self;
    static int paints,calls,workers,afterService,syncPairs,mode,inside,result;
    protected void paint(Graphics g){
        if(inside!=0)throw new RuntimeException("recursive paint");inside++;
        float f=0.2f;for(int i=0;i<500000;i++)f=(f+0.007f)*0.99f;
        result=Float.floatToRawIntBits(f);paints++;
        g.setColor(paints);g.fillRect(0,0,240,320);
        repaint();inside--;
    }
    public void run(){
        calls++;
        if(mode==1){int before=paints;repaint();serviceRepaints();
            if(paints!=before+1||inside!=0)throw new RuntimeException("first synchronous paint did not complete");
            repaint();serviceRepaints();
            if(paints!=before+2||inside!=0)throw new RuntimeException("second synchronous paint did not complete");syncPairs++;}
        if(calls<4)Display.getDisplay(null).callSerially(this);repaint();
    }
    public static int setup(int which){
        paints=calls=workers=afterService=syncPairs=inside=result=0;mode=which;
        self=new PaintHandoff241Checks();Display.getDisplay(null).setCurrent(self);
        Display.getDisplay(null).callSerially(self);self.repaint();return 1;
    }
    public static int startWorker(){new Thread(){public void run(){
        for(int i=0;i<6;i++){workers++;int before=paints;self.repaint();self.serviceRepaints();
            if(paints!=before+1||inside!=0)throw new RuntimeException("worker resumed before completed paint");
            afterService++;Thread.yield();}
    }}.start();return 1;}
}
