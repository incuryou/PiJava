package javax.microedition.lcdui;
public class Item {
    public static final int LAYOUT_DEFAULT = 0, LAYOUT_LEFT = 1, LAYOUT_RIGHT = 2, LAYOUT_CENTER = 3;
    public static final int LAYOUT_TOP = 16, LAYOUT_BOTTOM = 32, LAYOUT_VCENTER = 48;
    public static final int LAYOUT_NEWLINE_BEFORE = 256, LAYOUT_NEWLINE_AFTER = 512, LAYOUT_SHRINK = 1024;
    public static final int LAYOUT_EXPAND = 2048, LAYOUT_VSHRINK = 4096, LAYOUT_VEXPAND = 8192, LAYOUT_2 = 16384;
    public static final int PLAIN = 0, HYPERLINK = 1, BUTTON = 2;
    String label;
    Displayable owner;
    private int layout;
    int preferredWidth = -1, preferredHeight = -1;
    private Command[] commands = new Command[4];
    private int commandCount;
    private Command defaultCommand;
    private ItemCommandListener commandListener;
    public Item() {}
    protected Item(String label) { this.label = label; }
    void changed() { if (owner != null) owner.invalidate(); }
    private void mutable() { if (owner instanceof Alert) throw new IllegalStateException(); }
    boolean validIndicator() { return label == null && commandCount == 0 && commandListener == null && preferredWidth == -1 && preferredHeight == -1 && layout == LAYOUT_DEFAULT; }
    public void setLabel(String value) { mutable(); label = value; changed(); }
    public String getLabel() { return label; }
    public void setLayout(int value) { mutable(); if ((value & ~0x7F33) != 0) throw new IllegalArgumentException(); layout = value; changed(); }
    public int getLayout() { return layout; }
    public void addCommand(Command command) {
        mutable();
        if (command == null) throw new NullPointerException();
        for (int i = 0; i < commandCount; i++) if (commands[i] == command) return;
        if (commandCount == commands.length) { Command[] n = new Command[commands.length * 2]; System.arraycopy(commands, 0, n, 0, commandCount); commands = n; }
        commands[commandCount++] = command; changed();
    }
    public void removeCommand(Command command) {
        mutable();
        for (int i = 0; i < commandCount; i++) if (commands[i] == command) { System.arraycopy(commands, i + 1, commands, i, commandCount - i - 1); commands[--commandCount] = null; break; }
        if (defaultCommand == command) defaultCommand = null; changed();
    }
    public void setDefaultCommand(Command command) { mutable(); if (command != null) addCommand(command); defaultCommand = command; changed(); }
    public void setItemCommandListener(ItemCommandListener listener) { mutable(); commandListener = listener; }
    boolean activate() { if (defaultCommand == null) return false; if (commandListener != null) commandListener.commandAction(defaultCommand, this); return true; }
    public int getMinimumWidth() { return 16; }
    public int getMinimumHeight() { return 22; }
    public int getPreferredWidth() { return preferredWidth < 0 ? 216 : preferredWidth; }
    public int getPreferredHeight() { return preferredHeight < 0 ? getMinimumHeight() : preferredHeight; }
    public void setPreferredSize(int width, int height) { mutable(); if (width < -1 || height < -1) throw new IllegalArgumentException(); preferredWidth = width; preferredHeight = height; changed(); }
    public void notifyStateChanged() { if (!(owner instanceof Form)) throw new IllegalStateException(); changed(); ((Form)owner).itemChanged(this); }
    public String labelText() { return label == null ? "" : label; }
}
