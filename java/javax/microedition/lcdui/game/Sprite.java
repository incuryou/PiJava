package javax.microedition.lcdui.game;
import javax.microedition.lcdui.Graphics;
import javax.microedition.lcdui.Image;
public class Sprite extends Layer {
    public static final int TRANS_NONE = 0;
    public static final int TRANS_MIRROR_ROT180 = 1;
    public static final int TRANS_MIRROR = 2;
    public static final int TRANS_ROT180 = 3;
    public static final int TRANS_MIRROR_ROT270 = 4;
    public static final int TRANS_ROT90 = 5;
    public static final int TRANS_ROT270 = 6;
    public static final int TRANS_MIRROR_ROT90 = 7;
    Image image;
    int frameWidth, frameHeight, frame, transform, cols;
    int refX, refY;
    int[] sequence;
    int seqIndex;
    private int collisionX, collisionY, collisionWidth, collisionHeight;
    public Sprite(Image img) { this(img, img.getWidth(), img.getHeight()); }
    public Sprite(Image img, int fw, int fh) {
        super(fw, fh);
        validateFrames(img, fw, fh);
        image = img; frameWidth = fw; frameHeight = fh; frame = 0; transform = 0;
        collisionWidth = fw; collisionHeight = fh;
        cols = img.getWidth() / fw;
    }
    public Sprite(Sprite other) {
        this(other.image, other.frameWidth, other.frameHeight);
        frame = other.frame; transform = other.transform; setPosition(other.getX(), other.getY());
        refX = other.refX; refY = other.refY; visible = other.visible;
        w = other.w; h = other.h; seqIndex = other.seqIndex;
        if (other.sequence != null) {
            sequence = new int[other.sequence.length];
            for (int i = 0; i < sequence.length; i++) sequence[i] = other.sequence[i];
        }
        collisionX = other.collisionX; collisionY = other.collisionY;
        collisionWidth = other.collisionWidth; collisionHeight = other.collisionHeight;
    }
    public void setImage(Image img, int fw, int fh) {
        validateFrames(img, fw, fh);
        int oldCount = getRawFrameCount();
        int newCols = img.getWidth() / fw;
        int newCount = newCols * (img.getHeight() / fh);
        if (fw != frameWidth || fh != frameHeight) {
            collisionX = 0; collisionY = 0; collisionWidth = fw; collisionHeight = fh;
        }
        image = img; frameWidth = fw; frameHeight = fh;
        cols = newCols;
        w = transform >= TRANS_MIRROR_ROT270 && transform <= TRANS_MIRROR_ROT90 ? fh : fw;
        h = transform >= TRANS_MIRROR_ROT270 && transform <= TRANS_MIRROR_ROT90 ? fw : fh;
        if (newCount < oldCount) {
            sequence = null; seqIndex = 0; frame = 0;
        }
    }
    private static void validateFrames(Image img, int fw, int fh) {
        if (img == null) throw new NullPointerException();
        if (fw < 1 || fh < 1 || img.getWidth() % fw != 0 || img.getHeight() % fh != 0)
            throw new IllegalArgumentException();
    }
    private void selectFrame(int f) {
        seqIndex = f;
        frame = sequence == null ? f : sequence[f];
    }
    public void setFrame(int f) {
        if (f < 0 || f >= getFrameSequenceLength()) throw new IndexOutOfBoundsException();
        selectFrame(f);
    }
    public int getFrame() { return sequence == null ? frame : seqIndex; }
    public int getRawFrameCount() {
        int rows = image.getHeight() / frameHeight; if (rows < 1) rows = 1;
        return cols * rows;
    }
    public void setFrameSequence(int[] seq) {
        int[] copy = null;
        if (seq != null) {
            if (seq.length == 0) throw new IllegalArgumentException();
            int count = getRawFrameCount();
            copy = new int[seq.length];
            for (int i = 0; i < seq.length; i++) {
                if (seq[i] < 0 || seq[i] >= count) throw new ArrayIndexOutOfBoundsException();
                copy[i] = seq[i];
            }
        }
        sequence = copy;
        selectFrame(0);
    }
    public int getFrameSequenceLength() { return sequence == null ? getRawFrameCount() : sequence.length; }
    public void nextFrame() {
        int f = (sequence == null ? frame : seqIndex) + 1;
        if (f >= getFrameSequenceLength()) f = 0;
        selectFrame(f);
    }
    public void prevFrame() {
        int f = (sequence == null ? frame : seqIndex) - 1;
        if (f < 0) f = getFrameSequenceLength() - 1;
        selectFrame(f);
    }
    public void setTransform(int t) { transform = t; }
    public void defineReferencePixel(int x, int y) { refX = x; refY = y; }
    public void setRefPixelPosition(int x, int y) { setPosition(x - refX, y - refY); }
    public int getRefPixelX() { return getX() + refX; }
    public int getRefPixelY() { return getY() + refY; }
    public void defineCollisionRectangle(int x, int y, int width, int height) {
        if (width < 0 || height < 0) throw new IllegalArgumentException();
        collisionX = x; collisionY = y; collisionWidth = width; collisionHeight = height;
    }
    public final boolean collidesWith(Sprite other, boolean pixelLevel) {
        if (pixelLevel) return hit(other);
        if (other == null) throw new NullPointerException();
        if (!isVisible() || !other.isVisible() || collisionWidth <= 0 || collisionHeight <= 0
                || other.collisionWidth <= 0 || other.collisionHeight <= 0) return false;
        long left = (long) getX() + collisionX;
        long top = (long) getY() + collisionY;
        long otherLeft = (long) other.getX() + other.collisionX;
        long otherTop = (long) other.getY() + other.collisionY;
        return left < otherLeft + other.collisionWidth && otherLeft < left + collisionWidth
            && top < otherTop + other.collisionHeight && otherTop < top + collisionHeight;
    }
    public final boolean collidesWith(TiledLayer other, boolean pixelLevel) { return hit(other); }
    public final boolean collidesWith(Image img, int ix, int iy, boolean pixelLevel) {
        if (img == null || !isVisible()) return false;
        return getX() < ix + img.getWidth() && ix < getX() + getWidth()
            && getY() < iy + img.getHeight() && iy < getY() + getHeight();
    }
    private boolean hit(Layer other) {
        if (other == null || !isVisible() || !other.isVisible()) return false;
        return getX() < other.getX() + other.getWidth() && other.getX() < getX() + getWidth()
            && getY() < other.getY() + other.getHeight() && other.getY() < getY() + getHeight();
    }
    public void paint(Graphics g) {
        if (!visible || image == null) return;
        int f = frame;
        int sx = (f % cols) * frameWidth;
        int sy = (f / cols) * frameHeight;
        g.drawRegion(image, sx, sy, frameWidth, frameHeight, transform, x, y, 0);
    }
}
