package javax.microedition.lcdui;
public abstract class Canvas extends Displayable {
    public static final int UP = 1;
    public static final int DOWN = 6;
    public static final int LEFT = 2;
    public static final int RIGHT = 5;
    public static final int FIRE = 8;
    public static final int GAME_A = 9;
    public static final int GAME_B = 10;
    public static final int GAME_C = 11;
    public static final int GAME_D = 12;
    public static final int KEY_NUM0 = 48;
    public static final int KEY_NUM1 = 49;
    public static final int KEY_STAR = 42;
    public static final int KEY_POUND = 35;
    protected Canvas() {}
    protected abstract void paint(Graphics g);
    protected void keyPressed(int keyCode) {}
    protected void keyReleased(int keyCode) {}
    protected void keyRepeated(int keyCode) {}
    public native void repaint();
    public void repaint(int x, int y, int width, int height) {
        if (width <= 0 || height <= 0 || x >= getWidth() || y >= getHeight()
                || (long) x + width <= 0 || (long) y + height <= 0) return;
        // The current software backend coalesces dirty regions into a full repaint.
        repaint();
    }
    public native void serviceRepaints();
    public native void setFullScreenMode(boolean mode);
    public boolean hasPointerEvents() { return false; }
    public boolean hasPointerMotionEvents() { return false; }
    public boolean hasRepeatEvents() { return false; }
    public boolean isDoubleBuffered() { return true; }
    protected void pointerPressed(int x, int y) {}
    protected void pointerReleased(int x, int y) {}
    protected void pointerDragged(int x, int y) {}
    protected void showNotify() {}
    protected void hideNotify() {}
    protected void sizeChanged(int w, int h) {}
    public String getKeyName(int keyCode) {
        if (keyCode >= 48 && keyCode <= 57 || keyCode == KEY_STAR || keyCode == KEY_POUND)
            return new String(new char[] {(char) keyCode});
        if (keyCode == -6) return "Left soft key";
        if (keyCode == -7) return "Right soft key";
        if (keyCode == -8) return "Clear";
        int action = getGameAction(keyCode);
        if (action == UP) return "Up";
        if (action == DOWN) return "Down";
        if (action == LEFT) return "Left";
        if (action == RIGHT) return "Right";
        if (action == FIRE) return "Fire";
        if (action == GAME_A) return "Game A";
        if (action == GAME_B) return "Game B";
        if (action == GAME_C) return "Game C";
        if (action == GAME_D) return "Game D";
        throw new IllegalArgumentException("unknown key code");
    }
    public int getKeyCode(int gameAction) {
        if (gameAction == UP) return -1;
        if (gameAction == DOWN) return -2;
        if (gameAction == LEFT) return -3;
        if (gameAction == RIGHT) return -4;
        if (gameAction == FIRE) return 8;
        if (gameAction == GAME_A) return 9;
        if (gameAction == GAME_B) return 10;
        if (gameAction == GAME_C) return 11;
        if (gameAction == GAME_D) return 12;
        return 0;
    }
    public int getGameAction(int keyCode) {
        if (keyCode == -1 || keyCode == 50 || keyCode == UP) return UP;
        if (keyCode == -2 || keyCode == 56 || keyCode == DOWN) return DOWN;
        if (keyCode == -3 || keyCode == 52 || keyCode == LEFT) return LEFT;
        if (keyCode == -4 || keyCode == 54 || keyCode == RIGHT) return RIGHT;
        if (keyCode == -5 || keyCode == 53 || keyCode == FIRE) return FIRE;
        if (keyCode == 49 || keyCode == GAME_A) return GAME_A;
        if (keyCode == 51 || keyCode == GAME_B) return GAME_B;
        if (keyCode == 55 || keyCode == GAME_C) return GAME_C;
        if (keyCode == 57 || keyCode == GAME_D) return GAME_D;
        return 0;
    }
}
