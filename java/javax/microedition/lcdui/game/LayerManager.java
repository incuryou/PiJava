package javax.microedition.lcdui.game;
import javax.microedition.lcdui.Graphics;
public class LayerManager {
    Layer[] layers;
    int count;
    int viewX, viewY, viewW, viewH;
    public LayerManager() {
        layers = new Layer[16]; count = 0; viewW = 240; viewH = 320;
    }
    public void append(Layer layer) { insert(layer, count); }
    public void insert(Layer layer, int index) {
        if (count + 1 > layers.length) {
            Layer[] next = new Layer[layers.length * 2];
            System.arraycopy(layers, 0, next, 0, count);
            layers = next;
        }
        int i = count;
        while (i > index) { layers[i] = layers[i - 1]; i = i - 1; }
        layers[index] = layer;
        count = count + 1;
    }
    public void remove(Layer layer) {
        int i = 0;
        while (i < count) {
            if (layers[i] == layer) { removeIndex(i); return; }
            i = i + 1;
        }
    }
    private void removeIndex(int index) {
        int i = index;
        while (i + 1 < count) { layers[i] = layers[i + 1]; i = i + 1; }
        count = count - 1; layers[count] = null;
    }
    public Layer getLayerAt(int index) { return layers[index]; }
    public int getSize() { return count; }
    public void setViewWindow(int x, int y, int width, int height) { viewX = x; viewY = y; viewW = width; viewH = height; }
    public void paint(Graphics g, int x, int y) {
        int cx = g.getClipX(), cy = g.getClipY(), cw = g.getClipWidth(), ch = g.getClipHeight();
        g.setClip(x, y, viewW, viewH);
        int i = count - 1;
        while (i >= 0) {
            Layer layer = layers[i];
            int ox = layer.getX(), oy = layer.getY();
            layer.setPosition(ox - viewX + x, oy - viewY + y);
            layer.paint(g);
            layer.setPosition(ox, oy);
            i = i - 1;
        }
        g.setClip(cx, cy, cw, ch);
    }
}
