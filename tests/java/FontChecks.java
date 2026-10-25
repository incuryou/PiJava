import javax.microedition.lcdui.*;
public final class FontChecks {
    static Image image;
    static Graphics graphics;
    static Font saved;
    private static void check(boolean b,String m) { if(!b)throw new RuntimeException(m); }
    public static int run() {
        int count=0;
        int[] sizes={Font.SIZE_SMALL,Font.SIZE_MEDIUM,Font.SIZE_LARGE};
        int[] faces={Font.FACE_SYSTEM,Font.FACE_MONOSPACE,Font.FACE_PROPORTIONAL};
        String s="Ag中éΩ\u0000\ud800";
        for(int i=0;i<3;i++)for(int face=0;face<3;face++)for(int style=0;style<8;style++) {
            Font f=Font.getFont(faces[face],style,sizes[i]);
            check(f.getFace()==faces[face]&&f.getStyle()==style&&f.getSize()==sizes[i],"font selection");
            check(f.getHeight()==(i==0?14:i==1?16:22)&&f.getBaselinePosition()==9+3*i,"metrics");
            int w=0;for(int c=0;c<s.length();c++)w+=f.charWidth(s.charAt(c));
            check(w==f.stringWidth(s)&&w==f.charsWidth(s.toCharArray(),0,s.length()),"advance agreement");
            check(f.substringWidth(s,1,3)==f.stringWidth(s.substring(1,4)),"substring advance");
            check(f.isPlain()==(style==0)&&f.isBold()==((style&1)!=0)&&f.isItalic()==((style&2)!=0)&&f.isUnderlined()==((style&4)!=0),"styles");
            if(face==1)check(f.charWidth('A')==f.charWidth('中'),"monospace cells");
            ++count;
        }
        Font f=Font.getDefaultFont();check(f.getHeight()==16&&f.charWidth('A')==8&&f.charWidth('中')==16,"legacy medium");
        for(int face=0;face<2;face++) {
            Font small=Font.getFont(face==0?Font.FACE_SYSTEM:Font.FACE_PROPORTIONAL,0,Font.SIZE_SMALL);
            check(small.charWidth('0')==7&&small.charWidth('0')<f.charWidth('0'),"small Latin is narrower than medium");
            check(small.stringWidth("3/20")==28&&small.charWidth('中')==14,"compact HUD and CJK advance");
            // Self-authored compact handset HUD: five 16px icons and 20
            // digit cells must leave room for gaps inside a 240px canvas.
            check(5*16+6+20*small.charWidth('0')+2*small.charWidth('0')/3<=240,"compact HUD fits logical screen");
        }
        check(f.substringWidth("x",1,0)==0&&f.charsWidth(new char[0],0,0)==0&&f.stringWidth("")==0,"empty ranges");
        boolean bad=false;try{Font.getFont(1,0,0);}catch(IllegalArgumentException e){bad=true;}check(bad,"face reject");
        bad=false;try{Font.getFont(0,8,0);}catch(IllegalArgumentException e){bad=true;}check(bad,"style reject");
        bad=false;try{Font.getFont(0,0,1);}catch(IllegalArgumentException e){bad=true;}check(bad,"size reject");
        bad=false;try{f.stringWidth(null);}catch(NullPointerException e){bad=true;}check(bad,"null string");
        bad=false;try{f.charsWidth(null,0,0);}catch(NullPointerException e){bad=true;}check(bad,"null chars");
        bad=false;try{f.substringWidth(null,-1,-1);}catch(NullPointerException e){bad=true;}check(bad,"null before bounds");
        bad=false;try{f.charsWidth(new char[1],2147483647,1);}catch(ArrayIndexOutOfBoundsException e){bad=true;}check(bad,"overflow bounds");
        bad=false;try{f.substringWidth("a",0,-1);}catch(StringIndexOutOfBoundsException e){bad=true;}check(bad,"negative range");
        bad=false;try{f.substringWidth("a",1,1);}catch(StringIndexOutOfBoundsException e){bad=true;}check(bad,"end range");
        return count;
    }
    public static Image draw(int size,int style,int face,int anchor,int x,int y) {
        image=Image.createImage(120,100);graphics=image.getGraphics();graphics.setColor(0);graphics.fillRect(0,0,120,100);
        graphics.setColor(0xffffff);graphics.setFont(Font.getFont(face,style,size));
        graphics.drawString("Ag中é",x,y,anchor);return image;
    }
    public static Image lines() {
        image=Image.createImage(100,100);graphics=image.getGraphics();graphics.setColor(0);graphics.fillRect(0,0,100,100);
        Font f=Font.getFont(0,0,8);graphics.setFont(f);graphics.setColor(0xffffff);
        graphics.drawString("中文A",10,10,20);graphics.drawString("中文A",10,10+f.getHeight(),20);
        return image;
    }
    public static Image topStroke() {
        image=Image.createImage(24,24);graphics=image.getGraphics();graphics.setColor(0);graphics.fillRect(0,0,24,24);
        graphics.setFont(Font.getDefaultFont());graphics.setColor(0xffffff);graphics.drawString("是",4,4,20);
        return image;
    }
    public static Image smallCrisp() {
        image=Image.createImage(240,40);graphics=image.getGraphics();graphics.setColor(0);graphics.fillRect(0,0,240,40);
        graphics.setFont(Font.getFont(0,0,8));graphics.setColor(0xffffff);
        graphics.drawString("喝下牛奶增加3点体力上限Ag019",0,0,20);return image;
    }
    public static Font valueCopy() {
        Font f=Font.getFont(0,0,8);graphics.setFont(f);return f;
    }
    public static int savedSetup() {
        image=Image.createImage(100,100);graphics=image.getGraphics();saved=Font.getFont(32,7,8);graphics.setFont(saved);
        return 1;
    }
    public static int savedCheck() {
        Font f=graphics.getFont();check(f.getFace()==32&&f.getStyle()==7&&f.getSize()==8&&f.getHeight()==14,"restored graphics value");
        check(saved.getSize()==8&&saved.getStyle()==7,"restored Font value");
        graphics.setColor(7);check(graphics.getFont().getSize()==8,"color preserves font");
        graphics.setFont(null);check(graphics.getFont().getSize()==0,"null defaults");return 1;
    }
    public static int legacySetup() {
        image=Image.createImage(100,100);graphics=image.getGraphics();saved=Font.getDefaultFont();return 1;
    }
    public static int legacyCheck() { return saved.getHeight()==16&&graphics.getFont().getHeight()==16?1:0; }
}
