package javax.microedition.lcdui;
public abstract class Displayable {
    protected Command[] commands;
    protected int commandCount;
    protected CommandListener listener;
    protected String title;
    protected Ticker ticker;
    private boolean commandMenu;
    private int commandSelection;
    protected Displayable() { commands = new Command[4]; }
    public native int getWidth();
    public native int getHeight();
    public void addCommand(Command cmd) {
        if (cmd == null) throw new NullPointerException();
        for (int i = 0; i < commandCount; i++) if (commands[i] == cmd) return;
        if (commandCount == commands.length) {
            Command[] next = new Command[commands.length * 2];
            System.arraycopy(commands, 0, next, 0, commandCount); commands = next;
        }
        int at = commandCount;
        while (at > 0 && commands[at - 1].getPriority() > cmd.getPriority()) { commands[at] = commands[at - 1]; at--; }
        commands[at] = cmd; commandCount++; invalidate();
    }
    public void removeCommand(Command cmd) {
        for (int i = 0; i < commandCount; i++) if (commands[i] == cmd) {
            System.arraycopy(commands, i + 1, commands, i, commandCount - i - 1);
            commands[--commandCount] = null; if (commandSelection >= commandCount) commandSelection = 0; if (commandCount <= 2) commandMenu = false; invalidate(); return;
        }
    }
    public void setCommandListener(CommandListener l) { listener = l; }
    public native boolean isShown();
    protected native void invalidate();
    public void setTitle(String value) { title = value; invalidate(); }
    public String getTitle() { return title; }
    public void setTicker(Ticker value) { ticker = value; invalidate(); }
    public Ticker getTicker() { return ticker; }
    protected Command rightCommand() {
        for (int i = 0; i < commandCount; i++) {
            int t = commands[i].getCommandType();
            if (t == Command.BACK || t == Command.CANCEL || t == Command.EXIT || t == Command.STOP) return commands[i];
        }
        return commandCount > 1 ? commands[1] : null;
    }
    protected Command leftCommand() {
        Command right = rightCommand();
        for (int i = 0; i < commandCount; i++) if (commands[i] != right) return commands[i];
        return null;
    }
    protected boolean handleCommandMenu(int code) {
        if (!commandMenu) {
            if (code == -6 && commandCount > 2) { commandMenu = true; commandSelection = 0; return true; }
            return false;
        }
        if (code == -7) commandMenu = false;
        else if (code == -1 || code == 1 || code == 50) { if (commandSelection > 0) commandSelection--; }
        else if (code == -2 || code == 6 || code == 56) { if (commandSelection + 1 < commandCount) commandSelection++; }
        else if (code == -6 || code == -5 || code == 8 || code == 53) {
            Command cmd = commands[commandSelection]; commandMenu = false;
            if (listener != null) listener.commandAction(cmd, this);
        }
        return true;
    }
    protected boolean dispatchCommand(int code) {
        if (handleCommandMenu(code)) return true;
        Command cmd = code == -7 ? rightCommand() : (code == -6 || code == -5 || code == 8 || code == 53) ? leftCommand() : null;
        if (cmd == null) return false;
        if (listener != null) listener.commandAction(cmd, this);
        return true;
    }
    protected void paintFrame(Graphics g, String fallback) {
        g.setColor(0x142033); g.fillRect(0, 0, getWidth(), getHeight());
        g.setColor(0xF4E2B0); g.drawString(title == null ? fallback : title, 12, 8, Graphics.TOP | Graphics.LEFT);
        if (ticker != null) { g.setColor(0xC6A15B); g.drawString(ticker.getString(), 12, 26, Graphics.TOP | Graphics.LEFT); }
    }
    protected void paintCommands(Graphics g) {
        if (commandMenu) {
            int visible = commandCount < 8 ? commandCount : 8;
            int first = commandSelection >= visible ? commandSelection - visible + 1 : 0;
            int y = getHeight() - 34 - visible * 22;
            g.setColor(0x203047); g.fillRect(8, y - 6, getWidth() - 16, visible * 22 + 10);
            for (int i = first; i < commandCount && i < first + visible; i++) {
                g.setColor(i == commandSelection ? 0xF4E2B0 : 0xF2F2F2);
                g.drawString(commands[i].getLabel(), 16, y + (i - first) * 22, Graphics.TOP | Graphics.LEFT);
            }
            g.setColor(0x142033); g.fillRect(0, getHeight() - 28, getWidth(), 28); g.setColor(0xC6A15B);
            g.drawString("Select", 8, getHeight() - 22, Graphics.TOP | Graphics.LEFT);
            g.drawString("Cancel", getWidth() - 8, getHeight() - 22, Graphics.TOP | Graphics.RIGHT); return;
        }
        g.setColor(0x142033); g.fillRect(0, getHeight() - 28, getWidth(), 28);
        g.setColor(0xC6A15B);
        Command left = leftCommand(), right = rightCommand();
        if (commandCount > 2) g.drawString("Commands", 8, getHeight() - 22, Graphics.TOP | Graphics.LEFT);
        else if (left != null) g.drawString(left.getLabel(), 8, getHeight() - 22, Graphics.TOP | Graphics.LEFT);
        if (right != null) g.drawString(right.getLabel(), getWidth() - 8, getHeight() - 22, Graphics.TOP | Graphics.RIGHT);
    }
    protected void keyPressed(int code) { dispatchCommand(code); }
    protected void keyReleased(int code) {}
    protected void sizeChanged(int w, int h) {}
}
