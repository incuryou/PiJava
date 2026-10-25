public class VmChecks {
    static int checks;
    static void eq(boolean ok) { checks++; if (!ok) throw new RuntimeException("VM check " + checks); }
    static class Parent { static long wide; static int value = 17; long zero; double dzero; }
    static class Child extends Parent { }
    static double fromInt(int v) { return v; }
    static double fromLong(long v) { return v; }
    static double fromFloat(float v) { return v; }
    static float fromDouble(double v) { return (float)v; }
    static int asInt(double v) { return (int)v; }
    static long asLong(double v) { return (long)v; }
    static int overflow(int a,int b) { return a+b; }
    static long product(long a,long b) { return a*b; }
    static int div(int a,int b) { return a/b; }
    static long div(long a,long b) { return a/b; }
    static int rem(int a,int b) { return a%b; }
    static long rem(long a,long b) { return a%b; }
    public static int run() {
        eq(fromInt(-12)==-12.0); eq(fromLong(1234567890123L)==1234567890123.0);
        eq(fromFloat(1.25f)==1.25); eq(fromDouble(-3.5)==-3.5f);
        double nan=Double.longBitsToDouble(0x7ff0000000000001L);
        eq(asInt(nan)==0);eq(asLong(nan)==0);eq(asInt(1e200)==2147483647);eq(asLong(1e200)==9223372036854775807L);
        eq(asInt(-1e200)==-2147483648);eq(asLong(-1e200)==-9223372036854775808L);
        eq(overflow(2147483647,1)==-2147483648); eq(product(9223372036854775807L,2L)==-2L);
        eq(div(-2147483648,-1)==-2147483648);eq(rem(-2147483648,-1)==0);
        eq(div(-9223372036854775808L,-1L)==-9223372036854775808L);eq(rem(-9223372036854775808L,-1L)==0);
        eq(new Child().zero==0L);eq(new Child().dzero==0.0);eq(Child.wide==0L);
        eq(Child.value==17);Child.value=29;eq(Parent.value==29);
        long[] longs=new long[2]; double[] doubles=new double[2]; eq(longs[0]==0L);eq(doubles[0]==0.0);
        longs[0]++;eq(longs[0]==1L); // wide array dup stack patterns
        Object a = new int[2];eq(a instanceof Object);eq(a instanceof int[]);eq(!(a instanceof byte[]));eq(!(a instanceof Object[]));
        Object b = new String[2][];eq(b instanceof Object[]);eq(b instanceof String[][]);
        try { Object[] t = new String[1];t[0]=new Object();eq(false); } catch(ArrayStoreException e) {eq(true);}
        try { int[][] t=new int[2][-1];eq(false); } catch(NegativeArraySizeException e) {eq(true);}
        int[] q={1,2,3,4};System.arraycopy(q,0,q,1,3);eq(q[1]==1&&q[2]==2&&q[3]==3);
        try {System.arraycopy(null,0,q,0,0);eq(false);}catch(NullPointerException e){eq(true);}
        try {System.arraycopy(q,0,new long[4],0,0);eq(false);}catch(ArrayStoreException e){eq(true);}
        try {System.arraycopy(q,2147483647,q,0,1);eq(false);}catch(IndexOutOfBoundsException e){eq(true);}
        Object[] src={"ok",new Object()};String[] dst=new String[2];
        try {System.arraycopy(src,0,dst,0,2);eq(false);}catch(ArrayStoreException e){eq(dst[0].equals("ok")&&dst[1]==null);}
        eq(((Object)q).getClass().getName().equals("[I"));
        eq(Float.floatToIntBits(Float.intBitsToFloat(0x7f800001))==0x7fc00000);
        eq(Double.doubleToLongBits(nan)==0x7ff8000000000000L);
        try {throw null;}catch(NullPointerException e){eq(true);}
        return checks;
    }
}
