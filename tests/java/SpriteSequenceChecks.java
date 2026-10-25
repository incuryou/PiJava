import javax.microedition.lcdui.Image;
import javax.microedition.lcdui.game.Sprite;

// MIDP 2 Sprite contract: getFrame is a sequence position, never a raw frame.
public final class SpriteSequenceChecks {
    private static final int SENTINEL = 114241;
    private static GameSprite savedOriginal;
    private static Sprite savedCopy;
    private static Sprite savedAlias;
    private static Sprite savedDefault;

    private static void check(boolean value, String message) {
        if (!value) throw new RuntimeException(message);
    }

    private static void equal(int expected, int actual, String message) {
        if (expected != actual)
            throw new RuntimeException(message + " expected=" + expected + " actual=" + actual);
    }

    public static final class GameSprite extends Sprite {
        private int[] a;
        public int marker;
        public GameSprite(Image image) {
            super(image, 1, 1);
            setFrameSequence(new int[] {5, 1, 3});
            marker = 114243;
        }
        public void setFrameSequence(int[] value) {
            a = value;
            super.setFrameSequence(value);
        }
        // Same inherited-frame lookup shape as QuanMianZhan aa.c()I.
        public int c() { return a[super.getFrame()]; }
    }

    public static int runRed() {
        GameSprite sprite = new GameSprite(Image.createImage(6, 1));
        equal(0, sprite.getFrame(), "custom getFrame sequence index");
        equal(5, sprite.c(), "game subclass raw-frame array lookup");
        return SENTINEL;
    }

    // Six distinct raw frames in row-major order, including a second row.
    private static Image palette() {
        return Image.createRGBImage(new int[] {
            0xffff0000, 0xff00ff00, 0xff0000ff,
            0xffffff00, 0xff00ffff, 0xffff00ff
        }, 3, 2, false);
    }

    private static void displayed(Sprite sprite, int index, int color, String message) {
        equal(index, sprite.getFrame(), message + " index");
        Image target = Image.createImage(12, 12);
        sprite.paint(target.getGraphics());
        int[] pixel = new int[1];
        target.getRGB(pixel, 0, 1, sprite.getX(), sprite.getY(), 1, 1);
        equal(color, pixel[0], message + " painted raw frame");
    }

    public static int runIndex() {
        Sprite s = new Sprite(palette(), 1, 1);
        equal(6, s.getRawFrameCount(), "raw count includes rows");
        equal(6, s.getFrameSequenceLength(), "default sequence length");
        displayed(s, 0, 0xffff0000, "default starts at zero");
        s.prevFrame();
        displayed(s, 5, 0xffff00ff, "default previous wraps");
        s.nextFrame();
        displayed(s, 0, 0xffff0000, "default next wraps");
        s.setFrame(4);
        displayed(s, 4, 0xff00ffff, "default setFrame");
        s.setFrameSequence(new int[] {5, 1, 3});
        displayed(s, 0, 0xffff00ff, "custom starts at sequence zero");
        s.nextFrame();
        displayed(s, 1, 0xff00ff00, "custom next selects raw one");
        s.setFrame(2);
        displayed(s, 2, 0xffffff00, "custom setFrame selects raw three");
        s.nextFrame();
        displayed(s, 0, 0xffff00ff, "custom next wraps");
        s.prevFrame();
        displayed(s, 2, 0xffffff00, "custom previous wraps");
        s.setFrameSequence(new int[] {4, 4, 0, 4});
        s.nextFrame();
        displayed(s, 1, 0xff00ffff, "repeated raw frames have distinct positions");
        s.setFrame(3);
        displayed(s, 3, 0xff00ffff, "repeat sequence may exceed selected raw index");
        s.setFrameSequence(new int[] {5, 1, 5});
        s.setFrame(2);
        displayed(s, 2, 0xffff00ff, "repeated raw frame cannot determine sequence position");
        s.setFrameSequence(new int[] {0, 0, 0, 0, 0, 0, 0});
        equal(7, s.getFrameSequenceLength(), "custom sequence may exceed raw count");
        s.setFrame(6);
        displayed(s, 6, 0xffff0000, "setFrame bound is sequence length not raw count");
        s.setFrameSequence(null);
        equal(6, s.getFrameSequenceLength(), "null restores default length");
        displayed(s, 0, 0xffff0000, "null resets sequence and raw frame");
        s.setFrameSequence(new int[] {3});
        s.nextFrame(); s.prevFrame();
        displayed(s, 0, 0xffffff00, "single entry remains selected");
        GameSprite game = new GameSprite(palette());
        equal(5, game.c(), "game first lookup");
        game.setFrame(2);
        equal(3, game.c(), "game setFrame lookup");
        game.nextFrame();
        equal(5, game.c(), "game wrapped lookup");
        return SENTINEL;
    }

