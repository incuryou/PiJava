import javax.microedition.lcdui.Image;
public class Optimization234Checks {
    static void check(boolean condition) { if(!condition)throw new RuntimeException("2.34 regression"); }
    static int seed=0x234;
    static int next() { seed=seed*1664525+1013904223;return seed; }
    static class Fields { int i;long l;Object o;static int si;static long sl;static Object so; }
    static class Init { static int value=next(); }
    static class BadInit { static int value=fail();static int fail(){throw new RuntimeException("expected init failure");} }
    static int sparse(int x) { switch(x){case -10000:return 9;case 0:return 8;case 23:return 7;case 900:return 6;default:return 5;} }
    static int dense(int x) { switch(x){case 0:return 10;case 1:return 20;case 2:return 30;case 3:return 40;default:return -10;} }
    static int recurse(int a,int b,int c,int depth) { return depth==0?a+b+c:recurse(b,c,a+depth,depth-1)+a; }
    public static int run() {
        int checks=0,zero=0,min=Integer.MIN_VALUE;
        check(min/-1==min && min%-1==0);checks++;
        check((min>>>32)==min && (-1>>>1)==2147483647 && (min>>31)==-1);checks++;
        check((char)-1==65535 && (byte)128==-128 && (short)32768==-32768);checks++;
        int[] a=new int[97];byte[] b=new byte[97];char[] c=new char[97];short[] s=new short[97];long[] l=new long[97];
        Fields f=new Fields();Object marker=new Object();int sum=0;
        for(int i=0;i<18000;i++) {
            int x=next(),y=next(),at=i%97,div=y|1;
            a[at]=x;b[at]=(byte)x;c[at]=(char)x;s[at]=(short)x;l[at]=(((long)x)<<32)|(y&0xffffffffL);
            check(a[at]==x && b[at]==(byte)x && c[at]==(char)x && s[at]==(short)x);
            check((int)l[at]==y && (int)(l[at]>>32)==x);
            f.i=x;f.l=l[at];f.o=marker;Fields.si=y;Fields.sl=~f.l;Fields.so=f;
            check(f.o==marker && Fields.so==f && (int)Fields.sl==~y);
            int r=(x*7)+(x<<y)+(x>>y)+(x>>>y)+x/div+x%div;
            r^=(x&y)|(x^y);sum+=r+f.i+Fields.si+(int)f.l;
            if((i&63)==0)sum+=recurse(i,y,x,9)+sparse(i%24)+dense(i%5);
        }
        check(Init.value!=0);checks++;
        try { sum+=1/zero;check(false); } catch(ArithmeticException expected) { checks++; }
        try { sum+=a[-1];check(false); } catch(ArrayIndexOutOfBoundsException expected) { checks++; }
        try { a[a.length]=1;check(false); } catch(ArrayIndexOutOfBoundsException expected) { checks++; }
        try { int[] nil=null;sum+=nil.length;check(false); } catch(NullPointerException expected) { checks++; }
        try { Fields nil=null;sum+=nil.i;check(false); } catch(NullPointerException expected) { checks++; }
        try { Fields nil=null;nil.i=1;check(false); } catch(NullPointerException expected) { checks++; }
        try { Object[] strings=new String[1];strings[0]=marker;check(false); } catch(ArrayStoreException expected) { checks++; }
        try { sum+=BadInit.value;check(false); } catch(ExceptionInInitializerError expected) { checks++; }
        try { sum+=BadInit.value;check(false); } catch(NoClassDefFoundError expected) { checks++; }
        check(checks==13);return sum;
    }
    public static int images() throws Exception {
        Image a=Image.createImage("/cache.png"),b=Image.createImage("/cache.png");
        check(a!=b && !a.isMutable() && !b.isMutable());
        int[] ra=new int[35],rb=new int[35];a.getRGB(ra,0,7,0,0,7,5);b.getRGB(rb,0,7,0,0,7,5);
        int sum=0;for(int i=0;i<35;i++){check(ra[i]==rb[i]);sum+=ra[i];}
        Image copy=Image.createImage(7,5);copy.getGraphics().drawImage(a,0,0,0);copy.getGraphics().setColor(123);copy.getGraphics().fillRect(0,0,7,5);
        a.getRGB(rb,0,7,0,0,7,5);for(int i=0;i<35;i++)check(ra[i]==rb[i]);
        return sum;
    }
}
