import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;
public class GraphicsImageChecks {
    private static int checks;
    private static void check(boolean ok) {
        if (!ok) throw new RuntimeException("graphics check " + checks);
        checks++;
    }
    private static int pixel(Image image, int x, int y) {
        int[] rgb = new int[1];image.getRGB(rgb,0,1,x,y,1,1);return rgb[0];
    }
    public static int run() {
        Image dst=Image.createImage(10,8);Graphics g=dst.getGraphics();
        check(dst.isMutable());
        check(pixel(dst,0,0)==0xffffffff);
        check(g.getColor()==0);
        check(g.getClipWidth()==10&&g.getClipHeight()==8);
        g.translate(2,3);
        check(g.getClipX()==-2&&g.getClipY()==-3);
        g.setClip(-100,-100,500,500);
        check(g.getClipX()==-2&&g.getClipY()==-3&&g.getClipWidth()==10&&g.getClipHeight()==8);
        g.clipRect(0,0,4,3);
        check(g.getClipX()==0&&g.getClipY()==0&&g.getClipWidth()==4&&g.getClipHeight()==3);
        g.setClip(0,0,-1,4);g.fillRect(0,0,10,10);
        check(g.getClipWidth()==0&&pixel(dst,2,3)==0xffffffff);
        g.translate(-2,-3);g.setClip(0,0,10,8);
        g.drawRect(1,1,2,2);
        check(pixel(dst,3,3)==0xff000000&&pixel(dst,2,2)==0xffffffff);
        g.drawRect(5,5,0,0);check(pixel(dst,5,5)==0xff000000);
        g.fillRect(0,0,0,8);check(pixel(dst,0,0)==0xffffffff);
        g.fillRect(0,0,-1,8);check(pixel(dst,0,0)==0xffffffff);
        g.setColor(255,0,0);g.fillArc(0,0,8,8,0,90);
        check(pixel(dst,5,2)==0xffff0000&&pixel(dst,2,5)!=0xffff0000);
        g.setColor(0,255,0);g.fillArc(0,0,8,8,0,-90);
        check(pixel(dst,5,5)==0xff00ff00&&pixel(dst,2,2)==0xffffffff);
        Image round=Image.createImage(10,10);Graphics r=round.getGraphics();
        r.fillRoundRect(0,0,10,10,8,8);
        check(pixel(round,0,0)==0xffffffff&&pixel(round,5,5)==0xff000000);
        r.setColor(255,0,0);r.drawArc(0,0,8,8,0,360);
        check(pixel(round,8,4)==0xffff0000&&pixel(round,4,4)==0xff000000);
        r.setStrokeStyle(Graphics.DOTTED);check(r.getStrokeStyle()==Graphics.DOTTED);
        r.drawLine(0,9,9,9);
        check(pixel(round,0,9)==0xffff0000&&pixel(round,1,9)!=0xffff0000);
        boolean caught=false;try {r.setStrokeStyle(2);}catch(IllegalArgumentException e){caught=true;}check(caught);
        int[] source={0x80ff0000,0x0000ff00,0xff0000ff,0xff00ff00};
        Image rgb=Image.createRGBImage(source,2,2,true);
        check(!rgb.isMutable());check(pixel(rgb,0,0)==source[0]);check(pixel(rgb,1,0)==source[1]);
        caught=false;try {rgb.getGraphics();}catch(IllegalStateException e){caught=true;}check(caught);
        Image copy=Image.createImage(rgb);check(!copy.isMutable()&&pixel(copy,0,0)==source[0]);
        Image trans=Image.createImage(rgb,0,0,2,2,5);check(pixel(trans,1,0)==source[0]&&pixel(trans,0,1)==source[3]);
        g.setColor(0,0,255);g.fillRect(0,0,10,8);
        g.drawRGB(source,0,2,0,0,2,2,true);
        check(pixel(dst,0,0)==0xff80007f&&pixel(dst,1,0)==0xff0000ff);
        g.drawRGB(source,2,-2,2,0,2,2,false);
        check(pixel(dst,2,0)==0xff0000ff&&pixel(dst,2,1)==0xffff0000);
        g.drawRGB(source,0,0,4,0,2,2,false);
        check(pixel(dst,4,1)==0xffff0000&&pixel(dst,5,1)==0xff00ff00);
        int[] reverse=new int[4];rgb.getRGB(reverse,2,-2,0,0,2,2);
        check(reverse[2]==source[0]&&reverse[0]==source[2]);
        caught=false;try {g.drawRGB(null,0,1,0,0,1,1,false);}catch(NullPointerException e){caught=true;}check(caught);
        caught=false;try {g.drawRGB(source,3,2,0,0,2,2,false);}catch(ArrayIndexOutOfBoundsException e){caught=true;}check(caught);
        check(pixel(dst,0,0)==0xff80007f);
        int[] untouched={99,98,97,96};caught=false;
        try {rgb.getRGB(untouched,3,2,0,0,2,2);}catch(ArrayIndexOutOfBoundsException e){caught=true;}
        check(caught&&untouched[0]==99&&untouched[3]==96);
        caught=false;try {rgb.getRGB(untouched,0,1,0,0,2,2);}catch(IllegalArgumentException e){caught=true;}check(caught);
        caught=false;try {Image.createImage(0,1);}catch(IllegalArgumentException e){caught=true;}check(caught);
        caught=false;try {Image.createRGBImage(source,3,2,false);}catch(ArrayIndexOutOfBoundsException e){caught=true;}check(caught);
        caught=false;try {Image.createImage((Image)null);}catch(NullPointerException e){caught=true;}check(caught);
        caught=false;try {g.drawImage(rgb,0,0,Graphics.BASELINE|Graphics.LEFT);}catch(IllegalArgumentException e){caught=true;}check(caught);
        caught=false;try {g.drawRegion(rgb,-1,0,1,1,0,0,0,0);}catch(IllegalArgumentException e){caught=true;}check(caught);
        caught=false;try {Image.createImage(new byte[]{1,2,3},0,3);}catch(IllegalArgumentException e){caught=true;}check(caught);
        g.setColor(0xffffff);g.drawLine(Integer.MIN_VALUE,7,Integer.MAX_VALUE,7);
        check(pixel(dst,0,7)==0xffffffff&&pixel(dst,9,7)==0xffffffff);
        g.setColor(0);g.fillTriangle(0,0,9,0,0,7);check(pixel(dst,0,7)==0xff000000&&pixel(dst,1,1)==0xff000000);
        return checks;
    }
}