    public static int runValidation() {
        Sprite s = new Sprite(palette(), 1, 1);
        int[] input = new int[] {5, 1, 3};
        s.setFrameSequence(input);
        input[0] = 0; input[1] = 2;
        displayed(s, 0, 0xffff00ff, "sequence copies first input element");
        s.nextFrame();
        displayed(s, 1, 0xff00ff00, "sequence copies later input elements");
        boolean caught = false;
        try { s.setFrame(-1); } catch (IndexOutOfBoundsException e) { caught = true; }
        check(caught, "negative setFrame throws IndexOutOfBoundsException");
        displayed(s, 1, 0xff00ff00, "negative setFrame preserves state");
        caught = false;
        try { s.setFrame(3); } catch (IndexOutOfBoundsException e) { caught = true; }
        check(caught, "setFrame uses sequence length boundary");
        displayed(s, 1, 0xff00ff00, "large setFrame preserves state");
        caught = false;
        try { s.setFrameSequence(new int[0]); } catch (IllegalArgumentException e) { caught = true; }
        check(caught, "empty sequence throws IllegalArgumentException");
        displayed(s, 1, 0xff00ff00, "empty sequence preserves state");
        caught = false;
        try { s.setFrameSequence(new int[] {0, -1}); } catch (ArrayIndexOutOfBoundsException e) { caught = true; }
        check(caught, "negative raw frame throws ArrayIndexOutOfBoundsException");
        displayed(s, 1, 0xff00ff00, "negative sequence element preserves state");
        caught = false;
        try { s.setFrameSequence(new int[] {0, 6}); } catch (ArrayIndexOutOfBoundsException e) { caught = true; }
        check(caught, "raw count boundary throws ArrayIndexOutOfBoundsException");
        equal(3, s.getFrameSequenceLength(), "invalid sequence preserves old length");
        displayed(s, 1, 0xff00ff00, "out of range sequence preserves state");
        s.nextFrame();
        displayed(s, 2, 0xffffff00, "old sequence remains usable after rejection");
        s.setFrameSequence(null);
        s.setFrame(5);
        caught = false;
        try { s.setFrame(6); } catch (IndexOutOfBoundsException e) { caught = true; }
        check(caught, "default setFrame rejects raw count");
        displayed(s, 5, 0xffff00ff, "default rejected setFrame preserves state");
        return SENTINEL;
    }

    public static int runCopy() {
        Sprite source = new Sprite(palette(), 1, 1);
        source.setFrameSequence(new int[] {5, 1, 3});
        source.setFrame(2);
        source.setPosition(2, 3);
        source.defineReferencePixel(7, 8);
        source.setVisible(false);
        Sprite copy = new Sprite(source);
        equal(3, copy.getFrameSequenceLength(), "copy retains custom sequence");
        equal(2, copy.getFrame(), "copy retains sequence position");
        equal(2, copy.getX(), "copy retains x");
        equal(3, copy.getY(), "copy retains y");
        equal(9, copy.getRefPixelX(), "copy retains reference x");
        equal(11, copy.getRefPixelY(), "copy retains reference y");
        check(!copy.isVisible(), "copy retains visibility");
        copy.setVisible(true);
        displayed(copy, 2, 0xffffff00, "copy paints selected raw frame");
        source.nextFrame();
        equal(0, source.getFrame(), "source advances independently");
        displayed(copy, 2, 0xffffff00, "source advance leaves copy unchanged");
        source.setFrameSequence(new int[] {0});
        copy.nextFrame();
        displayed(copy, 0, 0xffff00ff, "source sequence replacement leaves copy sequence");
        copy.setFrameSequence(new int[] {2});
        equal(1, source.getFrameSequenceLength(), "copy replacement leaves source sequence length");
        source.setVisible(true);
        displayed(source, 0, 0xffff0000, "copy replacement leaves source raw frame");
        Sprite defaultSource = new Sprite(palette(), 1, 1);
        defaultSource.setFrame(4);
        Sprite defaultCopy = new Sprite(defaultSource);
        displayed(defaultCopy, 4, 0xff00ffff, "default copy retains selected frame");
        defaultSource.nextFrame();
        displayed(defaultCopy, 4, 0xff00ffff, "default copy advances independently");
        Sprite mirrored = new Sprite(Image.createRGBImage(new int[] {
            0xff112233, 0xff445566, 0xff778899, 0xffaabbcc
        }, 4, 1, false), 2, 1);
        mirrored.setFrameSequence(new int[] {1, 0});
        mirrored.setFrame(1);
        mirrored.setTransform(Sprite.TRANS_MIRROR);
        Sprite mirroredCopy = new Sprite(mirrored);
        displayed(mirroredCopy, 1, 0xff445566, "copy retains transform and custom raw selection");
        return SENTINEL;
    }

