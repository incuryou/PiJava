import javax.microedition.lcdui.*;
import com.nokia.mid.ui.*;
public class Compat236Checks {
 static int checks;
 static void check(boolean b,String why){checks++;if(!b)throw new RuntimeException(why);}
 public static int run()throws Exception {
  checks=0;
  check(new String().length()==0,"empty string");
  check(new String(new StringBuffer("ab").append("cd")).equals("abcd"),"StringBuffer constructor");
  check(String.valueOf(new char[]{'a','b','c'},1,2).equals("bc"),"char array valueOf");
  check(String.copyValueOf(new char[]{'x','y'}).equals("xy"),"char array copyValueOf");
  check("abcabc".lastIndexOf('a',3)==3&&"abcabc".lastIndexOf('a',2)==0,"lastIndexOf int offset");
  check("abcabc".lastIndexOf("bc",3)==1&&"abcabc".lastIndexOf("",100)==6,"lastIndexOf string offset");
  check("abc".startsWith("bc",1)&&!"abc".startsWith("",4),"startsWith offset");
  boolean bad=false;try{"abc".endsWith(null);}catch(NullPointerException e){bad=true;}check(bad,"endsWith null");
  check(Byte.parseByte("7f",16)==127&&Short.parseShort("-8000",16)==-32768,"radix parse");
  bad=false;try{Byte.parseByte("128");}catch(NumberFormatException e){bad=true;}check(bad,"byte overflow");
  check(Float.parseFloat(" -1.25f ")==-1.25f&&Double.parseDouble("0x1.8p1")==3.0,"floating syntax");
  check(new Float(2.5f).floatValue()==2.5f&&new Double("1.5").doubleValue()==1.5,"floating boxed values");
  check(Float.isNaN(Float.parseFloat("NaN"))&&Double.isInfinite(Double.parseDouble("Infinity")),"floating specials");
  bad=false;try{Double.parseDouble("nan(payload)");}catch(NumberFormatException e){bad=true;}check(bad,"non-Java float rejected");
  Graphics g=Image.createImage(7,5).getGraphics();g.setGrayScale(99);check(g.getGrayScale()==99&&g.getRedComponent()==99&&g.getBlueComponent()==99,"gray components");
  Font f=Font.getFont(Font.FONT_STATIC_TEXT);check(f!=null&&f.isPlain(),"font selection");
  Image original=Image.createRGBImage(new int[]{0xffff0000,0xff00ff00,0xff0000ff,0xffffffff,0xff000000,0xff777777},2,3,false);
  Image out=Image.createImage(3,2);DirectGraphics dg=DirectUtils.getDirectGraphics(out.getGraphics());dg.drawImage(original,0,0,Graphics.TOP|Graphics.LEFT,DirectGraphics.ROTATE_90);
  int[] px=new int[6];out.getRGB(px,0,3,0,0,3,2);
  check(px[0]==0xff00ff00&&px[1]==0xffffffff&&px[2]==0xff777777&&px[3]==0xffff0000,"Nokia rotation 90 CCW");
  dg.getPixels(px,0,3,0,0,3,2,DirectGraphics.TYPE_INT_8888_ARGB);check(px[0]==0xff00ff00,"official getPixels overload");
  Image cp=Image.createImage(6,2);Graphics cg=cp.getGraphics();cg.setColor(0xff0000);cg.fillRect(0,0,2,2);cg.copyArea(0,0,2,2,3,0,Graphics.TOP|Graphics.LEFT);int[] row=new int[6];cp.getRGB(row,0,6,0,0,6,1);check(row[0]==row[3]&&row[3]==0xffff0000,"copyArea");
  bad=false;try{javax.bluetooth.LocalDevice.getLocalDevice();}catch(javax.bluetooth.BluetoothStateException e){bad=true;}check(bad,"Bluetooth capability refusal not fake success");
  byte[] midi={'M','T','h','d',0,0,0,6,0,0,0,1,0,96,'M','T','r','k',0,0,0,12,0,(byte)0x90,60,100,96,(byte)0x80,60,0,0,(byte)0xff,47,0};
  com.samsung.util.AudioClip clip=new com.samsung.util.AudioClip(3,midi,0,midi.length);
  clip.play(1,3);check(clip.isRunning(),"Samsung real MIDI start");
  clip.pause();check(!clip.isRunning(),"Samsung pause");clip.resume();check(clip.isRunning(),"Samsung resume");
  clip.stop();check(!clip.isRunning(),"Samsung stop");clip.resume();check(!clip.isRunning(),"Samsung resume after stop no-op");
  bad=false;try{clip.play(1,6);}catch(IllegalArgumentException e){bad=true;}check(bad,"Samsung volume range");
  clip.play(255,5);check(clip.isRunning(),"Samsung looping");clip.close();check(!clip.isRunning(),"Samsung close");
  bad=false;try{new com.samsung.util.AudioClip(1,midi,0,midi.length);}catch(IllegalArgumentException e){bad=true;}check(bad,"Samsung MMF explicit unsupported");
  return checks;
 }
}
