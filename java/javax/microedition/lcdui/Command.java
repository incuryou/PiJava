package javax.microedition.lcdui;
public class Command {
    public static final int SCREEN = 1, BACK = 2, CANCEL = 3, OK = 4, HELP = 5, STOP = 6, EXIT = 7, ITEM = 8;
    String label;
    String longLabel;
    int commandType;
    int priority;
    public Command(String label, int commandType, int priority) { this(label, null, commandType, priority); }
    public Command(String shortLabel, String longLabel, int commandType, int priority) {
        if (shortLabel == null) throw new NullPointerException();
        if (commandType < SCREEN || commandType > ITEM) throw new IllegalArgumentException();
        label = shortLabel; this.longLabel = longLabel; this.commandType = commandType; this.priority = priority;
    }
    public String getLabel() { return label; }
    public String getLongLabel() { return longLabel; }
    public int getCommandType() { return commandType; }
    public int getPriority() { return priority; }
}