    public static int runImage() {
        Sprite s = new Sprite(palette(), 1, 1);
        s.setFrameSequence(new int[] {5, 1, 3});
        s.setFrame(2);
        s.setImage(palette(), 1, 1);
        equal(3, s.getFrameSequenceLength(), "same count preserves custom length");
        displayed(s, 2, 0xffffff00, "same count preserves current entry");
        Image vertical = Image.createRGBImage(new int[] {
            0xff123456, 0xff234567, 0xff345678,
            0xff456789, 0xff56789a, 0xff6789ab
        }, 1, 6, false);
        s.setImage(vertical, 1, 1);
        displayed(s, 2, 0xff456789, "same count new column layout uses raw three");
        s.setFrame(0);
        displayed(s, 0, 0xff6789ab, "same count new column layout uses raw five");
        s.setFrame(2);
        Image eight = Image.createRGBImage(new int[] {
            0xff112233, 0xff223344, 0xff334455, 0xff445566,
            0xff556677, 0xff667788, 0xff778899, 0xff8899aa
        }, 8, 1, false);
        s.setImage(eight, 1, 1);
        equal(8, s.getRawFrameCount(), "growth raw count");
        equal(3, s.getFrameSequenceLength(), "growth preserves custom sequence");
        displayed(s, 2, 0xff445566, "growth preserves index and uses new pixels");
        s.nextFrame();
        displayed(s, 0, 0xff667788, "growth preserves custom mapping");
        s.setImage(palette(), 1, 1);
        equal(6, s.getFrameSequenceLength(), "shrink discards custom even if all entries fit");
        displayed(s, 0, 0xffff0000, "shrink resets entry zero");
        s.setFrame(4);
        s.setImage(eight, 1, 1);
        equal(8, s.getFrameSequenceLength(), "default growth adds all new frames");
        displayed(s, 4, 0xff556677, "default growth retains index");
        s.setFrameSequence(new int[] {0, 1, 2, 3, 4, 5, 6, 7});
        s.setFrame(7);
        Image nine = Image.createImage(9, 1);
        s.setImage(nine, 1, 1);
        equal(8, s.getFrameSequenceLength(), "explicit identity sequence remains custom on growth");
        equal(7, s.getFrame(), "identity custom growth preserves entry");
        s.setFrameSequence(new int[] {0, 1});
        s.setFrame(1);
        s.setImage(eight, 1, 1);
        equal(8, s.getFrameSequenceLength(), "shrink discards valid short custom sequence");
        displayed(s, 0, 0xff112233, "shrink resets even when old selected raw remains valid");
        s.setFrame(7);
        s.setImage(palette(), 1, 1);
        displayed(s, 0, 0xffff0000, "default shrink resets out of range index");
        s.setFrameSequence(new int[] {5, 1, 3});
        s.setFrame(1);
        boolean caught = false;
        try { s.setImage(null, 1, 1); } catch (NullPointerException e) { caught = true; }
        check(caught, "null setImage throws");
        displayed(s, 1, 0xff00ff00, "null setImage preserves current image and sequence");
        caught = false;
        try { s.setImage(eight, 0, 1); } catch (IllegalArgumentException e) { caught = true; }
        check(caught, "zero frame width throws");
        displayed(s, 1, 0xff00ff00, "invalid width preserves image and sequence");
        caught = false;
        try { s.setImage(eight, 3, 1); } catch (IllegalArgumentException e) { caught = true; }
        check(caught, "indivisible frame width throws");
        displayed(s, 1, 0xff00ff00, "indivisible width preserves image and sequence");
        caught = false;
        try { s.setImage(palette(), 1, 3); } catch (IllegalArgumentException e) { caught = true; }
        check(caught, "indivisible frame height throws");
        displayed(s, 1, 0xff00ff00, "indivisible height preserves image and sequence");
        caught = false;
        try { s.setImage(eight, 1, -1); } catch (IllegalArgumentException e) { caught = true; }
        check(caught, "negative frame height throws");
        equal(6, s.getRawFrameCount(), "rejected image updates preserve raw count");
        equal(3, s.getFrameSequenceLength(), "rejected image updates preserve custom length");
        equal(1, s.getWidth(), "rejected image updates preserve layer width");
        equal(1, s.getHeight(), "rejected image updates preserve layer height");
        displayed(s, 1, 0xff00ff00, "negative height preserves image and sequence");
        s.setImage(eight, 2, 1);
        equal(4, s.getRawFrameCount(), "resized raw count");
        equal(2, s.getWidth(), "resized frame updates layer width");
        equal(1, s.getHeight(), "resized frame retains layer height");
        displayed(s, 0, 0xff112233, "resized shrink resets default");
        return SENTINEL;
    }

