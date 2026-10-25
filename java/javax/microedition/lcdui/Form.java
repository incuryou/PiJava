package javax.microedition.lcdui;
public class Form extends Screen {
    Item[] items;
    int count;
    int focus;
    int cursor;
    private int top;
    private ItemStateListener stateListener;
    public Form(String title) { setTitle(title); items = new Item[8]; }
    public Form(String title, Item[] initialItems) {
        this(title);
        if (initialItems != null) {
            for (int i = 0; i < initialItems.length; i++) {
                checkItem(initialItems[i]);
                for (int j = 0; j < i; j++) if (initialItems[i] == initialItems[j]) throw new IllegalStateException();
            }
            for (int i = 0; i < initialItems.length; i++) append(initialItems[i]);
        }
    }
    private void checkItem(Item item) { if (item == null) throw new NullPointerException(); if (item.owner != null) throw new IllegalStateException(); }
    private void index(int index) { if (index < 0 || index >= count) throw new IndexOutOfBoundsException(); }
    private void capacity() { if (count == items.length) { Item[] next = new Item[items.length * 2]; System.arraycopy(items, 0, next, 0, count); items = next; } }
    public int append(String text) { if (text == null) throw new NullPointerException(); return append(new StringItem(null, text)); }
    public int append(Image image) { if (image == null) throw new NullPointerException(); return append(new ImageItem(null, image, Item.LAYOUT_DEFAULT, null)); }
    public int append(Item item) { insert(count, item); return count - 1; }
    public void insert(int index, Item item) {
        if (index < 0 || index > count) throw new IndexOutOfBoundsException();
        checkItem(item); capacity(); System.arraycopy(items, index, items, index + 1, count - index);
        items[index] = item; item.owner = this; count++;
        if (index <= focus && count > 1) focus++; invalidate();
    }
    public void set(int index, Item item) {
        index(index); checkItem(item); items[index].owner = null; items[index] = item; item.owner = this; invalidate();
    }
    public Item get(int index) { index(index); return items[index]; }
    public void delete(int index) {
        index(index); items[index].owner = null;
        System.arraycopy(items, index + 1, items, index, count - index - 1); items[--count] = null;
        if (index < focus || focus >= count) focus--; if (focus < 0) focus = 0; invalidate();
    }
    public void deleteAll() { while (count > 0) delete(count - 1); top = 0; cursor = 0; }
    public int size() { return count; }
    public void setItemStateListener(ItemStateListener listener) { stateListener = listener; }
    void itemChanged(Item item) { if (stateListener != null) stateListener.itemStateChanged(item); }
    public void paint(Graphics g) {
        paintFrame(g, "FORM");
        int visible = (getHeight() - 78) / 24; if (visible < 1) visible = 1;
        if (focus < top) top = focus;
        if (focus >= top + visible) top = focus - visible + 1;
        for (int i = top; i < count && i < top + visible; i++) {
            int y = 46 + (i - top) * 24;
            if (i == focus) { g.setColor(0x1E3A34); g.fillRect(8, y - 2, getWidth() - 16, 24); }
            g.setColor(i == focus ? 0xF4E2B0 : 0xF2F2F2);
            g.drawString(items[i].labelText(), 12, y, Graphics.TOP | Graphics.LEFT);
        }
        paintCommands(g);
    }
    public void keyPressed(int code) {
        if (handleCommandMenu(code)) return;
        Item item = count > 0 ? items[focus] : null;
        if (code == -6 || code == -7) { dispatchCommand(code); return; }
        if (item instanceof TextField && (code >= 48 && code <= 57 || code == 42 || code == 35 || code == -8 || code == 127)) {
            if (((TextField)item).editKey(code)) itemChanged(item); return;
        }
        if (code == 1 || code == 50 || code == -1) { if (focus > 0) { focus--; cursor = 0; } }
        else if (code == 6 || code == 56 || code == -2) { if (focus + 1 < count) { focus++; cursor = 0; } }
        else if (code == 2 || code == 52 || code == -3 || code == 5 || code == 54 || code == -4) {
            int dir = code == 5 || code == 54 || code == -4 ? 1 : -1;
            if (item instanceof ChoiceGroup) { if (((ChoiceGroup)item).move(dir)) itemChanged(item); }
            else if (item instanceof Gauge && ((Gauge)item).isInteractive()) { Gauge gauge = (Gauge)item; int old = gauge.getValue(); gauge.setValue(old + dir); if (old != gauge.getValue()) itemChanged(item); }
            else if (item instanceof TextField) ((TextField)item).editKey(dir > 0 ? -4 : -3);
        } else if (code == 8 || code == 53 || code == -5) {
            if (item != null && item.activate()) return;
            if (item instanceof ChoiceGroup) { if (((ChoiceGroup)item).toggle()) itemChanged(item); }
            else dispatchCommand(code);
        }
    }
}
