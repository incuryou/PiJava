package javax.microedition.lcdui;
public class List extends Screen implements Choice {
    public static final int IMPLICIT = 3;
    public static final Command SELECT_COMMAND = new Command("Select", Command.SCREEN, 0);
    private ChoiceData data;
    private int highlight;
    private int top;
    private Command selectCommand = SELECT_COMMAND;
    public List(String title, int listType) { this(title, listType, null, null); }
    public List(String title, int listType, String[] strings, Image[] images) {
        if (listType != EXCLUSIVE && listType != MULTIPLE && listType != IMPLICIT) throw new IllegalArgumentException();
        setTitle(title); data = new ChoiceData(listType, strings, images);
    }
    public int append(String text, Image image) { int index = data.append(text, image); invalidate(); return index; }
    public void insert(int index, String text, Image image) { data.insert(index, text, image); if (index <= highlight && data.count > 1) highlight++; invalidate(); }
    public void delete(int index) { data.delete(index); if (index < highlight || highlight >= data.count) highlight--; if (highlight < 0) highlight = 0; invalidate(); }
    public void deleteAll() { data.deleteAll(); highlight = 0; top = 0; invalidate(); }
    public void set(int index, String text, Image image) { data.set(index, text, image); invalidate(); }
    public int size() { return data.count; }
    public String getString(int index) { data.index(index); return data.rows[index]; }
    public Image getImage(int index) { data.index(index); return data.images[index]; }
    public boolean isSelected(int index) { data.index(index); return data.flags[index]; }
    public int getSelectedIndex() { return data.selectedIndex(); }
    public void setSelectedIndex(int index, boolean on) { data.setSelectedIndex(index, on); if (on) highlight = index; invalidate(); }
    public int getSelectedFlags(boolean[] flags) { return data.getSelectedFlags(flags); }
    public void setSelectedFlags(boolean[] flags) { data.setSelectedFlags(flags); if (data.type != MULTIPLE && data.count > 0) highlight = data.selectedIndex(); invalidate(); }
    public void setFitPolicy(int policy) { data.setFitPolicy(policy); invalidate(); }
    public int getFitPolicy() { return data.fitPolicy; }
    public void setFont(int index, Font font) { data.setFont(index, font); invalidate(); }
    public Font getFont(int index) { return data.getFont(index); }
    public void setSelectCommand(Command command) { if (data.type == IMPLICIT) { selectCommand = command; if (command != null && command != SELECT_COMMAND) addCommand(command); invalidate(); } }
    public void removeCommand(Command command) { super.removeCommand(command); if (command == selectCommand) selectCommand = null; }
    public void paint(Graphics g) {
        paintFrame(g, "LIST");
        int visible = (getHeight() - 76) / 22;
        if (visible < 1) visible = 1;
        if (highlight < top) top = highlight;
        if (highlight >= top + visible) top = highlight - visible + 1;
        for (int i = top; i < data.count && i < top + visible; i++) {
            int y = 46 + (i - top) * 22;
            if (i == highlight) { g.setColor(0x1E3A34); g.fillRect(8, y - 2, getWidth() - 16, 22); }
            g.setColor(i == highlight ? 0xF4E2B0 : 0xF2F2F2);
            String marker = data.type == MULTIPLE ? (data.flags[i] ? "[x] " : "[ ] ") : data.type == EXCLUSIVE ? (data.flags[i] ? "(*) " : "( ) ") : "";
            int x = 12;
            if (data.images[i] != null) { g.drawImage(data.images[i], x, y, Graphics.TOP | Graphics.LEFT); x += data.images[i].getWidth() + 4; }
            g.drawString(marker + data.rows[i], x, y, Graphics.TOP | Graphics.LEFT);
        }
        paintCommands(g);
        if (data.type == IMPLICIT && selectCommand != null && leftCommand() == null) {
            g.setColor(0xC6A15B); g.drawString(selectCommand.getLabel(), 8, getHeight() - 22, Graphics.TOP | Graphics.LEFT);
        }
    }
    public void keyPressed(int code) {
        if (handleCommandMenu(code)) return;
        if (code == 1 || code == 50 || code == -1) { if (highlight > 0) highlight--; }
        else if (code == 6 || code == 56 || code == -2) { if (highlight + 1 < data.count) highlight++; invalidate(); }
        else if (code == 8 || code == 53 || code == -5 || (code == -6 && (leftCommand() == null || leftCommand() == selectCommand))) {
            if (data.count == 0) return;
            if (data.type == MULTIPLE) data.setSelectedIndex(highlight, !data.flags[highlight]);
            else data.setSelectedIndex(highlight, true);
            if (data.type == IMPLICIT && listener != null && selectCommand != null) listener.commandAction(selectCommand, this);
        } else dispatchCommand(code);
        if (data.type == IMPLICIT && data.count > 0) data.setSelectedIndex(highlight, true);
    }
}
