package javax.microedition.lcdui;
/** Small character-wrapping renderer shared by TextBox and Alert. */
final class ScreenText {
    static int lineCount(String text, int width) { return render(null, text, 0, 0, width, 0, 0); }
    static int paint(Graphics g, String text, int x, int y, int width, int height, int skip) { return render(g, text, x, y, width, height, skip); }
    private static int render(Graphics g, String text, int x, int y, int width, int height, int skip) {
        if (text == null) return 0;
        Font font = Font.getDefaultFont(); int start = 0, pixels = 0, line = 0;
        for (int i = 0; i <= text.length(); i++) {
            boolean end = i == text.length(); char c = end ? '\n' : text.charAt(i);
            int next = end || c == '\n' ? 0 : font.charWidth(c);
            if (end || c == '\n' || pixels + next > width && i > start) {
                if (g != null && line >= skip && (line - skip + 1) * 18 <= height) g.drawString(text.substring(start, i), x, y + (line - skip) * 18, Graphics.TOP | Graphics.LEFT);
                line++; start = c == '\n' ? i + 1 : i; pixels = 0;
            }
            if (c != '\n') pixels += next;
        }
        return line;
    }
}
