package java.lang;
import java.io.PrintStream;
public class System {
    public static PrintStream out;
    public static PrintStream err;
    static { out = consoleStream(); err = consoleStream(); }
    private static PrintStream consoleStream() {
        try {
            return new PrintStream(new java.io.ConsoleOutputStream(), true, "UTF-8");
        } catch (java.io.UnsupportedEncodingException unsupported) {
            return new PrintStream(new java.io.ConsoleOutputStream(), true);
        }
    }
    public static native void exit(int status);
    public static native long currentTimeMillis();
    public static native void arraycopy(Object src, int srcPos, Object dest, int destPos, int length);
    public static native void gc();
    public static native int identityHashCode(Object obj);
    public static String getProperty(String key) {
        if (key == null) return null;
        if (key.equals("microedition.configuration")) return "CLDC-1.1";
        if (key.equals("microedition.profiles")) return "MIDP-2.0";
        if (key.equals("microedition.platform")) return "j2me-vm";
        if (key.equals("microedition.encoding")) return "ISO-8859-1";
        if (key.equals("microedition.locale")) return "en-US";
        if (key.equals("microedition.media.version")) return "1.0";
        if (key.equals("supports.mixing")) return "true";
        if (key.equals("microedition.m3g.version")) return "1.1";
        if (key.equals("file.separator")) return "/";
        return null;
    }
}
