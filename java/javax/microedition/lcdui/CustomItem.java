package javax.microedition.lcdui;
public abstract class CustomItem extends Item {
    protected static final int NONE = 0, TRAVERSE_HORIZONTAL = 1, TRAVERSE_VERTICAL = 2;
    protected static final int KEY_PRESS = 4, KEY_RELEASE = 8, KEY_REPEAT = 16;
    protected static final int POINTER_PRESS = 32, POINTER_RELEASE = 64, POINTER_DRAG = 128;
    int contentWidth = -1, contentHeight = -1;
    int contentY;
    boolean visible, entered, layoutInvalid;
    int[] traversal = new int[4];
    private Image buffer;
    protected CustomItem(String label) { super(label); }
    protected abstract int getMinContentWidth();
    protected abstract int getMinContentHeight();
    protected abstract int getPrefContentWidth(int height);
    protected abstract int getPrefContentHeight(int width);
    protected abstract void paint(Graphics g, int w, int h);
    protected void sizeChanged(int w, int h) {}
    protected void showNotify() {}
    protected void hideNotify() {}
    protected boolean traverse(int dir, int viewportWidth, int viewportHeight, int[] visRect_inout) { return false; }
    protected void traverseOut() {}
    protected void keyPressed(int code) {}
    protected void keyReleased(int code) {}
    protected void keyRepeated(int code) {}
    protected void pointerPressed(int x, int y) {}
    protected void pointerReleased(int x, int y) {}
    protected void pointerDragged(int x, int y) {}
    protected final int getInteractionModes() { return TRAVERSE_HORIZONTAL | TRAVERSE_VERTICAL | KEY_PRESS | KEY_RELEASE; }
    public int getGameAction(int code) {
        if (code == -1 || code == 50 || code == Canvas.UP) return Canvas.UP;
        if (code == -2 || code == 56 || code == Canvas.DOWN) return Canvas.DOWN;
        if (code == -3 || code == 52 || code == Canvas.LEFT) return Canvas.LEFT;
        if (code == -4 || code == 54 || code == Canvas.RIGHT) return Canvas.RIGHT;
        if (code == -5 || code == 53 || code == Canvas.FIRE) return Canvas.FIRE;
        if (code == 49 || code == Canvas.GAME_A) return Canvas.GAME_A;
        if (code == 51 || code == Canvas.GAME_B) return Canvas.GAME_B;
        if (code == 55 || code == Canvas.GAME_C) return Canvas.GAME_C;
        if (code == 57 || code == Canvas.GAME_D) return Canvas.GAME_D;
        if (code == 48 || code == 42 || code == 35 || code == -6 || code == -7 || code == -8 || code == 127) return 0;
        throw new IllegalArgumentException();
    }
    int labelHeight() { return label == null || label.length() == 0 ? 0 : Font.getDefaultFont().getHeight() + 2; }
    public int getMinimumWidth() { return Math.max(0, getMinContentWidth()); }
    public int getMinimumHeight() { return Math.max(0, getMinContentHeight()) + labelHeight(); }
    public int getPreferredWidth() {
        return Math.max(getMinimumWidth(), preferredWidth < 0 ? getPrefContentWidth(preferredHeight < 0 ? -1 : Math.max(0, preferredHeight - labelHeight())) : preferredWidth);
    }
    public int getPreferredHeight() {
        return Math.max(getMinimumHeight(), preferredHeight < 0 ? Math.max(0, getPrefContentHeight(preferredWidth)) + labelHeight() : preferredHeight);
    }
    protected final void invalidate() { layoutInvalid = true; changed(); }
    protected final void repaint() { changed(); }
    protected final void repaint(int x, int y, int width, int height) {
        if (width > 0 && height > 0 && x < contentWidth && y < contentHeight && (long)x + width > 0 && (long)y + height > 0) changed();
    }
    void layout(int availableWidth) {
        int w = Math.min(availableWidth, getPreferredWidth());
        int h = preferredHeight < 0 ? Math.max(Math.max(0, getMinContentHeight()), getPrefContentHeight(w)) : Math.max(0, getPreferredHeight() - labelHeight());
        if (w != contentWidth || h != contentHeight) {
            contentWidth = w; contentHeight = h; buffer = null;
            sizeChanged(w, h);
        }
    }
    void setVisible(boolean value) {
        if (visible == value) return;
        visible = value;
        if (value) showNotify(); else hideNotify();
    }
    void leave() { if (entered) { entered = false; traverseOut(); } }
    void detach() { leave(); setVisible(false); buffer = null; }
    void paintContent(Graphics g, int x, int y) {
        if (contentWidth <= 0 || contentHeight <= 0) return;
        // A separate surface enforces the content boundary even when subclass
        // paint() expands its clip or changes its translation.
        if (buffer == null) buffer = Image.createImage(contentWidth, contentHeight);
        Graphics content = buffer.getGraphics();
        content.setColor(0); content.setFont(Font.getDefaultFont()); content.setStrokeStyle(Graphics.SOLID);
        paint(content, contentWidth, contentHeight);
        g.drawImage(buffer, x, y, Graphics.TOP | Graphics.LEFT);
    }
}
