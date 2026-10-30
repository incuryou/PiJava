package javax.microedition.lcdui;
public class Form extends Screen {
    Item[] items;
    int count;
    int focus;
    int cursor; // Existing slot: current CustomItem has accepted internal traversal.
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
        index(index); checkItem(item); detach(items[index]); items[index].owner = null; items[index] = item; item.owner = this; invalidate();
    }
    public Item get(int index) { index(index); return items[index]; }
    public void delete(int index) {
        index(index); detach(items[index]); items[index].owner = null;
        System.arraycopy(items, index + 1, items, index, count - index - 1); items[--count] = null;
        if (index < focus || focus >= count) focus--; if (focus < 0) focus = 0; invalidate();
    }
    public void deleteAll() { while (count > 0) delete(count - 1); top = 0; cursor = 0; }
    public int size() { return count; }
    public void setItemStateListener(ItemStateListener listener) { stateListener = listener; }
    void itemChanged(Item item) { if (stateListener != null) stateListener.itemStateChanged(item); }
    private void detach(Item item) { if (item instanceof CustomItem) ((CustomItem)item).detach(); }
    private boolean hasCustomItems() { for (int i = 0; i < count; i++) if (items[i] instanceof CustomItem) return true; return false; }
    void focusItem(Item item) {
        for (int i = 0; i < count; i++) if (items[i] == item) { moveFocus(i, CustomItem.NONE); invalidate(); return; }
        throw new IllegalStateException();
    }
    private void moveFocus(int next, int direction) {
        if (next != focus && focus < count && items[focus] instanceof CustomItem) ((CustomItem)items[focus]).leave();
        if (next != focus) cursor = 0;
        focus = next;
        if (isShown() && count > 0 && items[focus] instanceof CustomItem) {
            layoutCustom();
            CustomItem item = (CustomItem)items[focus];
            if (!item.entered) traverseItem(item, direction);
        }
    }
    private int viewportHeight() { return Math.max(1, getHeight() - 78); }
    private void layoutCustom() {
        int y = 0;
        for (int i = 0; i < count; i++) {
            if (items[i] instanceof CustomItem) {
                CustomItem item = (CustomItem)items[i]; item.layout(Math.max(1, getWidth() - 24));
                item.contentY = y + item.labelHeight(); y += item.labelHeight() + item.contentHeight + 4;
            } else y += 24;
        }
    }
    private void reveal(int y, int height) {
        int viewport = viewportHeight();
        if (y < top) top = y;
        else if (y + height > top + viewport) top = height > viewport ? y : y + height - viewport;
        if (top < 0) top = 0;
    }
    private boolean traverseItem(CustomItem item, int direction) {
        boolean entering = !item.entered;
        boolean relayout = item.layoutInvalid;
        if (entering) cursor = 0;
        int[] rect = item.traversal;
        rect[0] = 0; rect[1] = Math.max(0, top - item.contentY);
        rect[2] = item.contentWidth; rect[3] = Math.max(0, Math.min(item.contentHeight - rect[1], viewportHeight()));
        item.entered = true; item.layoutInvalid = false;
        boolean inside = item.traverse(direction, Math.max(1, getWidth() - 24), viewportHeight(), rect);
        if (inside) {
            cursor = 1;
            reveal(item.contentY + Math.max(0, Math.min(item.contentHeight, rect[1])), Math.max(0, rect[3]));
        } else if (entering) {
            int y = item.contentY - item.labelHeight();
            int height = item.contentHeight + item.labelHeight();
            if (direction == Canvas.UP && height > viewportHeight()) top = y + height - viewportHeight();
            else reveal(y, Math.min(viewportHeight(), height));
        } else if (relayout) {
            int y = item.contentY - item.labelHeight();
            int height = item.contentHeight + item.labelHeight();
            if (height <= viewportHeight()) reveal(y, height);
            else top = Math.max(y, Math.min(top, y + height - viewportHeight()));
        }
        return inside;
    }
    private boolean scrollItem(CustomItem item, int direction) {
        if (direction != Canvas.UP && direction != Canvas.DOWN) return false;
        if (item.labelHeight() + item.contentHeight <= viewportHeight()) return false;
        int first = item.contentY - item.labelHeight();
        int last = Math.max(first, item.contentY + item.contentHeight - viewportHeight());
        int step = Math.max(1, viewportHeight() - 16);
        if (direction == Canvas.DOWN && top < last) top = Math.min(last, top + step);
        else if (direction == Canvas.UP && top > first) top = Math.max(first, top - step);
        else return false;
        updateVisibility();
        return true;
    }
    protected void showNotify() {
        if (!hasCustomItems()) return;
        layoutCustom();
        if (count > 0 && items[focus] instanceof CustomItem) traverseItem((CustomItem)items[focus], CustomItem.NONE);
        updateVisibility();
    }
    protected void hideNotify() { for (int i = 0; i < count; i++) if (items[i] instanceof CustomItem) { CustomItem item = (CustomItem)items[i]; item.leave(); item.setVisible(false); } }
    private boolean intersectsViewport(int y, int height) {
        // Rows start at 46; the viewport clip starts two pixels above them.
        return height > 0 && y + height > top - 2 && y < top + viewportHeight();
    }
    private void updateVisibility() {
        for (int i = 0; i < count; i++) if (items[i] instanceof CustomItem) {
            CustomItem item = (CustomItem)items[i];
            item.setVisible(isShown() && intersectsViewport(item.contentY - item.labelHeight(), item.labelHeight() + item.contentHeight));
        }
    }
    private void paintCustomForm(Graphics g) {
        layoutCustom();
        int y = 0;
        for (int i = 0; i < count; i++) {
            Item item = items[i];
            int height = item instanceof CustomItem ? ((CustomItem)item).labelHeight() + ((CustomItem)item).contentHeight + 4 : 24;
            if (i == focus) {
                if (item instanceof CustomItem) {
                    CustomItem custom = (CustomItem)item;
                    if (!custom.entered || custom.layoutInvalid) traverseItem(custom, CustomItem.NONE);
                } else reveal(y, height);
            }
            y += height;
        }
        updateVisibility();
        g.setClip(8, 44, Math.max(1, getWidth() - 16), viewportHeight() + 2);
        y = 0;
        for (int i = 0; i < count; i++) {
            Item item = items[i];
            int height = item instanceof CustomItem ? ((CustomItem)item).labelHeight() + ((CustomItem)item).contentHeight + 4 : 24;
            int row = 46 + y - top;
            if (row + height > 44 && row < getHeight() - 32) {
                if (i == focus) { g.setColor(0x1E3A34); g.fillRect(8, row - 2, getWidth() - 16, height); }
                g.setColor(i == focus ? 0xF4E2B0 : 0xF2F2F2);
                if (item instanceof CustomItem) {
                    CustomItem custom = (CustomItem)item;
                    if (custom.visible && custom.labelHeight() > 0) g.drawString(custom.labelText(), 12, row, Graphics.TOP | Graphics.LEFT);
                    if (custom.visible && intersectsViewport(custom.contentY, custom.contentHeight)) custom.paintContent(g, 12, row + custom.labelHeight());
                } else paintItem(g, item, row);
            }
            y += height;
        }
        g.setClip(0, 0, getWidth(), getHeight());
    }
    private void paintItem(Graphics g, Item item, int y) {
        Font previous = g.getFont();
        if (item instanceof StringItem) g.setFont(((StringItem)item).getFont());
        g.drawString(item.labelText(), 12, y, Graphics.TOP | Graphics.LEFT);
        if (item instanceof StringItem) g.setFont(previous);
    }
    public void paint(Graphics g) {
        paintFrame(g, "FORM");
        if (hasCustomItems()) { paintCustomForm(g); paintCommands(g); return; }
        int visible = (getHeight() - 78) / 24; if (visible < 1) visible = 1;
        if (focus < top) top = focus;
        if (focus >= top + visible) top = focus - visible + 1;
        for (int i = top; i < count && i < top + visible; i++) {
            int y = 46 + (i - top) * 24;
            if (i == focus) { g.setColor(0x1E3A34); g.fillRect(8, y - 2, getWidth() - 16, 24); }
            g.setColor(i == focus ? 0xF4E2B0 : 0xF2F2F2);
            paintItem(g, items[i], y);
        }
        paintCommands(g);
    }
    protected void keyReleased(int code) {
        if (count == 0 || !(items[focus] instanceof CustomItem) || code == -6 || code == -7) return;
        CustomItem item = (CustomItem)items[focus];
        if (code >= 48 && code <= 57) { item.keyReleased(code); return; }
        int action = item.getGameAction(code);
        if (action != Canvas.UP && action != Canvas.DOWN && action != Canvas.LEFT && action != Canvas.RIGHT) item.keyReleased(code);
    }
    public void keyPressed(int code) {
        if (handleCommandMenu(code)) return;
        Item item = count > 0 ? items[focus] : null;
        if (code == -6 || code == -7) { dispatchCommand(code); return; }
        if (item instanceof TextField && (code >= 48 && code <= 57 || code == 42 || code == 35 || code == -8 || code == 127)) {
            if (((TextField)item).editKey(code)) itemChanged(item); return;
        }
        if (item instanceof CustomItem) {
            CustomItem custom = (CustomItem)item;
            // Numeric keypad events remain raw input even when getGameAction
            // maps 2/4/6/8/5 to navigation/FIRE. Dedicated actions still traverse.
            if (code >= 48 && code <= 57) { custom.keyPressed(code); return; }
            int action = custom.getGameAction(code);
            if (action == Canvas.UP || action == Canvas.DOWN || action == Canvas.LEFT || action == Canvas.RIGHT) {
                layoutCustom();
                if (!custom.entered) traverseItem(custom, CustomItem.NONE);
                if (traverseItem(custom, action)) return;
                // Once internal traversal was accepted, false means leave it.
                // Default-false items instead scroll their own row to its edge.
                if (cursor == 0 && scrollItem(custom, action)) return;
                int next = focus + (action == Canvas.UP || action == Canvas.LEFT ? -1 : 1);
                if (next >= 0 && next < count) moveFocus(next, action);
                return;
            }
            if ((action == Canvas.FIRE) && custom.activate()) return;
            custom.keyPressed(code); return;
        }
        if (code == 1 || code == 50 || code == -1) { if (focus > 0) moveFocus(focus - 1, Canvas.UP); }
        else if (code == 6 || code == 56 || code == -2) { if (focus + 1 < count) moveFocus(focus + 1, Canvas.DOWN); }
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
