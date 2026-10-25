package javax.microedition.m3g;
/** Bulk native fills retain Java array and range validation. */
final class M3GArrays {
 static native boolean _prepEnabled241();
 static native boolean _convert241(int[] data,int start,int count,int format);
 static native boolean _decode241(byte[] src,int srcOffset,byte[] palette,int[] dst,int dstOffset,int count,int format,boolean opaque);
 static native boolean _mipRow241(Image2D src,Image2D dst,int y);
 private static native void _fillShort(short[] data, int from, int to, short value);
 private static native void _fillInt(int[] data, int from, int to, int value);
 private static void check(int length,int from,int to) { if(from>to) throw new IllegalArgumentException(); if(from<0||to>length) throw new ArrayIndexOutOfBoundsException(); }
 static void fill(short[] data,short value) { fill(data,0,data.length,value); }
 static void fill(short[] data,int from,int to,short value) { check(data.length,from,to); _fillShort(data,from,to,value); }
 static void fill(int[] data,int from,int to,int value) { check(data.length,from,to); _fillInt(data,from,to,value); }
}
