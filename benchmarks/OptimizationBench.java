import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;
public class OptimizationBench {
    static int small(int x) { return (x*7)^0x233; }
    static int depth(int x,int n) { return n==0?small(x):depth(x+n,n-1)+n; }
    public static int run(int mode,int iterations) {
        int result=0;
        if(mode==0) {
            for(int i=0;i<iterations;i++)result+=depth(i,6)+small(i+1);
        } else if(mode==1) {
            Image im=Image.createImage(96,96);Graphics g=im.getGraphics();
            for(int i=0;i<iterations;i++) {
                g.setColor(i*31);g.setClip(0,0,96,96);g.translate(1,1);
                g.clipRect(0,0,80,70);g.fillRect(-1,-1,96,96);g.translate(-1,-1);
            }
            int[] rgb=new int[96*96];im.getRGB(rgb,0,96,0,0,96,96);
            for(int i=0;i<rgb.length;i++)result+=rgb[i];
        } else if(mode==2) {
            String text="0123456789abcdefghijklmnopqrstuvwxyz";
            for(int i=0;i<5;i++)text=text+text;
            for(int i=0;i<iterations;i++) {
                result+=text.length()+text.indexOf("uvwxyz",i&15);
                if(text.endsWith("xyz"))result++;
                result+=text.charAt(i%1000);
            }
        } else if(mode==3) {
            Image im=Image.createImage(240,320);Graphics g=im.getGraphics();
            for(int i=0;i<iterations;i++){g.setColor((i&1)==0?0x505050:i);g.fillRect(0,0,240,320);}
            int[] rgb=new int[1];im.getRGB(rgb,0,1,0,0,1,1);result=rgb[0];
        }
        return result;
    }
}
