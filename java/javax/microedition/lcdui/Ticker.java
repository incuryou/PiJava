package javax.microedition.lcdui;
public class Ticker {
    String text;
    public Ticker(String text) { if (text == null) throw new NullPointerException(); this.text = text; }
    public void setString(String text) { if (text == null) throw new NullPointerException(); this.text = text; Display display = Display.getDisplay(null); Displayable current = display == null ? null : display.getCurrent(); if (current != null && current.getTicker() == this) current.invalidate(); }
    public String getString() { return text; }
}
