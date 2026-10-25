package javax.microedition.lcdui;
public class Font {
    public static final int STYLE_PLAIN = 0;
    public static final int STYLE_BOLD = 1;
    public static final int STYLE_ITALIC = 2;
    public static final int SIZE_SMALL = 8;
    public static final int SIZE_MEDIUM = 0;
    public static final int SIZE_LARGE = 16;
    public static final int FACE_SYSTEM = 0;
    public static final int FACE_MONOSPACE = 32;
    public static final int FACE_PROPORTIONAL = 64;
    public static final int FONT_STATIC_TEXT=0;
    public static final int FONT_INPUT_TEXT=1;
    public static final int STYLE_UNDERLINED=4;
    public static Font getFont(int specifier) {
        if(specifier!=FONT_STATIC_TEXT&&specifier!=FONT_INPUT_TEXT)throw new IllegalArgumentException();
        return getDefaultFont();
    }
    public boolean isPlain() { return getStyle()==STYLE_PLAIN; }
    public boolean isBold() { return (getStyle()&STYLE_BOLD)!=0; }
    public boolean isItalic() { return (getStyle()&STYLE_ITALIC)!=0; }
    public boolean isUnderlined() { return (getStyle()&STYLE_UNDERLINED)!=0; }
    // Native value metadata keeps the existing zero-field J2S5 class schema.
    public Font() {}
    private Font(int face, int style, int size) { init(face, style, size); }
    private native void init(int face, int style, int size);
    public static Font getDefaultFont() { return new Font(); }
    public static Font getFont(int face, int style, int size) {
        if ((face != FACE_SYSTEM && face != FACE_MONOSPACE && face != FACE_PROPORTIONAL) ||
            (style & ~7) != 0 || (size != SIZE_SMALL && size != SIZE_MEDIUM && size != SIZE_LARGE))
            throw new IllegalArgumentException();
        return new Font(face, style, size);
    }
    public native int getHeight();
    public native int getBaselinePosition();
    public native int stringWidth(String text);
    public native int charWidth(char ch);
    public int charsWidth(char[] ch, int offset, int length) {
        if (ch == null) throw new NullPointerException();
        if (offset < 0 || length < 0 || offset > ch.length - length) throw new ArrayIndexOutOfBoundsException();
        int w = 0;
        for (int i = 0; i < length; ++i) w += charWidth(ch[offset + i]);
        return w;
    }
    public native int getStyle();
    public native int getSize();
    public native int getFace();
    public int substringWidth(String text, int offset, int length) {
        if (text == null) throw new NullPointerException();
        if (offset < 0 || length < 0 || offset > text.length() - length) throw new StringIndexOutOfBoundsException();
        int w = 0;
        for (int i = 0; i < length; ++i) w += charWidth(text.charAt(offset + i));
        return w;
    }
}
