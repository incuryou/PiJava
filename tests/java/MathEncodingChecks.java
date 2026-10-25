import java.io.UnsupportedEncodingException;
public class MathEncodingChecks {
    static int checks;
    static void check(boolean ok, String label) { checks++; if (!ok) throw new RuntimeException(label); }
    static void near(double got, double want, double tolerance, String label) {
        check(got == got && Math.abs(got - want) <= tolerance, label);
    }
    static boolean nan(double d) { return d != d; }
    static void bytes(byte[] actual, int[] expected, String label) {
        check(actual.length == expected.length, label + " length");
        for (int i = 0; i < actual.length; i++) check((actual[i] & 255) == expected[i], label + " byte " + i);
    }
    static void math() {
        double inf = Double.longBitsToDouble(0x7ff0000000000000L);
        double nan = Double.longBitsToDouble(0x7ff8000000000000L);
        double nz = Double.longBitsToDouble(0x8000000000000000L);
        float fnan = Float.intBitsToFloat(0x7fc00000);
        float fnz = Float.intBitsToFloat(0x80000000);
        near(Math.sin(Math.PI / 6), .5, 1e-14, "sin nonzero");
        near(Math.cos(Math.PI / 3), .5, 1e-14, "cos nonzero");
        near(Math.tan(Math.PI / 4), 1, 1e-14, "tan nonzero");
        check(Math.cos(0) == 1 && Math.sin(0) == 0, "trig zero");
        check(nan(Math.sin(inf)) && nan(Math.cos(-inf)) && nan(Math.tan(nan)), "trig special values");
        check(Double.doubleToLongBits(Math.sin(nz)) == 0x8000000000000000L, "sin negative zero");
        check(Double.doubleToLongBits(Math.tan(nz)) == 0x8000000000000000L, "tan negative zero");
        check(Math.sqrt(9) == 3 && nan(Math.sqrt(-1)) && Math.sqrt(inf) == inf, "sqrt domain");
        check(Double.doubleToLongBits(Math.sqrt(nz)) == 0x8000000000000000L, "sqrt negative zero");
        near(Math.sqrt(1e300) / 1e150, 1, 1e-14, "large sqrt");
        near(Math.sqrt(1e-300) / 1e-150, 1, 1e-14, "small sqrt");
        near(Math.sqrt(Double.longBitsToDouble(1)), 2.2227587494850775e-162, 0, "subnormal sqrt");
        check(Math.ceil(-1.2) == -1 && Math.floor(-1.2) == -2 && Math.ceil(1.2) == 2 && Math.floor(1.2) == 1, "ceil floor signs");
        check(Double.doubleToLongBits(Math.ceil(-.1)) == 0x8000000000000000L, "ceil signed zero");
        check(Math.floor(inf) == inf && nan(Math.ceil(nan)), "ceil floor special values");
        near(Math.toRadians(180), Math.PI, 0, "degrees to radians");
        near(Math.toDegrees(Math.PI), 180, 0, "radians to degrees");
        check(Double.doubleToLongBits(Math.abs(nz)) == 0 && Float.floatToIntBits(Math.abs(fnz)) == 0, "abs zero sign");
        check(Math.abs(Integer.MIN_VALUE) == Integer.MIN_VALUE && Math.abs(Long.MIN_VALUE) == Long.MIN_VALUE, "abs signed overflow");
        check(nan(Math.max(nan, 1)) && nan(Math.max(1, nan)) && nan(Math.min(nan, 1)) && nan(Math.min(1, nan)), "double nan propagation");
        check(nan(Math.max(fnan, 1f)) && nan(Math.max(1f, fnan)) && nan(Math.min(fnan, 1f)) && nan(Math.min(1f, fnan)), "float nan propagation");
        double[] zeros = {0, nz};
        float[] fzeros = {0, fnz};
        for (int a = 0; a < 2; a++) for (int b = 0; b < 2; b++) {
            check(Double.doubleToLongBits(Math.max(zeros[a], zeros[b])) == (a == 1 && b == 1 ? 0x8000000000000000L : 0), "max zero pair");
            check(Double.doubleToLongBits(Math.min(zeros[a], zeros[b])) == (a == 1 || b == 1 ? 0x8000000000000000L : 0), "min zero pair");
            check(Float.floatToIntBits(Math.max(fzeros[a], fzeros[b])) == (a == 1 && b == 1 ? 0x80000000 : 0), "float max zero pair");
            check(Float.floatToIntBits(Math.min(fzeros[a], fzeros[b])) == (a == 1 || b == 1 ? 0x80000000 : 0), "float min zero pair");
        }
        check(Math.round(-1.5f) == -1 && Math.round(-.5f) == 0 && Math.round(1.5f) == 2, "round ties toward positive infinity");
        check(Math.round(Float.intBitsToFloat(0x3effffff)) == 0, "round below half");
        check(Math.round(fnan) == 0 && Math.round((float)inf) == Integer.MAX_VALUE && Math.round((float)-inf) == Integer.MIN_VALUE, "round saturation");
    }
    static void encoding() throws Exception {
        String text = "A\u0000\u00e9\u20ac\u4e2d\u6587\ud83d\ude00";
        byte[] utf = text.getBytes("uTf-8");
        bytes(utf, new int[]{65,0,195,169,226,130,172,228,184,173,230,150,135,240,159,152,128}, "UTF8");
        String decoded = new String(utf, "UTF8");
        check(decoded.equals(text) && decoded.length() == 8, "UTF8 UTF16 roundtrip");
        check(decoded.charAt(6) == '\ud83d' && decoded.charAt(7) == '\ude00', "surrogate indices");
        check(decoded.substring(6).equals("\ud83d\ude00"), "supplementary substring");
        bytes("A\u00e9\u20ac".getBytes(), new int[]{65,233,63}, "default Latin1");
        bytes("\u00e9\ud83d\ude00".getBytes("ASCII"), new int[]{63,63}, "ASCII replacement");
        bytes("\ud800X\udc00".getBytes("UTF8"), new int[]{63,88,63}, "unpaired surrogate replacement");
        bytes("中文".getBytes("GB2312"), new int[]{0xd6,0xd0,0xce,0xc4}, "GB2312");
        bytes("中文".getBytes("GBK"), new int[]{0xd6,0xd0,0xce,0xc4}, "GBK");
        check(new String("中文".getBytes("GBK"), "GBK").equals("中文"), "GBK roundtrip");
        bytes("\u30fb\u2015".getBytes("GB2312"), new int[]{0xa1,0xa4,0xa1,0xaa}, "GB2312 distinct punctuation");
        check(new String(new byte[]{(byte)0xa1,(byte)0xa4,(byte)0xa1,(byte)0xaa}, "GB2312").equals("\u30fb\u2015"), "GB2312 decode punctuation");
        bytes("\u2170".getBytes("GB2312"), new int[]{63}, "GB2312 excludes GBK extension");
        check(new String(new byte[]{(byte)0xe2,65}, "UTF-8").equals("\ufffdA"), "malformed UTF8 keeps next ASCII");
        check(new String(new byte[]{(byte)0xf0,(byte)0x9f}, "UTF8").equals("\ufffd"), "truncated UTF8 replacement");
        check(new String(new byte[]{(byte)0xff}, "ASCII").equals("\ufffd"), "ASCII invalid input");
        boolean caught = false;
        try { text.getBytes("UTF-MADEUP"); } catch (UnsupportedEncodingException e) { caught = true; }
        check(caught, "unsupported encoding rejected");
        caught = false; try { new String(utf, "GB18030"); } catch (UnsupportedEncodingException e) { caught = true; }
        check(caught, "GB18030 not falsely claimed as GBK");
        caught = false; try { text.getBytes(null); } catch (NullPointerException e) { caught = true; }
        check(caught, "null encoding rejected");
        caught = false; try { new String((byte[])null); } catch (NullPointerException e) { caught = true; }
        check(caught, "null byte constructor");
        caught = false; try { new String((String)null); } catch (NullPointerException e) { caught = true; }
        check(caught, "null copy constructor");
        caught = false; try { new String(utf, 1, Integer.MAX_VALUE, "UTF8"); } catch (IndexOutOfBoundsException e) { caught = true; }
        check(caught, "constructor overflow-safe range");
        byte[] first = text.getBytes("UTF8"); first[0] = 0;
        check(text.getBytes("UTF8")[0] == 65, "byte result independent");
        bytes("".getBytes("UTF8"), new int[0], "empty string");
    }
    public static int run() throws Exception { math(); encoding(); return checks; }
}
