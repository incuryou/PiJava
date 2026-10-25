package javax.microedition.lcdui;
public class ImageItem extends Item {
    public static final int LAYOUT_DEFAULT = 0;
    public static final int LAYOUT_LEFT = 1;
    public static final int LAYOUT_RIGHT = 2;
    public static final int LAYOUT_CENTER = 3;
    Image image;
    String alt;
    public ImageItem(String label, Image image, int layout, String alt) {
        this.label = label;
        this.image = image;
        this.alt = alt;
    }
    public Image getImage() { return image; }
    public void setImage(Image image) { this.image = image; changed(); }
    public String labelText() {
        if (label != null && label.length() > 0) return label;
        return alt == null ? "IMAGE" : alt;
    }
}
