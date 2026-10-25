package javax.microedition.lcdui;
/** Shared selection model. Highlights belong to the containing screen. */
final class ChoiceData {
    String[] rows = new String[8];
    Image[] images = new Image[8];
    boolean[] flags = new boolean[8];
    Font[] fonts = new Font[8];
    int count;
    int type;
    int fitPolicy;
    ChoiceData(int type, String[] texts, Image[] initialImages) {
        this.type = type;
        if (texts == null) {
            if (initialImages != null) throw new NullPointerException();
        } else {
            if (initialImages != null && initialImages.length != texts.length) throw new IllegalArgumentException();
            for (int i = 0; i < texts.length; i++) append(texts[i], initialImages == null ? null : initialImages[i]);
        }
    }
    void index(int index) { if (index < 0 || index >= count) throw new IndexOutOfBoundsException(); }
    void capacity() {
        if (count < rows.length) return;
        int n = rows.length * 2;
        String[] r = new String[n]; Image[] im = new Image[n]; boolean[] f = new boolean[n]; Font[] ft = new Font[n];
        System.arraycopy(rows, 0, r, 0, count); System.arraycopy(images, 0, im, 0, count);
        System.arraycopy(flags, 0, f, 0, count); System.arraycopy(fonts, 0, ft, 0, count);
        rows = r; images = im; flags = f; fonts = ft;
    }
    int append(String text, Image image) { insert(count, text, image); return count - 1; }
    void insert(int index, String text, Image image) {
        if (index < 0 || index > count) throw new IndexOutOfBoundsException();
        if (text == null) throw new NullPointerException();
        capacity();
        System.arraycopy(rows, index, rows, index + 1, count - index);
        System.arraycopy(images, index, images, index + 1, count - index);
        System.arraycopy(flags, index, flags, index + 1, count - index);
        System.arraycopy(fonts, index, fonts, index + 1, count - index);
        rows[index] = text; images[index] = image; fonts[index] = null;
        flags[index] = count == 0 && type != Choice.MULTIPLE;
        count++;
    }
    void delete(int index) {
        index(index);
        boolean wasSelected = flags[index];
        int n = count - index - 1;
        System.arraycopy(rows, index + 1, rows, index, n); System.arraycopy(images, index + 1, images, index, n);
        System.arraycopy(flags, index + 1, flags, index, n); System.arraycopy(fonts, index + 1, fonts, index, n);
        count--; rows[count] = null; images[count] = null; fonts[count] = null; flags[count] = false;
        if (count > 0 && wasSelected && type != Choice.MULTIPLE) flags[index < count ? index : count - 1] = true;
    }
    void deleteAll() { while (count > 0) delete(count - 1); }
    void set(int index, String text, Image image) { index(index); if (text == null) throw new NullPointerException(); rows[index] = text; images[index] = image; }
    int selectedIndex() { if (type == Choice.MULTIPLE) return -1; for (int i = 0; i < count; i++) if (flags[i]) return i; return -1; }
    void setSelectedIndex(int index, boolean on) {
        index(index);
        if (type == Choice.MULTIPLE) flags[index] = on;
        else if (on) { for (int i = 0; i < count; i++) flags[i] = i == index; }
    }
    int getSelectedFlags(boolean[] result) {
        if (result == null) throw new NullPointerException();
        if (result.length < count) throw new IllegalArgumentException();
        int selected = 0;
        for (int i = 0; i < result.length; i++) { result[i] = i < count && flags[i]; if (result[i]) selected++; }
        return selected;
    }
    void setSelectedFlags(boolean[] selected) {
        if (selected == null) throw new NullPointerException();
        if (selected.length < count) throw new IllegalArgumentException();
        int first = -1;
        for (int i = 0; i < count; i++) if (selected[i] && first < 0) first = i;
        if (first < 0 && count > 0) first = 0;
        for (int i = 0; i < count; i++) flags[i] = type == Choice.MULTIPLE ? selected[i] : i == first;
    }
    void setFitPolicy(int policy) { if (policy < 0 || policy > 2) throw new IllegalArgumentException(); fitPolicy = policy; }
    void setFont(int index, Font font) { index(index); fonts[index] = font; }
    Font getFont(int index) { index(index); return fonts[index] == null ? Font.getDefaultFont() : fonts[index]; }
}
