package java.io;

final class Encoding {
    static final int LATIN1 = 1;
    static final int ASCII = 2;
    static final int UTF8 = 3;
    static int find(String name) throws UnsupportedEncodingException {
        if (name == null) throw new NullPointerException();
        if (name.equalsIgnoreCase("UTF-8") || name.equalsIgnoreCase("UTF8")) return UTF8;
        if (name.equalsIgnoreCase("ISO-8859-1") || name.equalsIgnoreCase("ISO8859_1") ||
            name.equalsIgnoreCase("ISO8859-1") || name.equalsIgnoreCase("ISO_8859_1") ||
            name.equalsIgnoreCase("ISO_8859-1") || name.equalsIgnoreCase("LATIN1")) return LATIN1;
        if (name.equalsIgnoreCase("US-ASCII") || name.equalsIgnoreCase("ASCII")) return ASCII;
        throw new UnsupportedEncodingException(name);
    }
    static String name(int code) { return code == UTF8 ? "UTF8" : code == ASCII ? "ASCII" : "ISO8859_1"; }
}
