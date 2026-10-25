package javax.microedition.lcdui;
public class ChoiceGroup extends Item implements Choice {
    private ChoiceData data;
    private int highlight;
    public ChoiceGroup(String label, int type) { this(label, type, null, null); }
    public ChoiceGroup(String label, int type, String[] texts, Image[] images) {
        if (type != EXCLUSIVE && type != MULTIPLE && type != POPUP) throw new IllegalArgumentException();
        this.label = label; data = new ChoiceData(type, texts, images);
    }
    public int append(String text, Image image) { int index = data.append(text, image); changed(); return index; }
    public void insert(int index, String text, Image image) { data.insert(index, text, image); if (index <= highlight && data.count > 1) highlight++; changed(); }
    public void delete(int index) { data.delete(index); if (index < highlight || highlight >= data.count) highlight--; if (highlight < 0) highlight = 0; changed(); }
    public void deleteAll() { data.deleteAll(); highlight = 0; changed(); }
    public void set(int index, String text, Image image) { data.set(index, text, image); changed(); }
    public int size() { return data.count; }
    public String getString(int index) { data.index(index); return data.rows[index]; }
    public Image getImage(int index) { data.index(index); return data.images[index]; }
    public boolean isSelected(int index) { data.index(index); return data.flags[index]; }
    public int getSelectedIndex() { return data.selectedIndex(); }
    public void setSelectedIndex(int index, boolean on) { data.setSelectedIndex(index, on); if (on) highlight = index; changed(); }
    public int getSelectedFlags(boolean[] flags) { return data.getSelectedFlags(flags); }
    public void setSelectedFlags(boolean[] flags) { data.setSelectedFlags(flags); if (data.type != MULTIPLE && data.count > 0) highlight = data.selectedIndex(); changed(); }
    public void setFitPolicy(int policy) { data.setFitPolicy(policy); changed(); }
    public int getFitPolicy() { return data.fitPolicy; }
    public void setFont(int index, Font font) { data.setFont(index, font); changed(); }
    public Font getFont(int index) { return data.getFont(index); }
    boolean move(int direction) {
        if (data.count == 0) return false;
        int next = highlight + direction;
        if (next < 0) next = data.count - 1;
        if (next >= data.count) next = 0;
        highlight = next;
        if (data.type != MULTIPLE) data.setSelectedIndex(next, true);
        return true;
    }
    boolean toggle() { if (data.count == 0) return false; data.setSelectedIndex(highlight, data.type != MULTIPLE || !data.flags[highlight]); return true; }
    public String labelText() {
        String name = label == null ? "" : label;
        if (data.count == 0) return name;
        int row = data.type == MULTIPLE ? highlight : data.selectedIndex();
        String marker = data.type == MULTIPLE ? (data.flags[row] ? "[x] " : "[ ] ") : "";
        return name + (name.length() > 0 ? " " : "") + marker + data.rows[row];
    }
}
