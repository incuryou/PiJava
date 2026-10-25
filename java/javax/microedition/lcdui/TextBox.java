package javax.microedition.lcdui;
public class TextBox extends Screen {
    private TextField field;
    private int scroll;
    public TextBox(String title, String text, int maxSize, int constraints) { setTitle(title); field = new TextField(null, text, maxSize, constraints); field.owner = this; }
    public String getString() { return field.getString(); }
    public void setString(String value) { field.setString(value); invalidate(); }
    public int getChars(char[] data) { return field.getChars(data); }
    public void setChars(char[] data, int offset, int length) { field.setChars(data, offset, length); invalidate(); }
    public void insert(String value, int position) { field.insert(value, position); invalidate(); }
    public void insert(char[] data, int offset, int length, int position) { field.insert(data, offset, length, position); invalidate(); }
    public void delete(int offset, int length) { field.delete(offset, length); invalidate(); }
    public int getMaxSize() { return field.getMaxSize(); }
    public int setMaxSize(int value) { int result = field.setMaxSize(value); invalidate(); return result; }
    public int getConstraints() { return field.getConstraints(); }
    public void setConstraints(int value) { field.setConstraints(value); invalidate(); }
    public void setInitialInputMode(String characterSubset) { field.setInitialInputMode(characterSubset); }
    public int size() { return field.size(); }
    public int getCaretPosition() { return field.getCaretPosition(); }
    public void paint(Graphics g) {
        paintFrame(g, "TEXT"); g.setColor(0xF2F2F2);
        ScreenText.paint(g, field.displayText(), 12, 46, getWidth() - 24, getHeight() - 78, scroll);
        paintCommands(g);
    }
    public void keyPressed(int code) {
        if (handleCommandMenu(code)) return;
        if (code == -1 || code == 1) { if (scroll > 0) scroll--; }
        else if (code == -2 || code == 6) { int lines = ScreenText.lineCount(field.displayText(), getWidth() - 24); if (scroll + (getHeight() - 78) / 18 < lines) scroll++; }
        else if (code == -6 || code == -7 || code == -5) dispatchCommand(code);
        else field.editKey(code);
    }
}
