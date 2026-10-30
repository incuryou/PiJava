package javax.microedition.lcdui;
public class StringItem extends Item {
    String text;
    private int appearanceMode;
    private Font font;
    public StringItem(String label, String text) {
        this(label, text, PLAIN);
    }
    public StringItem(String label, String text, int appearanceMode) {
        super(label);
        if (appearanceMode != PLAIN && appearanceMode != HYPERLINK && appearanceMode != BUTTON) throw new IllegalArgumentException();
        this.text = text;
        this.appearanceMode = appearanceMode;
    }
    public int getAppearanceMode() { return appearanceMode; }
    public Font getFont() { return font == null ? Font.getDefaultFont() : font; }
    public void setFont(Font value) { font = value; changed(); }
    public String getText() { return text; }
    public void setText(String value) { text = value; changed(); }
    public String labelText() {
        String a = label == null ? "" : label;
        String b = text == null ? "" : text;
        if (a.length() == 0) return b;
        if (b.length() == 0) return a;
        return a + " " + b;
    }
}
