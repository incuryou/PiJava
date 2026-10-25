package javax.microedition.lcdui;
public class StringItem extends Item {
    String text;
    public StringItem(String label, String text) {
        this.label = label;
        this.text = text;
    }
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
