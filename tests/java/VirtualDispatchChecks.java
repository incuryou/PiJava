import java.io.InputStream;
import java.io.ByteArrayInputStream;
public class VirtualDispatchChecks {
    interface Op { int value(int x); long wide(long x); Object object(Object x); void add(int x); }
    static class A implements Op {
        int count;
        public int value(int x){return x+7;}
        public long wide(long x){return x+17L;}
        public Object object(Object x){return x;}
        public void add(int x){count+=x;}
    }
    static class B extends A {
        public int value(int x){return x-9;}
        public long wide(long x){return x-31L;}
        public Object object(Object x){return this;}
        public void add(int x){count-=x;}
    }
    static class C extends A { public int value(int x){return x*3;} }
    static class CustomStream extends InputStream {public int read(){return 77;}}
    static int call(Op o,int x){return o.value(x);}
    static void check(boolean ok){if(!ok)throw new RuntimeException("dispatch mismatch");}
    public static int run() throws Exception {
        A a=new A();B b=new B();C c=new C();Op[] ops={a,b,c};int checks=0;
        for(int i=0;i<12000;++i){int k=i%3;Op o=ops[k];
            check(call(o,i)==(k==0?i+7:k==1?i-9:i*3));
            check(o.wide(1234567890123L)==1234567890123L+(k==1?-31L:17L));
            check(o.object(a)==(k==1?b:a));o.add(1);checks+=3;
            if(i%1000==0)System.gc();
        }
        check(a.count==4000&&b.count==-4000&&c.count==4000);
        for(int i=0;i<1000;++i){InputStream in=i%2==0?new ByteArrayInputStream(new byte[]{11}):new CustomStream();check(in.read()==(i%2==0?11:77));++checks;}
        boolean caught=false;try{call(null,1);}catch(NullPointerException e){caught=true;}check(caught);
        return checks+2;
    }
}
