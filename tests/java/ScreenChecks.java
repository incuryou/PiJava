import javax.microedition.lcdui.*;
import javax.microedition.lcdui.game.GameCanvas;
public class ScreenChecks {
    public static Probe canvas;
    public static Display display;
    public static String events="";
    public static App app=new App();
    public static class App extends javax.microedition.midlet.MIDlet {
        protected void startApp() {} protected void pauseApp() {} protected void destroyApp(boolean b) {}
    }
    public static int changes,width,height,paints,redirect;
    public static class Probe extends Canvas {
        protected void sizeChanged(int w,int h) { ++changes;width=w;height=h;events+="s";if(redirect==1)display.setCurrent(null);else if(redirect==2)app.notifyPaused();else if(redirect==3)app.notifyDestroyed(); }
        protected void showNotify() { events+="v"; }
        protected void hideNotify() { events+="h"; }
        protected void paint(Graphics g) { ++paints;events+="p";g.setColor(0);g.fillRect(0,0,getWidth(),getHeight());g.setColor(0xff0000);g.fillRect(0,0,4,getHeight());g.setColor(0x00ff00);g.fillRect(0,0,getWidth(),4);g.setColor(0xffffff);g.drawString(getWidth()+"x"+getHeight(),8,12,20); }
    }
    public static class Game extends GameCanvas { public Game(){super(false);} }
    public static int setup(int w,int h) {
        events="";changes=width=height=paints=redirect=0;
        canvas=new Probe();Game game=new Game();Form form=new Form("screen");
        if(canvas.getWidth()!=w||canvas.getHeight()!=h||game.getWidth()!=w||game.getHeight()!=h||form.getWidth()!=w||form.getHeight()!=h)return -1;
        Graphics g=game.getGraphics();if(g.getClipWidth()!=w||g.getClipHeight()!=h)return -2;
        display=Display.getDisplay(null);display.setCurrent(canvas);return 1;
    }
    public static void toggle() { canvas.setFullScreenMode(true);canvas.setFullScreenMode(false);canvas.setFullScreenMode(true); }
    public static void hide() { display.setCurrent(null); }
    public static void show() { display.setCurrent(canvas); }
    public static int verify() { return canvas.getWidth()*1000+canvas.getHeight(); }
    public static void redirectSetup() { redirect=1;events="";display.setCurrent(canvas); }
}