    public static int run() {
        runRed(); runIndex(); runValidation(); runCopy(); runImage();
        return SENTINEL;
    }

    public static void setupSnapshot() {
        savedOriginal = new GameSprite(palette());
        savedOriginal.setPosition(2, 3);
        savedOriginal.setFrame(1);
        savedCopy = new Sprite(savedOriginal);
        savedCopy.setFrame(2);
        savedAlias = savedOriginal;
        savedDefault = new Sprite(palette(), 1, 1);
        savedDefault.setFrame(4);
    }

    public static void mutateSnapshot() {
        savedOriginal.setFrameSequence(null);
        savedOriginal.setPosition(0, 0);
        savedOriginal.marker = -1;
        savedCopy.setFrameSequence(new int[] {0});
        savedAlias = savedCopy;
        savedDefault.setFrameSequence(new int[] {0});
    }

    public static int verifySnapshot() {
        check(savedAlias == savedOriginal, "snapshot restores root alias");
        equal(114243, savedOriginal.marker, "snapshot preserves subclass field");
        equal(2, savedOriginal.getX(), "snapshot restores inherited x");
        equal(3, savedOriginal.getY(), "snapshot restores inherited y");
        equal(3, savedOriginal.getFrameSequenceLength(), "snapshot restores original sequence");
        equal(3, savedCopy.getFrameSequenceLength(), "snapshot restores copied sequence");
        equal(6, savedDefault.getFrameSequenceLength(), "snapshot restores default sequence length");
        displayed(savedDefault, 4, 0xff00ffff, "snapshot restores nonzero default frame");
        savedDefault.nextFrame();
        displayed(savedDefault, 5, 0xffff00ff, "snapshot default next continuation");
        savedDefault.nextFrame();
        displayed(savedDefault, 0, 0xffff0000, "snapshot default next wraps");
        savedDefault.prevFrame();
        displayed(savedDefault, 5, 0xffff00ff, "snapshot default previous wraps");
        displayed(savedOriginal, 1, 0xff00ff00, "snapshot restores original index and raw frame");
        displayed(savedCopy, 2, 0xffffff00, "snapshot restores copy index and raw frame");
        equal(1, savedOriginal.c(), "snapshot game subclass lookup");
        savedOriginal.nextFrame();
        equal(3, savedOriginal.c(), "snapshot game subclass continuation");
        displayed(savedCopy, 2, 0xffffff00, "snapshot source advance preserves copy");
        savedCopy.nextFrame();
        displayed(savedCopy, 0, 0xffff00ff, "snapshot copy wraps independently");
        savedOriginal.setFrameSequence(null);
        displayed(savedCopy, 0, 0xffff00ff, "snapshot independent copied sequence");
        return SENTINEL;
    }
}
