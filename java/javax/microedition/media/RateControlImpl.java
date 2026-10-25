package javax.microedition.media;

/** Java roots retain the owner and rate across GC and existing J2S5 snapshots. */
final class RateControlImpl implements javax.microedition.media.control.RateControl {
    private static final RateControlImpl[] controls = new RateControlImpl[64];
    private final NativePlayer owner;
    private int rate = 100000;
    private RateControlImpl(NativePlayer player) { owner = player; }
    static synchronized RateControlImpl forPlayer(NativePlayer player) {
        player.rateHandle();
        int slot=-1;
        for(int i=0;i<controls.length;i++) {
            if(controls[i]!=null && controls[i].owner==player) return controls[i];
            if(controls[i]==null && slot<0) slot=i;
        }
        if(slot<0) throw new IllegalStateException("too many rate controls");
        RateControlImpl control=new RateControlImpl(player); controls[slot]=control; return control;
    }
    static synchronized void release(NativePlayer player) {
        for(int i=0;i<controls.length;i++) if(controls[i]!=null && controls[i].owner==player) controls[i]=null;
    }
    public int setRate(int value) {
        int applied=NativePlayer.nRate(owner.rateHandle(),value,this);
        if(applied<0) throw new IllegalStateException("player unavailable");
        return applied;
    }
    public int getRate() { owner.rateHandle(); return rate; }
    public int getMaxRate() { owner.rateHandle(); return 200000; }
    public int getMinRate() { owner.rateHandle(); return 50000; }
}
