import javax.microedition.midlet.MIDlet;
import javax.microedition.lcdui.Canvas;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Graphics;
import javax.microedition.m3g.Transform;

/** Synthetic MIDlets and bounded callbacks; these classes stay in build/test-classes. */
public class P1CloseoutChecks extends MIDlet {
    static P1CloseoutChecks app;
    static Surface surface;
    public static int starts,destroys,calls,identities,workers,checksum;
    public P1CloseoutChecks(){app=this;surface=new Surface();}
    protected void startApp(){starts++;Display.getDisplay(this).setCurrent(surface);}
    protected void pauseApp(){}
    protected void destroyApp(boolean force){destroys++;}
    static final class Surface extends Canvas {
        protected void paint(Graphics g){g.setColor(0x102030);g.fillRect(0,0,4,4);}
    }
    public static int queueShort(){
        Display.getDisplay(app).callSerially(new Runnable(){public void run(){
            int sum=0;for(int i=0;i<16;i++)sum+=i;checksum=sum;calls++;
        }});return 1;
    }
    public static int queueM3G(final int count){
        Display.getDisplay(app).callSerially(new Runnable(){public void run(){
            Transform t=new Transform();float[] out=new float[16];
            for(int i=0;i<count;i++){
                t.postTranslate(1,2,3);t.setIdentity();t.get(out);
                for(int j=0;j<16;j++)if(out[j]!=(j%5==0?1.0f:0.0f))
                    throw new RuntimeException("native identity result");
                identities++;
            }
            calls++;
        }});return 1;
    }
    public static int startWorker(){new Thread(){public void run(){workers++;}}.start();return 1;}
}
class P1CloseoutOtherChecks extends P1CloseoutChecks {}
