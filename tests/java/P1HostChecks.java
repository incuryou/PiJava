import javax.microedition.midlet.MIDlet;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.media.Manager;
import javax.microedition.media.Player;
import javax.microedition.media.PlayerListener;
import java.io.ByteArrayInputStream;

/** Actual interpreted callbacks for the shared Switch outer-turn matrix. */
public final class P1HostChecks extends MIDlet {
    static P1HostChecks app;
    static Surface self;
    static int mode,calls,order,paints,inside,workers,afterService,syncPairs;
    static int starts,ends,releases,resourceSeen;
    static int[] resource;
    static Player player;
    public P1HostChecks(){app=this;self=new Surface();}
    protected void startApp(){starts++;Display.getDisplay(this).setCurrent(self);}
    protected void pauseApp(){}
    protected void destroyApp(boolean force){}
    static final class Surface extends Canvas {
        protected void paint(Graphics g){
            if(inside!=0)throw new RuntimeException("recursive paint");inside++;
            if(mode==1){float f=0.2f;for(int i=0;i<500000;i++)f=(f+0.007f)*0.99f;resourceSeen=Float.floatToRawIntBits(f);}
            paints++;
            if(resource!=null){resource[0]++;resourceSeen=resource[0];}
            g.setColor(paints);g.fillRect(0,0,240,320);
            if(mode==1)repaint();
            inside--;
        }
    }
    static void syncResource(int id){
        resource=new int[]{id};int before=paints;
        self.repaint();self.serviceRepaints();
        if(paints!=before+1||inside!=0||resource[0]!=id+1||resourceSeen!=id+1)
            throw new RuntimeException("resource read before synchronous completion");
        resource[0]=id+10;self.repaint();self.serviceRepaints();
        if(paints!=before+2||resource[0]!=id+11||resourceSeen!=id+11)
            throw new RuntimeException("resource write reordered across synchronous paint");
        resource=null;releases++;syncPairs++;
    }
    static final class Callback implements Runnable {
        final int id;
        Callback(int value){id=value;}
        public void run(){
            if(id!=calls+1)throw new RuntimeException("serial FIFO violation");
            calls++;order=order*10+id;
            if(mode==2)syncResource(id*100);
            if(mode==0&&(id==3||id==6))self.repaint();
            if(mode==1)self.repaint();
            if(id<6)Display.getDisplay(app).callSerially(new Callback(id+1));
        }
    }
    public static int setup(int which){
        mode=which;calls=order=paints=inside=workers=afterService=syncPairs=releases=resourceSeen=ends=0;
        resource=null;Display.getDisplay(app).setCurrent(self);
        Display.getDisplay(app).callSerially(new Callback(1));return 1;
    }
    public static int startWorker(){new Thread(){public void run(){
        for(int i=0;i<6;i++){
            workers++;
            if(mode==2)syncResource(1000+i*100);
            else {int before=paints;self.repaint();self.serviceRepaints();
                if(paints<before+1||inside!=0)throw new RuntimeException("worker resumed before paint");}
            afterService++;Thread.yield();
        }
    }}.start();return 1;}
    public static int startAudio() throws Exception {
        byte[] b=new byte[]{'R','I','F','F',38,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,
            1,0,1,0,64,31,0,0,(byte)128,62,0,0,2,0,16,0,'d','a','t','a',2,0,0,0,1,0};
        player=Manager.createPlayer(new ByteArrayInputStream(b),"audio/wav");
        player.addPlayerListener(new PlayerListener(){public void playerUpdate(Player p,String event,Object data){
            if(event.equals(PlayerListener.END_OF_MEDIA))ends++;
        }});player.start();return 1;
    }
    public static int resume(){app.resumeRequest();return 1;}
}
