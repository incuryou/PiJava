/*
 * MIT License
 * Copyright (c) 2026 Yury Kharchenko
 * Copyright (c) 2026 Roman Lahin
 * Adapted for PiJava from MascotME 7659769711c8d93232b3364a00fe9c2c1e581772.
 * CORE RGBTRIPLE support; checked dimensions and raster bounds; queued shallow captures and release.
 * Full MIT license: licenses/mascotme.txt.
 */

package com.mascotcapsule.micro3d.v3;

import java.io.IOException;
//import javax.microedition.lcdui.Image;

public class Texture {
	//8-bit indexed color texture data
	byte[] bitmapData;
	//32-bit color bitmap for environment mapping
	int[] envmapData;
	//Image debugImage;
	int width, height;
	int widthBit, heightBit;
	int paddedWidth, paddedHeight;
	
	//ARGB palette (256 colors or 256*32 when shade lookup table is used)
	int[] origPalette, palette;
	// Captured queues share this Java-owned slot. Lazy lighting preparation is
	// performed once per immutable texture, not once per frame/capture.
	private int[][] shadedPaletteCache;
	boolean firstColorIsBlack;
	
	//True if this is a model texture, false if for environment mapping
	boolean isForModel;
	
	//Used in Graphics3D polygons list
	int g3dBindIdx = -1;
	private Texture() {}
	// Queue-owned shallow capture: dispose clears the public object's references,
	// never the arrays already retained by a submitted draw.
	final Texture _capture() {
		if(isForModel?(bitmapData==null||palette==null):envmapData==null)throw new IllegalStateException("Disposed texture");
		if(isForModel&&shadedPaletteCache==null)shadedPaletteCache=new int[1][];
		Texture t=new Texture();t.bitmapData=bitmapData;t.envmapData=envmapData;
		t.width=width;t.height=height;t.widthBit=widthBit;t.heightBit=heightBit;
		t.paddedWidth=paddedWidth;t.paddedHeight=paddedHeight;t.origPalette=origPalette;t.palette=palette;
		t.firstColorIsBlack=firstColorIsBlack;t.isForModel=isForModel;
		t.shadedPaletteCache=shadedPaletteCache;
		return t;
	}

	public Texture(byte[] b, boolean isForModel) {
		if (b == null) throw new NullPointerException();
		this.isForModel = isForModel;
		loadBMP(b);
	}

	public Texture(String name, boolean isForModel) throws IOException {
		this(Loader.resource(Texture.class,name),isForModel);
	}
	
	private static final int BMP_FILE_HEADER_SIZE = 14;
	private static final int BMP_VERSION_3 = 40;
	private static final int BMP_VERSION_CORE = 12;

	// PiJava: CORE palettes are RGBTRIPLEs; reject bad sizes/offsets before allocation.
	private final void loadBMP(byte[] data) {
		Loader loader=new Loader(data);
		if(loader.readUByte()!='B'||loader.readUByte()!='M')throw new RuntimeException("Not a BMP");
		loader.skip(8);int rasterOffset=loader.readInt(),dibHeaderSize=loader.readInt();
		int numColors,colorSize,signedHeight,planes,bpp;
		if(dibHeaderSize==BMP_VERSION_CORE) {
			width=loader.readUShort();signedHeight=loader.readUShort();planes=loader.readUShort();bpp=loader.readUShort();
			numColors=256;colorSize=3;
		}else if(dibHeaderSize==BMP_VERSION_3) {
			width=loader.readInt();signedHeight=loader.readInt();planes=loader.readUShort();bpp=loader.readUShort();
			if(loader.readInt()!=0)throw new RuntimeException("Compressed BMP is unsupported");
			loader.skip(12);numColors=loader.readInt();if(numColors==0)numColors=256;loader.skip(4);colorSize=4;
		}else throw new RuntimeException("Unsupported BMP header");
		if(width<=0||signedHeight==0||signedHeight==Integer.MIN_VALUE||planes!=1||bpp!=8||numColors<1||numColors>256)
			throw new RuntimeException("Invalid indexed BMP dimensions or palette");
		height=signedHeight<0?-signedHeight:signedHeight;
		if(!isForModel&&(width>64||height>64))throw new RuntimeException("Sphere texture exceeds 64x64");
		int paletteOffset=BMP_FILE_HEADER_SIZE+dibHeaderSize;
		long paletteEnd=paletteOffset+(long)numColors*colorSize;
		long stride=((long)width+3)&~3L;
		if(rasterOffset<paletteEnd||paletteEnd>data.length||rasterOffset+(long)height*stride>data.length)
			throw new RuntimeException("Truncated BMP palette or raster");
		widthBit=isForModel?countBits(width):6;heightBit=isForModel?countBits(height):6;
		paddedWidth=1<<widthBit;paddedHeight=1<<heightBit;
		long count=(long)paddedWidth*paddedHeight;
		if(count>Integer.MAX_VALUE)throw new RuntimeException("BMP is too large");
		palette=new int[256];
		for(int i=0;i<numColors;i++) {
			int p=paletteOffset+i*colorSize;
			palette[i]=0xff000000|((data[p+2]&255)<<16)|((data[p+1]&255)<<8)|(data[p]&255);
		}
		bitmapData=new byte[(int)count];
		for(int y=0;y<height;y++) {
			int from=(int)(rasterOffset+(signedHeight>0?height-1-y:y)*stride);
			for(int x=0;x<width;x++) {
				int index=data[from+x]&255;
				if(index>=numColors)throw new RuntimeException("BMP palette index out of range");
				bitmapData[y*paddedWidth+x]=(byte)index;
			}
		}
		origPalette=palette;firstColorIsBlack=palette[0]==0xff000000;
		if(!isForModel) {
			envmapData=new int[bitmapData.length];
			for(int i=0;i<envmapData.length;i++) { int index=bitmapData[i]&255;envmapData[i]=index==0?0:palette[index]&0xffffff; }
			bitmapData=null;origPalette=null;palette=null;
		}
	}
	private final int countBits(int size) {
		for(int i=0;i<31;i++)if((1L<<i)>=size)return i;
		throw new RuntimeException("BMP dimension is too large");
	}
	
	final void generateShadedPalette() {
		if(shadedPaletteCache==null)shadedPaletteCache=new int[1][];
		if(shadedPaletteCache[0]!=null){palette=shadedPaletteCache[0];return;}
		int[] newPalette = new int[256 * 32];
		
		for (int shade = 0; shade < 32; shade++) {
			for (int i = 0; i < 256; i++) {
				int color = palette[i];

				int r = (color >> 16) & 0xFF;
				int g = (color >> 8) & 0xFF;
				int b = color & 0xFF;

				r = (r * shade) / 31;
				g = (g * shade) / 31;
				b = (b * shade) / 31;

				newPalette[shade * 256 + i] = 0xff000000 | (r << 16) | (g << 8) | b;
			}
		}
		
		shadedPaletteCache[0]=newPalette;
		palette = newPalette;
	}

	public final void dispose() {
		bitmapData = null;
		envmapData = null;
		palette = null;
		origPalette = null;
		shadedPaletteCache = null;
	}
}
