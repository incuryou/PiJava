import javax.microedition.lcdui.*;
import com.nokia.mid.ui.*;
public class NokiaPixels236Checks {
 static int checks;
 static void check(boolean b,String s){checks++;if(!b)throw new RuntimeException(s);}
 static int[] pixels(Image im){int[] p=new int[im.getWidth()*im.getHeight()];im.getRGB(p,0,im.getWidth(),0,0,im.getWidth(),im.getHeight());return p;}
 public static int run()throws Exception {
  Image transparent=DirectUtils.createImage(8,9,0x00334455);check(transparent.isMutable(),"mutable image");
  Graphics g=transparent.getGraphics();DirectGraphics d=DirectUtils.getDirectGraphics(g);
  check(d.getAlphaComponent()==255,"default opaque");
  d.setARGBColor(0x80ff0000);g.fillRect(0,0,1,1);check(pixels(transparent)[0]==0x80ff0000,"source over transparent");
  d.setARGBColor(0x800000ff);g.fillRect(0,0,1,1);int p=pixels(transparent)[0];check((p>>>24)==192&&(p&255)>=169&&((p>>>16)&255)<=86,"straight alpha accumulation");
  g.setColor(0x112233);check(d.getAlphaComponent()==255,"setColor resets alpha");g.drawLine(0,1,7,1);check(pixels(transparent)[8]==0xff112233,"opaque line updates target alpha");
  d.setARGBColor(0x33445566);d.fillTriangle(0,2,4,2,0,6,0xff00ff00);check(g.getColor()==0x445566&&d.getAlphaComponent()==0x33,"triangle preserves active color");
  Image mask=DirectUtils.createImage(6,6,0x00000000);DirectGraphics md=DirectUtils.getDirectGraphics(mask.getGraphics());
  md.fillPolygon(new int[]{0,5,5,2,2,0},0,new int[]{0,0,2,2,5,5},0,6,0xffabcdef);int[] mp=pixels(mask);check(mp[1*6+4]==0xffabcdef&&mp[4*6+1]==0xffabcdef&&mp[4*6+4]==0,"concave polygon");
  // Nokia published 9x9 packing example, including unused bits preservation.
  Image mono=Image.createImage(9,9);int[] src=new int[81];
  for(int y=0;y<9;y++)for(int x=0;x<9;x++)src[y*9+x]=(x<2||x>6||(y>0&&y<7&&x==y+1))?0xffffffff:0xff000000;
  mono.getGraphics().drawRGB(src,0,9,0,0,9,9,false);DirectGraphics m=DirectUtils.getDirectGraphics(mono.getGraphics());
  byte[] vertical=new byte[18];for(int i=9;i<18;i++)vertical[i]=(byte)0xfe;
  m.getPixels(vertical,null,0,9,0,0,9,9,-1);
  check((vertical[0]&255)==255&&(vertical[1]&255)==255&&(vertical[9]&255)==255&&(vertical[11]&255)==254,"vertical bit order and padding");
  Image restored=Image.createImage(9,9);DirectUtils.getDirectGraphics(restored.getGraphics()).drawPixels(vertical,null,0,9,0,0,9,9,0,-1);int[] got=pixels(restored);
  for(int i=0;i<81;i++)check(src[i]==got[i],"vertical roundtrip");
  int[] formats={1,2,4,8,332};
  for(int f=0;f<formats.length;f++) {
   int format=formats[f],bits=format==332?8:format;byte[] data=new byte[256],alpha=new byte[256];
   for(int i=0;i<256;i++){data[i]=(byte)0xa5;alpha[i]=(byte)0x5a;}
   m.getPixels(data,alpha,3,12,0,0,9,9,format);
   Image ri=Image.createImage(9,9);DirectUtils.getDirectGraphics(ri.getGraphics()).drawPixels(data,alpha,3,12,0,0,9,9,0,format);got=pixels(ri);
   for(int i=0;i<81;i++)check(got[i]==src[i],"packed offset-stride roundtrip");
   byte original=data[0];boolean bad=false;try{m.getPixels(data,alpha,-1,12,0,0,9,9,format);}catch(ArrayIndexOutOfBoundsException e){bad=true;}
   check(bad&&data[0]==original,"bad get leaves buffer unchanged");
  }
  byte[] negative=new byte[90];m.getPixels(negative,null,80,-9,0,0,9,9,8);Image ni=Image.createImage(9,9);DirectUtils.getDirectGraphics(ni.getGraphics()).drawPixels(negative,null,80,-9,0,0,9,9,0,8);got=pixels(ni);for(int i=0;i<81;i++)check(got[i]==src[i],"negative scan");
  int[] sf={444,555,565,4444,1555};for(int f=0;f<sf.length;f++) {
   short[] b=new short[100];m.getPixels(b,7,10,0,0,9,9,sf[f]);Image ri=Image.createImage(9,9);DirectUtils.getDirectGraphics(ri.getGraphics()).drawPixels(b,true,7,10,0,0,9,9,0,sf[f]);got=pixels(ri);for(int i=0;i<81;i++)check(got[i]==src[i],"short format roundtrip");
  }
  return checks;
 }
}
