package javax.microedition.lcdui;
public class Alert extends Screen {
    public static final int FOREVER = -2;
    public static final Command DISMISS_COMMAND = new Command("", Command.OK, 0);
    String text;
    Image image;
    AlertType type;
    int timeout;
    Displayable returnScreen;
    private Gauge indicator;
    private int scroll;
    public Alert(String title) { this(title, null, null, null); }
    public Alert(String title, String text, Image image, AlertType type) {
        setTitle(title); this.text = text; setImage(image); this.type = type; timeout = getDefaultTimeout(); super.addCommand(DISMISS_COMMAND);
    }
    public int getDefaultTimeout() { return FOREVER; }
    public int getTimeout() { return commandCount > 1 || ScreenText.lineCount(text, getWidth() - 24) * 18 > textHeight() ? FOREVER : timeout; }
    public void setTimeout(int value) { if (value <= 0 && value != FOREVER) throw new IllegalArgumentException(); timeout = value; invalidate(); }
    public AlertType getType() { return type; }
    public void setType(AlertType value) { type = value; invalidate(); }
    public String getString() { return text; }
    public void setString(String value) { text = value; scroll = 0; invalidate(); }
    public Image getImage() { return image; }
    public void setImage(Image value) { image = value == null ? null : Image.createImage(value); invalidate(); }
    public Gauge getIndicator() { return indicator; }
    public void setIndicator(Gauge value) {
        if (value == indicator) return;
        if (value != null && (value.isInteractive() || value.owner != null || !value.validIndicator())) throw new IllegalArgumentException();
        if (indicator != null) indicator.owner = null;
        indicator = value; if (value != null) value.owner = this; invalidate();
    }
    public void addCommand(Command command) { if (command == DISMISS_COMMAND) return; if (command == null) throw new NullPointerException(); super.removeCommand(DISMISS_COMMAND); super.addCommand(command); }
    public void removeCommand(Command command) { if (command == DISMISS_COMMAND) return; super.removeCommand(command); if (commandCount == 0) super.addCommand(DISMISS_COMMAND); }
    /** Runtime timeout entry point; follows the same listener path as dismissal. */
    public void dismiss() {
        Command command = commandCount == 0 ? DISMISS_COMMAND : commands[0];
        if (listener != null) listener.commandAction(command, this);
        else Display.getDisplay(null).setCurrent(returnScreen);
    }
    private int textTop() { return 46 + (image == null ? 0 : image.getHeight() + 8); }
    private int textHeight() { int height = getHeight() - textTop() - (indicator == null ? 32 : 58); return height < 0 ? 0 : height; }
    public void paint(Graphics g) {
        paintFrame(g, "ALERT");
        if (image != null) g.drawImage(image, getWidth() / 2, 46, Graphics.TOP | Graphics.HCENTER);
        g.setColor(0xF2F2F2); ScreenText.paint(g, text, 12, textTop(), getWidth() - 24, textHeight(), scroll);
        if (indicator != null) {
            int y = getHeight() - 56; g.setColor(0xC6A15B); g.drawRect(12, y, getWidth() - 24, 14);
            if (indicator.getMaxValue() > 0) g.fillRect(14, y + 2, (int)((long)(getWidth() - 28) * indicator.getValue() / indicator.getMaxValue()), 10);
            else g.drawString("...", 16, y, Graphics.TOP | Graphics.LEFT);
        }
        paintCommands(g);
        if (commandCount == 1 && commands[0] == DISMISS_COMMAND) { g.setColor(0xC6A15B); g.drawString("OK", 8, getHeight() - 22, Graphics.TOP | Graphics.LEFT); }
    }
    public void keyPressed(int code) {
        if (handleCommandMenu(code)) return;
        if (code == -1 || code == 1 || code == 50) { if (scroll > 0) scroll--; }
        else if (code == -2 || code == 6 || code == 56) { int lines = ScreenText.lineCount(text, getWidth() - 24); if (scroll + textHeight() / 18 < lines) scroll++; }
        else if (listener == null && (code == -6 || code == -7 || code == -5 || code == 8 || code == 53)) dismiss();
        else dispatchCommand(code);
    }
}
