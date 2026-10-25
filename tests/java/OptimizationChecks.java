import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;
public class OptimizationChecks {
    static int calls;
    static int recursive(int n) { calls++;return n==0?1:n+recursive(n-1); }
    static int leaf(int n) { return (n*7)^0x233; }
    static void require(boolean b) { if(!b)throw new RuntimeException("optimization regression"); }
    static class CustomInput extends java.io.InputStream {
        int value;
        public int read() { return ++value; }
    }
    public static int fallbackStream() throws java.io.IOException {
        java.io.DataInputStream input=new java.io.DataInputStream(new CustomInput());
        return input.readInt();
    }
    public static int run() {
        calls=0;int result=0;
        for(int i=0;i<1000;i++)result+=recursive(7)+leaf(i);
        require(calls==8000);
        Image image=Image.createImage(17,13);Graphics g=image.getGraphics();
        g.setColor(0x123456);g.fillRect(0,0,17,13);
        g.translate(3,4);g.setClip(0,0,7,5);g.setColor(12,34,56);g.fillRect(-100,-100,300,300);
        g.clipRect(1,1,3,2);g.setColor(255,255,255);g.fillRect(0,0,100,100);
        int[] data=new int[17*13];image.getRGB(data,0,17,0,0,17,13);
        for(int y=0;y<13;y++)for(int x=0;x<17;x++) {
            int expected=0xff123456;
            if(x>=3&&x<10&&y>=4&&y<9)expected=0xff0c2238;
            if(x>=4&&x<7&&y>=5&&y<7)expected=0xffffffff;
            require(data[y*17+x]==expected);
        }
        boolean caught=false;try{g.setColor(-1,0,0);}catch(IllegalArgumentException e){caught=true;}require(caught);
        require(g.getColor()==0xffffff);
        g.setClip(0,0,-1,3);require(g.getClipWidth()==0);
        g.translate(2147483647,0);g.translate(1,0);require(g.getTranslateX()==-2147483645);
        String s="abcdefghij";require(s.substring(2,7).equals("cdefg"));require(s.endsWith("hij"));
        StringBuffer b=new StringBuffer("seed");b.append(b);require(b.toString().equals("seedseed"));
        return result;
    }
}
