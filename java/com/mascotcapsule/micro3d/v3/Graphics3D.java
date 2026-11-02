/*
 * MIT License
 * Copyright (c) 2026 Roman Lahin
 * Fit32 adaptation: actual MIDP targets, independent texture queues, validated commands.
 * Modified 2026-10-31. See third-party license/provenance manifest.
 */

package com.mascotcapsule.micro3d.v3;

import javax.microedition.lcdui.Graphics;

public class Graphics3D {
	//Constants
	public static final int COMMAND_LIST_VERSION_1_0 = 0xFE000001;
	public static final int COMMAND_END = 0x80000000;
	public static final int COMMAND_NOP = 0x81000000;
	public static final int COMMAND_FLUSH = 0x82000000;
	public static final int COMMAND_ATTRIBUTE = 0x83000000;
	public static final int COMMAND_CLIP = 0x84000000;
	public static final int COMMAND_CENTER = 0x85000000;
	public static final int COMMAND_TEXTURE_INDEX = 0x86000000;
	public static final int COMMAND_AFFINE_INDEX = 0x87000000;
	public static final int COMMAND_PARALLEL_SCALE = 0x90000000;
	public static final int COMMAND_PARALLEL_SIZE = 0x91000000;
	public static final int COMMAND_PERSPECTIVE_FOV = 0x92000000;
	public static final int COMMAND_PERSPECTIVE_WH = 0x93000000;
	public static final int COMMAND_AMBIENT_LIGHT = 0xa0000000;
	public static final int COMMAND_DIRECTION_LIGHT = 0xa1000000;
	public static final int COMMAND_THRESHOLD = 0xaf000000;

	public static final int ENV_ATTR_LIGHTING = 1;
	public static final int ENV_ATTR_SPHERE_MAP = 2;
	public static final int ENV_ATTR_TOON_SHADING = 4;
	public static final int ENV_ATTR_SEMI_TRANSPARENT = 8;

	public static final int PATTR_LIGHTING = 0x01;
	public static final int PATTR_SPHERE_MAP = 0x02;
	public static final int PATTR_COLORKEY = 0x10;
	public static final int PATTR_BLEND_NORMAL = 0x00;
	public static final int PATTR_BLEND_HALF = 0x20;
	public static final int PATTR_BLEND_ADD = 0x40;
	public static final int PATTR_BLEND_SUB = 0x60;

	public static final int PDATA_NORMAL_NONE = 0x0000;
	public static final int PDATA_NORMAL_PER_FACE = 0x0200;
	public static final int PDATA_NORMAL_PER_VERTEX = 0x0300;
	public static final int PDATA_COLOR_NONE = 0x0000;
	public static final int PDATA_COLOR_PER_COMMAND = 0x0400;
	public static final int PDATA_COLOR_PER_FACE = 0x0800;
	public static final int PDATA_TEXURE_COORD_NONE = 0x0000;
	public static final int PDATA_TEXURE_COORD = 0x3000;
	public static final int PDATA_POINT_SPRITE_PARAMS_PER_CMD = 0x1000;
	public static final int PDATA_POINT_SPRITE_PARAMS_PER_FACE = 0x2000;
	public static final int PDATA_POINT_SPRITE_PARAMS_PER_VERTEX = 0x3000;

	public static final int POINT_SPRITE_LOCAL_SIZE = 0;
	public static final int POINT_SPRITE_PIXEL_SIZE = 1;
	public static final int POINT_SPRITE_PERSPECTIVE = 0;
	public static final int POINT_SPRITE_NO_PERS = 2;

	public static final int PRIMITVE_POINTS = 0x01000000;
	public static final int PRIMITVE_LINES = 0x02000000;
	public static final int PRIMITVE_TRIANGLES = 0x03000000;
	public static final int PRIMITVE_QUADS = 0x04000000;
	public static final int PRIMITVE_POINT_SPRITES = 0x05000000;

	//Internal flags
	private static final int PROJ_PARALLEL = 0;
	private static final int PROJ_PERSPECTIVE = 1;
	
	private static final int PATTR_BLEND_MASK = PATTR_BLEND_SUB;
    // Legacy primitive bit 0x08 is ignored; ENV_ATTR_SEMI_TRANSPARENT belongs to COMMAND_ATTRIBUTE.
    private static final int PATTR_LEGACY_IGNORED = 0x08;
	
	private static final int PDATA_NORMAL_MASK = 0x0300;
	private static final int PDATA_SPRITE_PARAMS_MASK = 0x3000;
	private static final int PDATA_COLOR_MASK = 0x0C00;
	
	private static final int PDATA_NORMAL_INVALID = 0x0100;
	private static final int PDATA_COLOR_INVALID = 0x0C00;

	private static final int PRIM_TYPE_POLY_FLAG = Figure.MAT_UNUSED_1; //Reusing unused material flag
	private static final int PRIM_TYPE_POINT = 0x00;
	private static final int PRIM_TYPE_LINE = 0x01;
	private static final int PRIM_TYPE_SPRITE = 0x02;
	private static final int PRIM_TYPE_MASK = 0x03;
	private static final int PRIM_MAT_TOON = Figure.MAT_DOUBLE_FACE; //Reusing double face material flag
	
	//Instance fields
	private Graphics boundGraphics;
	private int[] frameBuffer;
	private int fbWidth, fbHeight;
	private int clipX, clipY, clipW, clipH;
	private int originX, originY;
	private int bindClipX, bindClipY, bindClipW, bindClipH;
	private static native int _targetWidth(Graphics graphics);
	private static native int _targetHeight(Graphics graphics);
	private static native boolean _readTarget(Graphics graphics, int[] pixels);
	private static native boolean _writeTarget(Graphics graphics, int[] pixels, int x, int y, int w, int h);
	
	
	private boolean disposed;

	//Active projection parameters
	private int projectionMode;
	private int projNear, projFar;
	private int projScaleX, projScaleY;
	private int drawCenterX, drawCenterY;
	
	//Active effect parameters
	private int efxToonThreshold, efxToonLow, efxToonHigh;
	private boolean efxToon, efxTransparency;
	private Light efxLight;
	private Texture efxSphereTex;
	
	private Light g3dLight;
	private Texture g3dSphereTex;

	//Primitive data for sorting and rendering
	//[0] - Sort Z
	
	//For polygons:
	//[1] - Envmap id (12 bits), Texture id (12 bits), Bottom 8 bits: material flags + poly flag + toon shading
	//[2-13] - X1, Y1, X2, Y2, X3, Y3 as low/high 64-bit word pairs
	//[14+, Optional] - Polygon color
	//Optional - Vtx 1 U, V (in one int) (fp8.8 precision for clipping)
	//Optional - Vtx 2 U, V
	//Optional - Vtx 3 U, V
	//Optional - Shade level for vtx 3, 2, 1 (fp 5.5 precision)
	//Optional - Vtx 1 envmap U, V, Vtx 2 envmap U (fp 6.4)
	//Optional - Vtx 2 envmap V, Vtx 3 envmap U, V (fp 6.4)
	
	//For primitives:
	//[1] - Primitive color (24 bits) (or 12 bit angle + 12 bit tex id), 8 bit primitive attributes (blend mode, color key, primitive type)
	//For points: [2-3] - X1, Y1
	//For lines: [2-5] - X1, Y1, X2, Y2
	//For sprites - [2-5] - Center x, y as 64-bit word pairs
	//For sprites - [6-13] - Four projected sizes as 64-bit word pairs
	//For sprites - [14-17] - Texture u0, v0, u1, v1
	private int[] primData;
    private int[] primitiveClip;
    private int[] clipRegions = new int[16];
    private int clipRegionUsed, activeClip;
	private int primDataUsed;
	private int primDataReserved;
	
	//Offsets into primitive data array for each primitive
	private int[] sortPrimIdx;
	private int sortPrimCount;
	private int sortPrimReserved;
	
	private Texture[] textures;
    private Texture[] textureSources;
    // Persistent environment selection is independent of one flush's captured textures.
    // All references are Java-owned so GC and save/restore see the same state.
    private Texture[] selectedTextures;
    private int selectedTextureIndex;
	private int bindTextures;
	
	//Temporary buffers for T&L
	//[Tx, Ty], [Px, Py, Z], [Shade level (12 bits)], [Env U, Env V] (fp 6.4 precision)
	private int[] tranVtx, projVtx;
    private long[] projectedXY;
	private short[] lightVtx, envUVs;

	private AffineTrans[] boneTransforms;
	private Vector3D tmpVec;
	
	//Clipping
	static final int NEAR_CLIP = 1, FAR_CLIP = 2, TOON_SPLIT = 4;
	
	private int allowedClippingStages;
	private int activeClippingStages;
	
	private boolean clipPolyHasUVs, clipPolyFlatNorm;
	private boolean clipPolyHasLight, clipPolyEnvmap;
	private int clipAttsCount;

	private int clipPolyMat, clipPolyTexCol, clipPolyEnvmapId, clipPolyNormalId;

	//Clip vertices input & outputs
	private int[][] clipBuffers;

	private int[] tmpClipProjVtx;
    private long[] tmpClipProjectedXY;
	private short[] tmpClipLightVtx, tmpClipEnvUVs;

	public Graphics3D() {
		//Prealloc temp buffers

		
		efxLight = g3dLight = new Light();
	
		final int polyCount = 512;
		primData = new int[polyCount * 8];
        primitiveClip = new int[primData.length];
		sortPrimIdx = new int[polyCount];
		
		final int vtxCount = 256;
		tranVtx = new int[vtxCount * 2];
		projVtx = new int[vtxCount * 3];
        projectedXY = new long[vtxCount * 2];
		lightVtx = new short[vtxCount];
		envUVs = new short[vtxCount * 2];
		
		textures = new Texture[32];
        textureSources = new Texture[32];
		
		final int boneCount = 10;
		boneTransforms = new AffineTrans[boneCount];
		tmpVec = new Vector3D();

		for (int i = 0; i < boneCount; i++) {
			boneTransforms[i] = new AffineTrans();
		}
		
		//Clipping buffers
		clipBuffers = new int[][] {
			new int[(3 + 2 + 1 + 2) * 3], //Clipping input
			new int[(3 + 2 + 1 + 2) * 4], //Near plane clip output
			new int[(3 + 2 + 1 + 2) * 6], //Far plane clip output
			new int[(3 + 2 + 1 + 2) * 12],//Toon clip output 1
			new int[(3 + 2 + 1 + 2) * 12] //Toon clip output 2
		};

		tmpClipProjVtx = new int[12 * 3];
        tmpClipProjectedXY = new long[12 * 2];
		tmpClipLightVtx = new short[12];
		tmpClipEnvUVs = new short[12 * 2];
	}
	
	public final void dispose() {
		disposed = true;
		boundGraphics = null;
		frameBuffer = null;

		
		efxLight = g3dLight = null;
		efxSphereTex = g3dSphereTex = null;
		
		primData = primitiveClip = clipRegions = null;
		sortPrimIdx = null;
		
		tranVtx = null;
		projVtx = null; projectedXY = null;
		lightVtx = envUVs = null;
		
		textures = textureSources = selectedTextures = null;
		
		boneTransforms = null;
		tmpVec = null;
		
		//Clipping buffers
		clipBuffers = null;

		tmpClipProjVtx = null; tmpClipProjectedXY = null;
		tmpClipLightVtx = null;
		tmpClipEnvUVs = null;
	}

	private final void preallocBoneTransforms(int size) {
		if(boneTransforms.length < size) {
			boneTransforms = new AffineTrans[size];

			for (int i = 0; i < size; i++) {
				boneTransforms[i] = new AffineTrans();
			}
		}
	}
	
	private final void reservePrimBuffers(int primCount, int primDataSize) {
        primDataReserved = primDataUsed; sortPrimReserved = sortPrimCount;
		long needed = (long)primCount * primDataSize + primDataReserved;
        if (primCount < 0 || primDataSize < 0 || needed > Integer.MAX_VALUE - 8 ||
            (long)sortPrimReserved + primCount > Integer.MAX_VALUE - 8)
            throw new IllegalArgumentException("Primitive queue overflow");
        primDataSize *= primCount;
		
		if (primData.length < primDataReserved + primDataSize) {
			int newSize = Math.max(
					primDataReserved + primDataSize, 
					primData.length * 3 / 2
			);
			
			int[] newBuf = new int[newSize];
			System.arraycopy(primData, 0, newBuf, 0, primDataReserved);
			primData = newBuf;
            int[] clips = new int[newSize];
            System.arraycopy(primitiveClip, 0, clips, 0, primDataUsed);
            primitiveClip = clips;
		}
		
		if (sortPrimIdx.length < sortPrimReserved + primCount) {
			int newSize = Math.max(
					sortPrimReserved + primCount, 
					sortPrimIdx.length * 3 / 2
			);
			
			int[] newBuf = new int[newSize];
			System.arraycopy(sortPrimIdx, 0, newBuf, 0, sortPrimReserved);
			sortPrimIdx = newBuf;
		}
	}
	
	private final void flushPrimBufferReserved() {
		primDataReserved = primDataUsed;
		sortPrimReserved = sortPrimCount;
	}
	
	private final void preallocVtxBuffers(int vertsCount) {
		if (tranVtx.length < vertsCount * 2) {
			int newSize = Math.max(vertsCount, tranVtx.length / 2 * 3 / 2);
			tranVtx = new int[newSize * 2];
			projVtx = new int[newSize * 3];
            projectedXY = new long[newSize * 2];
		}
	}
	
	private final void preallocLightBuffers(int vertsCount, boolean envMapping) {
		if (lightVtx.length < vertsCount) {
			int newSize = Math.max(vertsCount, lightVtx.length * 3 / 2);
			lightVtx = new short[newSize];
		}
		
		if (envMapping && envUVs.length < vertsCount * 2) {
			int newSize = Math.max(vertsCount, envUVs.length / 2 * 3 / 2);
			envUVs = new short[newSize * 2];
		}
	}

	public final void bind(Graphics graphics) {
        if (disposed) return;
        if (graphics == null) throw new NullPointerException();
        if (boundGraphics != null) throw new IllegalStateException();
        int width = _targetWidth(graphics), height = _targetHeight(graphics);
        if (width <= 0 || height <= 0 || (long) width * height > Integer.MAX_VALUE)
            throw new IllegalArgumentException("Invalid render target");
        fbWidth = width; fbHeight = height;
        if (frameBuffer == null || frameBuffer.length < width * height)
            frameBuffer = new int[width * height];
        originX = graphics.getTranslateX(); originY = graphics.getTranslateY();
        bindClipX = clampLong((long)graphics.getClipX() + originX, 0, width);
        bindClipY = clampLong((long)graphics.getClipY() + originY, 0, height);
        int right = clampLong((long)graphics.getClipX() + originX + graphics.getClipWidth(), bindClipX, width);
        int bottom = clampLong((long)graphics.getClipY() + originY + graphics.getClipHeight(), bindClipY, height);
        bindClipW = right - bindClipX; bindClipH = bottom - bindClipY;
        boundGraphics = graphics;
        clipX = bindClipX; clipY = bindClipY; clipW = bindClipW; clipH = bindClipH;
        resetClipRegions();
        allowedClippingStages = NEAR_CLIP | FAR_CLIP | TOON_SPLIT;
    }

    private static long addCoordinate(long a, long b) {
        long sum=a+b;
        return ((a^sum)&(b^sum))<0 ? (a<0 ? Long.MIN_VALUE : Long.MAX_VALUE) : sum;
    }

    private static long spriteTerm(int trig, long size) {
        return (size >> 13) * trig + (((size & 8191) * trig) >> 13);
    }

    private static long spriteCoordinate(long center, int trigA, long sizeA, int signA, int trigB, long sizeB, int signB) {
        return addCoordinate(addCoordinate(center,spriteTerm(trigA,sizeA)*signA),spriteTerm(trigB,sizeB)*signB);
    }

    private static void putCoordinate(int[] data, int offset, long value) {
        data[offset] = (int)value; data[offset + 1] = (int)(value >> 32);
    }

    private static long getCoordinate(int[] data, int offset) {
        return ((long)data[offset + 1] << 32) | ((long)data[offset] & 0xffffffffL);
    }

    private static boolean wideCoordinate(long value) { return value < -32768 || value > 32767; }

    private int averageDepth(int start, int count) {
        long sum = 0;
        for (int i = 0; i < count; i++) sum += projVtx[(start+i)*3+2];
        return (int)(sum/count);
    }

    private static int transformCoordinate(int x, int y, int z, int a, int b, int c, int translate) {
        long p = (long)a*x, q = (long)b*y, r = (long)c*z;
        long sum = p + q;
        if (((p ^ sum) & (q ^ sum)) < 0) return p < 0 ? Integer.MIN_VALUE : Integer.MAX_VALUE;
        long result = sum + r;
        if (((sum ^ result) & (r ^ result)) < 0) return sum < 0 ? Integer.MIN_VALUE : Integer.MAX_VALUE;
        if (result > Long.MAX_VALUE - 2048) return Integer.MAX_VALUE;
        return clampLong(((result + 2048) >> 12) + translate, Integer.MIN_VALUE, Integer.MAX_VALUE);
    }

    private static int fixedScale(int scale) {
        return clampLong((long)scale * 8, Integer.MIN_VALUE, Integer.MAX_VALUE);
    }

    private static long projectCoordinateLong(int value, int scale, int divisor, int center) {
        return (divisor == 0 ? ((long)value * scale) >> 12 : (long)value * scale / divisor) + center;
    }

    private static int projectCoordinate(int value, int scale, int divisor, int center) {
        return clampLong(projectCoordinateLong(value, scale, divisor, center), Integer.MIN_VALUE, Integer.MAX_VALUE);
    }

    private static int clampLong(long value, int min, int max) {
        return value < min ? min : value > max ? max : (int)value;
    }

    private void clearQueue() {
        primDataUsed = sortPrimCount = 0;
        flushPrimBufferReserved();
        for (int i = 0; i < bindTextures; i++) { textures[i] = null; textureSources[i] = null; }
        resetClipRegions();
        bindTextures = 0;
    }

    public final void flush() {
        if (disposed) return;
        if (boundGraphics == null) throw new IllegalStateException();
        if (sortPrimCount != 0) {
            if (_targetWidth(boundGraphics) != fbWidth || _targetHeight(boundGraphics) != fbHeight)
                throw new IllegalStateException("Render target resized while bound");
            if (!_readTarget(boundGraphics, frameBuffer)) throw new IllegalStateException("Unreadable render target");
            if (sortPrimCount > 1) quickSort(0, sortPrimCount - 1);
            flushPrimitives();
            if (!_writeTarget(boundGraphics, frameBuffer, bindClipX, bindClipY, bindClipW, bindClipH))
                throw new IllegalStateException("Unwritable render target");
        }
        clearQueue();
    }

    public final void release(Graphics graphics) {
        if (disposed) return;
        if (graphics == null) throw new NullPointerException();
        if (graphics != boundGraphics) throw new IllegalArgumentException("Different render target");
        // release discards unflushed geometry; drawing is observable only at flush.
        clearQueue();
        boundGraphics = null;
    }

    private int comparePrimitive(int a, int b) {
        int za = primData[a], zb = primData[b];
        if (za != zb) return za > zb ? -1 : 1;
        return a < b ? -1 : a > b ? 1 : 0;
    }

	private final void quickSort(int low, int high) {
		if (high - low <= 25) {
			insertionSort(low, high);
			return;
		}
		
		int[] sortPrimIdx = this.sortPrimIdx;
		int[] primData = this.primData;

		int pivot = sortPrimIdx[(low + high) >> 1];
		int i = low, j = high;

		while (i <= j) {
			while (comparePrimitive(sortPrimIdx[i], pivot) < 0) i++;
			while (comparePrimitive(sortPrimIdx[j], pivot) > 0) j--;
			
			if (i <= j) {
				int tp = sortPrimIdx[i]; 
				sortPrimIdx[i] = sortPrimIdx[j]; 
				sortPrimIdx[j] = tp;
				
				i++; j--;
			}
		}

		if (low < j) quickSort(low, j);
		if (i < high) quickSort(i, high);
	}

	private final void insertionSort(int low, int high) {
		int[] sortPrimIdx = this.sortPrimIdx;
		int[] primData = this.primData;
		
		for (int i = low + 1; i <= high; i++) {
			int tp = sortPrimIdx[i];
			int tz = primData[sortPrimIdx[i]];
			int j = i - 1;
			
			while (j >= low && comparePrimitive(sortPrimIdx[j], tp) > 0) {
				sortPrimIdx[j + 1] = sortPrimIdx[j];
				j--;
			}
			
			sortPrimIdx[j + 1] = tp;
		}
	}

	private final void flushPrimitives() {
		int[] primData = this.primData;
		int[] sortPrimIdx = this.sortPrimIdx;
		int sortPrimCount = this.sortPrimCount;
		Texture[] textures = this.textures;
		
		int[] frameBuffer = this.frameBuffer;
		int fbWidth = this.fbWidth, fbHeight = this.fbHeight;
		int clipX1 = clipX, clipY1 = clipY;
		int clipX2 = clipX1 + clipW, clipY2 = clipY1 + clipH;
		
		
		for (int p = 0; p < sortPrimCount; p++) {
			int sortEntry = sortPrimIdx[p];
            int clip = primitiveClip[sortEntry];
            clipX1 = clipRegions[clip]; clipY1 = clipRegions[clip + 1];
            clipX2 = clipRegions[clip + 2]; clipY2 = clipRegions[clip + 3];
            sortEntry++; //Skip sort z
			
			int header = primData[sortEntry++];
			
			if ((header & PRIM_TYPE_POLY_FLAG) != 0) {
				flushPolygon(
						header, primData, sortEntry,
						clipX1, clipY1, clipX2, clipY2
				);
			} else {
				int primType = header & PRIM_TYPE_MASK;
				int blendMode = (header & PATTR_BLEND_MASK) >> 5;

				switch (primType) {
					case PRIM_TYPE_POINT: {
						int x = primData[sortEntry];
						if (x < clipX1 || x >= clipX2) continue;
						int y = primData[sortEntry + 1];
						if (y < clipY1 || y >= clipY2) continue;
						//sortEntry += 2;

						int drawX = x + y * fbWidth;
						int color = 0xff000000 | (header >>> 8);

						if (blendMode != 0) {
							color = Rasterizer.blendPixel(color, frameBuffer[drawX], blendMode);
						}

						frameBuffer[drawX] = color;

						break;
					}
					case PRIM_TYPE_LINE: {
                        int x1 = primData[sortEntry];
                        int y1 = primData[sortEntry + 1];
                        int x2 = primData[sortEntry + 2];
                        int y2 = primData[sortEntry + 3];
                        //sortEntry += 4;

						int color = 0xff000000 | (header >>> 8);

						Rasterizer.drawLine(
								frameBuffer, fbWidth, 
								clipX1, clipY1, clipX2, clipY2,
								x1, y1, x2, y2, 
								color, blendMode
						);
						break;
					}
					case PRIM_TYPE_SPRITE: {
                        int angle=header>>>20;
                        Texture tex=textures[(header>>8)&4095];
                        boolean colorKey=(header&PATTR_COLORKEY)!=0;
                        long x=getCoordinate(primData,sortEntry), y=getCoordinate(primData,sortEntry+2);
                        long sprWX=getCoordinate(primData,sortEntry+4), sprHX=getCoordinate(primData,sortEntry+6);
                        long sprWY=getCoordinate(primData,sortEntry+8), sprHY=getCoordinate(primData,sortEntry+10);
                        int u0=primData[sortEntry+12]<<Rasterizer.fp, v0=primData[sortEntry+13]<<Rasterizer.fp;
                        int u1=primData[sortEntry+14]<<Rasterizer.fp, v1=primData[sortEntry+15]<<Rasterizer.fp;
                        int sin=Util3D.sine(angle,4096), cos=Util3D.sine(angle+1024,4096);
                        long x1=spriteCoordinate(x,cos,sprWX,-1,sin,sprHX,1), y1=spriteCoordinate(y,cos,sprHY,-1,sin,sprWY,-1);
                        long x2=spriteCoordinate(x,cos,sprWX,1,sin,sprHX,1), y2=spriteCoordinate(y,cos,sprHY,-1,sin,sprWY,1);
                        long x3=spriteCoordinate(x,cos,sprWX,1,sin,sprHX,-1), y3=spriteCoordinate(y,cos,sprHY,1,sin,sprWY,1);
                        long x4=spriteCoordinate(x,cos,sprWX,-1,sin,sprHX,-1), y4=spriteCoordinate(y,cos,sprHY,1,sin,sprWY,-1);
                        int shade=tex.palette.length==256 ? 0 : 31<<Rasterizer.fp;
                        if (wideCoordinate(x1)||wideCoordinate(y1)||wideCoordinate(x2)||wideCoordinate(y2)||
                            wideCoordinate(x3)||wideCoordinate(y3)||wideCoordinate(x4)||wideCoordinate(y4)) {
                            Rasterizer.fillTriangleWide(frameBuffer,fbWidth,clipX1,clipY1,clipX2,clipY2,
                                x1,y1,x2,y2,x3,y3,tex,0,colorKey,u0,v0,u1,v0,u1,v1,shade,shade,shade,
                                null,0,0,0,0,0,0,blendMode);
                            Rasterizer.fillTriangleWide(frameBuffer,fbWidth,clipX1,clipY1,clipX2,clipY2,
                                x1,y1,x3,y3,x4,y4,tex,0,colorKey,u0,v0,u1,v1,u0,v1,shade,shade,shade,
                                null,0,0,0,0,0,0,blendMode);
                        } else {
                            Rasterizer.fillTriangleAffineT(frameBuffer,fbWidth,clipX1,clipY1,clipX2,clipY2,
                                (int)x1,(int)y1,(int)x2,(int)y2,(int)x3,(int)y3,u0,v0,u1,v0,u1,v1,tex,colorKey,shade,blendMode);
                            Rasterizer.fillTriangleAffineT(frameBuffer,fbWidth,clipX1,clipY1,clipX2,clipY2,
                                (int)x1,(int)y1,(int)x3,(int)y3,(int)x4,(int)y4,u0,v0,u1,v1,u0,v1,tex,colorKey,shade,blendMode);
                        }
                        break;
                    }
				}
			}
		}
	}
	
	private final void flushPolygon(
			int header, int[] polyData, int sortEntry,
			int clipX1, int clipY1, int clipX2, int clipY2
	) {
		int blendMode = (header & Figure.MAT_BLEND_MASK) >> 1;

		boolean lighting = (header & Figure.MAT_LIGHTING) != 0;
		boolean envMapping = lighting && (header & Figure.MAT_SPECULAR) != 0;
		
		int texCol = (header >>> 8) & 4095;
		boolean isColorPoly = texCol == 4095;

        long wx0 = getCoordinate(polyData, sortEntry), wy0 = getCoordinate(polyData, sortEntry + 2);
        long wx1 = getCoordinate(polyData, sortEntry + 4), wy1 = getCoordinate(polyData, sortEntry + 6);
        long wx2 = getCoordinate(polyData, sortEntry + 8), wy2 = getCoordinate(polyData, sortEntry + 10);
        boolean wide = wideCoordinate(wx0) || wideCoordinate(wy0) || wideCoordinate(wx1) ||
                wideCoordinate(wy1) || wideCoordinate(wx2) || wideCoordinate(wy2);
        int x0 = (int)wx0, y0 = (int)wy0, x1 = (int)wx1, y1 = (int)wy1, x2 = (int)wx2, y2 = (int)wy2;
        sortEntry += 12;

		if (isColorPoly) texCol = polyData[sortEntry++];

		boolean flatLighting = false;
		int as = 31 << Rasterizer.fp, bs = 31 << Rasterizer.fp, cs = 31 << Rasterizer.fp;
		
		Texture sphereTex = null;
		int aeu = 0, aev = 0, beu = 0, bev = 0, ceu = 0, cev = 0;
		
		if (lighting) {
			boolean toon = (header & PRIM_MAT_TOON) != 0;
			flatLighting = (header & Figure.MAT_FLAT_NORMAL) != 0;

			int data = polyData[sortEntry++];
			as = (data & 1023) << (Rasterizer.fp - 5);

			if (!flatLighting && !toon) {
				bs = ((data >> 10) & 1023) << (Rasterizer.fp - 5);
				cs = ((data >> 20) & 1023) << (Rasterizer.fp - 5);
			}

			if (envMapping) {
				sphereTex = textures[header >>> 20];

				data = polyData[sortEntry++];
				aeu = ((data >> 20) & 1023) << (Rasterizer.fp - 4);
				aev = ((data >> 10) & 1023) << (Rasterizer.fp - 4);

				if (!flatLighting) {
					beu = (data & 1023) << (Rasterizer.fp - 4);

					data = polyData[sortEntry++];
					bev = ((data >> 20) & 1023) << (Rasterizer.fp - 4);
					ceu = ((data >> 10) & 1023) << (Rasterizer.fp - 4);
					cev = (data & 1023) << (Rasterizer.fp - 4);
				} else {
					beu = ceu = aeu;
					bev = cev = aev;
				}
			}

			if (toon || (as == bs && bs == cs)) flatLighting = true;
            if (flatLighting) bs = cs = as;
		}

        if (isColorPoly) {
            if (wide) {
                Rasterizer.fillTriangleWide(frameBuffer, fbWidth, clipX1, clipY1, clipX2, clipY2,
                    wx0, wy0, wx1, wy1, wx2, wy2, null, texCol, false, 0, 0, 0, 0, 0, 0,
                    as, bs, cs, sphereTex, aeu, aev, beu, bev, ceu, cev, blendMode);
                return;
            }
            if (!lighting || flatLighting) {
				if (!envMapping) {
					Rasterizer.fillTriangleAffineC(
							frameBuffer, fbWidth,
							clipX1, clipY1, clipX2, clipY2,
							x0, y0, x1, y1, x2, y2,
							texCol,
							as,
							blendMode
					);
				} else {
					Rasterizer.fillTriangleAffineCE(
							frameBuffer, fbWidth,
							clipX1, clipY1, clipX2, clipY2,
							x0, y0, x1, y1, x2, y2,
							texCol,
							as,
							aeu, aev, beu, bev, ceu, cev, 
							sphereTex,
							blendMode
					);
				}
			} else {
				if (!envMapping) {
					Rasterizer.fillTriangleAffineCL(
							frameBuffer, fbWidth,
							clipX1, clipY1, clipX2, clipY2,
							x0, y0, x1, y1, x2, y2,
							texCol,
							as, bs, cs,
							blendMode
					);
				} else {
					Rasterizer.fillTriangleAffineCLE(
							frameBuffer, fbWidth,
							clipX1, clipY1, clipX2, clipY2,
							x0, y0, x1, y1, x2, y2,
							texCol,
							as, bs, cs,
							aeu, aev, beu, bev, ceu, cev, 
							sphereTex,
							blendMode
					);
				}
			}
		} else {
			boolean useColorKey = (header & Figure.MAT_COLORKEY) != 0;

			int data = polyData[sortEntry];
			int au = (data >>> 16) << (Rasterizer.fp - 8);
			int av = (data & 0x0000ffff) << (Rasterizer.fp - 8);

			data = polyData[sortEntry + 1];
			int bu = (data >>> 16) << (Rasterizer.fp - 8);
			int bv = (data & 0x0000ffff) << (Rasterizer.fp - 8);

			data = polyData[sortEntry + 2];
			sortEntry += 3;
			int cu = (data >>> 16) << (Rasterizer.fp - 8);
			int cv = (data & 0x0000ffff) << (Rasterizer.fp - 8);

			Texture tex = textures[texCol];

			if (tex.palette.length == 256) {
				if (lighting) tex.generateShadedPalette();
				else as = 0;
			}

            if (!lighting) bs = cs = as;
            if (wide) {
                Rasterizer.fillTriangleWide(frameBuffer, fbWidth, clipX1, clipY1, clipX2, clipY2,
                    wx0, wy0, wx1, wy1, wx2, wy2, tex, 0, useColorKey, au, av, bu, bv, cu, cv,
                    as, bs, cs, sphereTex, aeu, aev, beu, bev, ceu, cev, blendMode);
                return;
            }
			if (!lighting || flatLighting) {
				if (!envMapping) {
					Rasterizer.fillTriangleAffineT(
							frameBuffer, fbWidth,
							clipX1, clipY1, clipX2, clipY2,
							x0, y0, x1, y1, x2, y2,
							au, av, bu, bv, cu, cv,
							tex, useColorKey,
							as,
							blendMode
					);
				} else {
					Rasterizer.fillTriangleAffineTE(
							frameBuffer, fbWidth,
							clipX1, clipY1, clipX2, clipY2,
							x0, y0, x1, y1, x2, y2,
							au, av, bu, bv, cu, cv,
							tex, useColorKey,
							as,
							aeu, aev, beu, bev, ceu, cev, 
							sphereTex,
							blendMode
					);
				}
			} else {
				if (!envMapping) {
					Rasterizer.fillTriangleAffineTL(
							frameBuffer, fbWidth,
							clipX1, clipY1, clipX2, clipY2,
							x0, y0, x1, y1, x2, y2,
							au, av, bu, bv, cu, cv,
							tex, useColorKey,
							as, bs, cs,
							blendMode
					);
				} else {
					Rasterizer.fillTriangleAffineTLE(
							frameBuffer, fbWidth,
							clipX1, clipY1, clipX2, clipY2,
							x0, y0, x1, y1, x2, y2,
							au, av, bu, bv, cu, cv,
							tex, useColorKey,
							as, bs, cs,
							aeu, aev, beu, bev, ceu, cev, 
							sphereTex,
							blendMode
					);
				}
			}
		}
	}
	
	private final void setCenter(FigureLayout layout, int x, int y) {
		if (layout != null) {
			drawCenterX = clampLong((long)layout.getCenterX() + x + originX, Integer.MIN_VALUE, Integer.MAX_VALUE);
			drawCenterY = clampLong((long)layout.getCenterY() + y + originY, Integer.MIN_VALUE, Integer.MAX_VALUE);
		} else {
			drawCenterX = clampLong((long)x + originX, Integer.MIN_VALUE, Integer.MAX_VALUE);
			drawCenterY = clampLong((long)y + originY, Integer.MIN_VALUE, Integer.MAX_VALUE);
		}
	}

	private final void setProjection(FigureLayout layout) {
		int projection = layout.projectionMode;

		if (projection == 0) {
			setOrthographicScale(fixedScale(layout.getScaleX()), fixedScale(layout.getScaleY()));
		} else if (projection == 1) {
			setOrthographicWH(layout.getParallelWidth(), layout.getParallelHeight());
		} else if (projection == 2) {
			setPerspectiveFov(layout.zNear, layout.zFar, layout.angle);
		} else {
			setPerspectiveWH(layout.zNear, layout.zFar, layout.perspectiveWidth, layout.perspectiveHeight);
		}
	}
	
	private final void setOrthographicScale(int scaleX, int scaleY) {
		projectionMode = PROJ_PARALLEL;
		projScaleX = scaleX;
		projScaleY = scaleY;
	}
	
	private final void setOrthographicWH(int w, int h) {
		if (w < 0 || h < 0) throw new IllegalArgumentException();

		projectionMode = PROJ_PARALLEL;
		projScaleX = w == 0 ? 0 : clampLong(((long)fbWidth << 12) / w, 0, Integer.MAX_VALUE);
		projScaleY = h == 0 ? 0 : clampLong(((long)fbHeight << 12) / h, 0, Integer.MAX_VALUE);
	}
	
	private final void setPerspectiveFov(int near, int far, int angle) {
		if (near >= far || near < 1 || far > 32767 || angle < 1 || angle > 2047) return;
		
		projectionMode = PROJ_PERSPECTIVE;

		projNear = near;
		projFar = far;

		float scale = 0.5f / (float) Math.tan(angle / 4096.0f * Math.PI);
		
		projScaleX = (int) (fbWidth * scale);

		projScaleY = projScaleX;
	}
	
	private final void setPerspectiveWH(int near, int far, int w, int h) {
		validatePerspective(near, far);
        if (w < 0 || h < 0) throw new IllegalArgumentException();

		projectionMode = PROJ_PERSPECTIVE;

		projNear = near;
		projFar = far;

		projScaleX = w == 0 ? 0 : clampLong(((long)fbWidth << 12) * projNear / w, 0, Integer.MAX_VALUE);
		projScaleY = h == 0 ? 0 : clampLong(((long)fbHeight << 12) * projNear / h, 0, Integer.MAX_VALUE);
	}
	
	private final void setEffect(Effect3D effect) {
		if (effect.light != null) {
			efxLight = g3dLight;
			efxLight.getParallelLightDirection().set(effect.light.getParallelLightDirection());
			efxLight.setParallelLightIntensity(effect.light.getParallelLightIntensity());
			efxLight.setAmbientIntensity(effect.light.getAmbientIntensity());
		} else {
			efxLight = null;
		}
		
		efxToon = effect.shadingType == Effect3D.TOON_SHADING;
		efxTransparency = effect.transparency;
		
		efxSphereTex = g3dSphereTex = effect.sphereTexture;

		efxToonThreshold = effect.toonThreshold;
		efxToonLow = effect.toonLow;
		efxToonHigh = effect.toonHigh;
	}
	
    private void resetClipRegions() {
        activeClip = 0; clipRegionUsed = 4;
        clipRegions[0] = clipX; clipRegions[1] = clipY;
        clipRegions[2] = clipX + clipW; clipRegions[3] = clipY + clipH;
    }

    private final void setClip(int left, int top, int right, int bottom) {
        int x1 = clampLong((long)left + originX, bindClipX, bindClipX + bindClipW);
        int y1 = clampLong((long)top + originY, bindClipY, bindClipY + bindClipH);
        int x2 = clampLong((long)right + originX, x1, bindClipX + bindClipW);
        int y2 = clampLong((long)bottom + originY, y1, bindClipY + bindClipH);
        clipX = x1; clipY = y1; clipW = x2 - x1; clipH = y2 - y1;
        if (clipRegionUsed > clipRegions.length - 4) {
            int[] grown = new int[clipRegions.length * 2];
            System.arraycopy(clipRegions, 0, grown, 0, clipRegionUsed);
            clipRegions = grown;
        }
        activeClip = clipRegionUsed;
        clipRegions[clipRegionUsed++] = x1; clipRegions[clipRegionUsed++] = y1;
        clipRegions[clipRegionUsed++] = x2; clipRegions[clipRegionUsed++] = y2;
    }

    private static void validateModelTexture(Texture texture) {
        if (texture == null) throw new NullPointerException();
        if (!texture.isForModel) throw new IllegalArgumentException("Model texture required");
        if (texture.bitmapData == null || texture.palette == null) throw new IllegalStateException("Disposed texture");
    }

    private static void validateSelectionArray(Texture[] textures) {
        if (textures != null) for (int i=0;i<textures.length;i++) validateModelTexture(textures[i]);
    }

    private static Texture selectedTexture(Texture[] textures,int index) {
        return textures == null || index < 0 || index >= textures.length ? null : textures[index];
    }

    private void selectTexture(Texture texture) {
        if (selectedTextures == null || selectedTextures.length != 1) selectedTextures=new Texture[1];
        selectedTextures[0]=texture;
        selectedTextureIndex=0;
    }

    private void selectTextureArray(Texture[] textures) {
        // Null/empty arrays leave the environment unchanged, like RenderProxy.
        if (textures == null || textures.length == 0) return;
        if (selectedTextures == null || selectedTextures.length != textures.length)
            selectedTextures=new Texture[textures.length];
        System.arraycopy(textures,0,selectedTextures,0,textures.length);
    }

    private final int bindTexture(Texture tex) {
        if (tex == null) throw new NullPointerException();
        if (tex.isForModel ? tex.palette == null || tex.bitmapData == null : tex.envmapData == null) throw new IllegalStateException("Disposed texture");
        for (int i = 0; i < bindTextures; i++) if (textureSources[i] == tex) return i;
        if (bindTextures == 4095) throw new IllegalArgumentException("Too many textures");
        if (bindTextures == textures.length) {
            Texture[] grown = new Texture[textures.length * 3 / 2];
            System.arraycopy(textures, 0, grown, 0, bindTextures);
            textures = grown;
            Texture[] sources = new Texture[grown.length];
            System.arraycopy(textureSources, 0, sources, 0, bindTextures);
            textureSources = sources;
        }
        textureSources[bindTextures] = tex;
        textures[bindTextures] = tex._capture();
        return bindTextures++;
    }

	public final void drawFigure(
			Figure figure, int x, int y,
			FigureLayout layout, Effect3D effect
	) {
        queueFigure(figure,x,y,layout,effect,true);
        flush();
	}

	public final void renderFigure(
			Figure figure, int x, int y,
			FigureLayout layout, Effect3D effect
	) {
        queueFigure(figure,x,y,layout,effect,false);
    }

    private void queueFigure(Figure figure,int x,int y,FigureLayout layout,Effect3D effect,boolean immediate) {
		if (disposed) return;
		if (boundGraphics == null) throw new IllegalStateException();
		if (figure == null || layout == null || effect == null) {
			throw new NullPointerException();
		}

        if (figure.vertices == null || figure.bones == null || figure.patterns == null)
            throw new IllegalStateException("Disposed figure");
        validateSelectionArray(figure.textures);
		AffineTrans viewTrans = layout.getAffineTrans();
		setCenter(layout, x, y);
		setProjection(layout);
		setEffect(effect);
		
		boolean useBlending = efxTransparency && ((figure.allMatsOr & Figure.MAT_BLEND_MASK) != 0);

		boolean useLighting = 
				efxLight != null && 
				figure.normals != null && 
				((figure.allMatsOr & Figure.MAT_LIGHTING) != 0);
		
		boolean useEnvMap = 
				useLighting &&
				efxSphereTex != null &&
				((figure.allMatsOr & Figure.MAT_SPECULAR) != 0);
		
		processFigureVertices(
				figure, viewTrans,
				useLighting, useEnvMap
		);

		submitFigurePolygons(figure, effect.sphereTexture, useBlending, useLighting, useEnvMap);
        if (immediate) {
            Texture texture=figure.getTexture();
            if (texture != null) selectTexture(texture);
        } else selectTextureArray(figure.textures);
	}
	
	private final void processFigureVertices(
			Figure figure, AffineTrans viewTrans,
			boolean useLighting, boolean useEnvMap
	) {
		short[] verts = figure.vertices;
		short[] normals = figure.normals;
		int numVerts = figure.numVertices;
		
		preallocVtxBuffers(numVerts);
		if (useLighting) preallocLightBuffers(numVerts, useEnvMap);

		Figure.Bone[] bones = figure.bones;
		preallocBoneTransforms(bones.length);

		for (int bIdx = 0; bIdx < bones.length; bIdx++) {
			Figure.Bone bone = bones[bIdx];
			int vtxStart = bone.startVertex;
			int vtxCount = bone.numVertices;
			
			figure.updateBoneTrans(bIdx, viewTrans, boneTransforms);
			AffineTrans trans = boneTransforms[bIdx];
			
			processVertices(verts, null, vtxStart * 3, vtxStart, vtxCount, trans);
			
			if (!useLighting) continue;
			processLighting(normals, null, vtxStart * 3, vtxStart, vtxCount, trans, useEnvMap);
		}
	}

	private final void submitFigurePolygons(
			Figure figure, Texture envMap,
			boolean useBlending, boolean useLighting, boolean useEnvMap
	) {
		int envmapTexId = 0;
		if (useEnvMap) {
			bindTexture(envMap);
			envmapTexId = bindTexture(envMap);
		}
		
		int materialMaskAnd = Figure.MAT_MASK;
		if (!useBlending) materialMaskAnd &= ~Figure.MAT_BLEND_MASK;
		if (!useLighting) materialMaskAnd &= ~Figure.MAT_LIGHTING;
		if (!useEnvMap) materialMaskAnd &= ~Figure.MAT_SPECULAR;
		
		boolean flatNormals = (figure.allMatsAnd & Figure.MAT_FLAT_NORMAL) != 0;
		
		int polyTexStride = calcPolygonStride(true, flatNormals, useLighting, useEnvMap);
		
		
		int polyColStride = calcPolygonStride(false, flatNormals, useLighting, useEnvMap);
		reservePrimBuffers(figure.numPolyT3 + figure.numPolyT4 * 2 + figure.numPolyC3 + figure.numPolyC4 * 2, Math.max(polyTexStride, polyColStride));
		
		Texture[] texs = figure.textures;
		int selectedTex = figure.textureIndex;
		
		if (texs != null) {
			for (int i = 0; i < texs.length; i++) {
				bindTexture(texs[i]);
			}
		}
		
		int[][][] patterns = figure.patterns;
		int selectedPattern = figure.selectedPattern;
		
		for (int p = 0; p < patterns.length; p++) {
			//Looks like there's a bug in original mcv3 implementation pattern parsing
			int pattern = p == 0 ? 0 : 1 << (p - 1);
			//int pattern = p == 0 ? 0 : 1 << (p - 1);
			if ((pattern & selectedPattern) != pattern) continue;
			
			int[][] patTexs = patterns[p];
			
			for (int t = 0; t < patTexs.length; t++) {
				int[] texData = patTexs[t];
				
				if (t > 0) {
					if (texs == null) break;
					
					int texId = t - 1;
					if (selectedTex != -1) {
						if (texId != 0) continue;
						texId = selectedTex;
					} else {
						if (texId >= texs.length) continue;
					}
					
					int g3dTexId = bindTexture(texs[texId]);
					
					submitFigureTris(figure, materialMaskAnd, envmapTexId, g3dTexId, texData[0], texData[2]);
					submitFigureQuads(figure, materialMaskAnd, envmapTexId, g3dTexId, texData[1], texData[3]);
				} else {
					if (figure.colors == null) continue;
					submitFigureTris(figure, materialMaskAnd, envmapTexId, -1, texData[0], texData[2]);
					submitFigureQuads(figure, materialMaskAnd, envmapTexId, -1, texData[1], texData[3]);
				}
			}
		}
		
		flushPrimBufferReserved();
	}
	
	private final void submitFigureTris(
			Figure figure, int materialMaskAnd, 
			int envmapTexId, int texId,
			int startIdx, int polyCount
	) {
		short[] tris = texId < 0 ? figure.polyC3 : figure.polyT3;
		int[] colors = figure.colors;
		int[] projVtx = this.projVtx;
		
		int stride = texId < 0 ? Figure.TRI_C_STRIDE : Figure.TRI_T_STRIDE;
		int offset = startIdx * stride;
		int end = offset + polyCount * stride;
		
		for (; offset < end; offset += stride) {
			int readOffset = offset;
			int mat = tris[readOffset] & materialMaskAnd;
			
			int texCol = texId;
			if (texCol < 0) {
				texCol = colors[tris[readOffset + 1] & 0xff];
				readOffset++;
			}

			int v0 = tris[readOffset + 1] & 0xffff;
			int v1 = tris[readOffset + 2] & 0xffff;
			int v2 = tris[readOffset + 3] & 0xffff;

			if (startTriangle(mat, texCol, envmapTexId, v0, v1, v2, v0, true)) {
				if (texCol >= 0) {
					int data = tris[readOffset + 4];
					int au = (data) & 0xff00;
					int av = (data << 8) & 0xff00;

					data = tris[readOffset + 5];
					int bu = (data) & 0xff00;
					int bv = (data << 8) & 0xff00;

					data = tris[readOffset + 6];
					int cu = (data) & 0xff00;
					int cv = (data << 8) & 0xff00;

					setTriangleUVs(au, av, bu, bv, cu, cv);
				}

				int z0 = projVtx[v0 * 3 + 2];
				int z1 = projVtx[v1 * 3 + 2];
				int z2 = projVtx[v2 * 3 + 2];

				int sortZ;
				if ((mat & Figure.MAT_ZSORT_MASK) == 0) {
					sortZ = (int)(((long)z0 + z1 + z2) / 3);
				} else if ((mat & Figure.MAT_ZSORT_NEAR) != 0) {
					sortZ = z0;
					if (z1 < sortZ) sortZ = z1;
					if (z2 < sortZ) sortZ = z2;
				} else {
					sortZ = z0;
					if (z1 > sortZ) sortZ = z1;
					if (z2 > sortZ) sortZ = z2;
				}
					
				endTriangle(sortZ);
			}
		}
	}
	
	private final void submitFigureQuads(
			Figure figure, int materialMaskAnd, 
			int envmapTexId, int texId,
			int startIdx, int polyCount
	) {
		short[] quads = texId < 0 ? figure.polyC4 : figure.polyT4;
		int[] colors = figure.colors;
		int[] projVtx = this.projVtx;
		
		int stride = texId < 0 ? Figure.QUAD_C_STRIDE : Figure.QUAD_T_STRIDE;
		int offset = startIdx * stride;
		int end = offset + polyCount * stride;
		
		for (; offset < end; offset += stride) {
			int readOffset = offset;
			int mat = quads[readOffset] & materialMaskAnd;
			
			int texCol = texId;
			if (texCol < 0) {
				texCol = colors[quads[readOffset + 1] & 0xff];
				readOffset++;
			}

			int v0 = quads[readOffset + 1] & 0xffff;
			int v1 = quads[readOffset + 2] & 0xffff;
			int v2 = quads[readOffset + 3] & 0xffff;
			int v3 = quads[readOffset + 4] & 0xffff;

			int sortZ = Integer.MIN_VALUE;

			//abc
			if (startTriangle(mat, texCol, envmapTexId, v0, v1, v2, v0, true)) {
				if (texCol >= 0) {
					int data = quads[readOffset + 5];
					int au = (data) & 0xff00;
					int av = (data << 8) & 0xff00;

					data = quads[readOffset + 6];
					int bu = (data) & 0xff00;
					int bv = (data << 8) & 0xff00;

					data = quads[readOffset + 7];
					int cu = (data) & 0xff00;
					int cv = (data << 8) & 0xff00;

					setTriangleUVs(au, av, bu, bv, cu, cv);
				}
				
				int z0 = projVtx[v0 * 3 + 2];
				int z1 = projVtx[v1 * 3 + 2];
				int z2 = projVtx[v2 * 3 + 2];
				int z3 = projVtx[v3 * 3 + 2];
				
				if ((mat & Figure.MAT_ZSORT_MASK) == 0) {
					sortZ = (int)(((long)z0 + z1 + z2 + z3) >> 2);
				} else if ((mat & Figure.MAT_ZSORT_NEAR) != 0) {
					sortZ = z0;
					if (z1 < sortZ) sortZ = z1;
					if (z2 < sortZ) sortZ = z2;
					if (z3 < sortZ) sortZ = z3;
				} else {
					sortZ = z0;
					if (z1 > sortZ) sortZ = z1;
					if (z2 > sortZ) sortZ = z2;
					if (z3 > sortZ) sortZ = z3;
				}
			
				endTriangle(sortZ);
			}

			//cbd
			if (startTriangle(mat, texCol, envmapTexId, v2, v1, v3, v0, true)) {
				if (texCol >= 0) {
					int data = quads[readOffset + 6];
					int bu = (data) & 0xff00;
					int bv = (data << 8) & 0xff00;

					data = quads[readOffset + 7];
					int cu = (data) & 0xff00;
					int cv = (data << 8) & 0xff00;

					data = quads[readOffset + 8];
					int du = (data) & 0xff00;
					int dv = (data << 8) & 0xff00;

					setTriangleUVs(cu, cv, bu, bv, du, dv);
				}
				
				if (sortZ == Integer.MIN_VALUE) {
					int z0 = projVtx[v0 * 3 + 2];
					int z1 = projVtx[v1 * 3 + 2];
					int z2 = projVtx[v2 * 3 + 2];
					int z3 = projVtx[v3 * 3 + 2];
				
					if ((mat & Figure.MAT_ZSORT_MASK) == 0) {
						sortZ = (int)(((long)z0 + z1 + z2 + z3) >> 2);
					} else if ((mat & Figure.MAT_ZSORT_NEAR) != 0) {
						sortZ = z0;
						if (z1 < sortZ) sortZ = z1;
						if (z2 < sortZ) sortZ = z2;
						if (z3 < sortZ) sortZ = z3;
					} else {
						sortZ = z0;
						if (z1 > sortZ) sortZ = z1;
						if (z2 > sortZ) sortZ = z2;
						if (z3 > sortZ) sortZ = z3;
					}
				}
					
				endTriangle(sortZ);
			}
		}
	}

	public final void renderPrimitives(
			Texture texture, int x, int y,
			FigureLayout layout, Effect3D effect,
			int command, int numPrimitives,
			int[] vertexCoords, int[] normals,
			int[] textureCoords, int[] colors
	) {
		if (disposed) return;
		if (layout == null || effect == null || vertexCoords == null || normals == null || textureCoords == null || colors == null) {
			throw new NullPointerException();
		}
		if (numPrimitives <= 0 || numPrimitives > 255) {
			throw new IllegalArgumentException();
		}
		if ((command & 0x00ff0000) != 0) {
			throw new IllegalArgumentException();
		}
		if (boundGraphics == null) throw new IllegalStateException();
		
		int normalType = command & PDATA_NORMAL_MASK;
        validatePrimitive(command, numPrimitives);
        int vertices = vertexCount(command, numPrimitives);
        requireRange(vertexCoords, 0, vertices * 3);
        requireRange(normals, 0, normalWords(command, numPrimitives));
        requireRange(textureCoords, 0, textureWords(command, numPrimitives));
        requireRange(colors, 0, colorWords(command, numPrimitives));
        Texture providedTexture=texture;
        if (providedTexture != null) validateModelTexture(providedTexture);
        else texture=selectedTexture(selectedTextures,selectedTextureIndex);
        if (textureWords(command,numPrimitives) != 0) {
            if (texture == null) return; // Valid textured submission without an available selection.
            validateModelTexture(texture);
        }
        int primType=command & 0xff000000;

		AffineTrans trans = layout.getAffineTrans();
		setCenter(layout, x, y);
		setProjection(layout);
		setEffect(effect);

		switch (primType) {
			case PRIMITVE_POINTS:
				submitPoints(trans, command, numPrimitives, vertexCoords, 0, colors, 0);
				break;
			case PRIMITVE_LINES:
				submitLines(trans, command, numPrimitives, vertexCoords, 0, colors, 0);
				break;
			case PRIMITVE_TRIANGLES:
				submitPrimitivePolygons(
						texture, trans,
						command, numPrimitives, false, 
						vertexCoords, 0, normals, 0,
						textureCoords, 0, colors, 0
				);
				break;
			case PRIMITVE_QUADS:
				submitPrimitivePolygons(
						texture, trans,
						command, numPrimitives, true, 
						vertexCoords, 0, normals, 0,
						textureCoords, 0, colors, 0
				);
				break;
			case PRIMITVE_POINT_SPRITES:
				submitSprites(texture, trans, command, numPrimitives, vertexCoords, 0, textureCoords, 0);
				break;
			default:
				throw new IllegalArgumentException();
		}
        if (providedTexture != null) selectTexture(providedTexture);
	}

	public final void drawCommandList(
			Texture[] textures, int x, int y,
			FigureLayout layout, Effect3D effect,
			int[] commandList
	) {
		if (disposed) return;
		
		drawCommandList(textures, null, x, y, layout, effect, commandList);
	}

	 public final void drawCommandList(
			Texture texture, int x, int y,
			FigureLayout layout, Effect3D effect,
			int[] commandList
	) {
		if (disposed) return;
		drawCommandList(null, texture, x, y, layout, effect, commandList);
	}
	
	private final void drawCommandList(
			Texture[] textures, Texture currentTex, 
			int x, int y,
			FigureLayout layout, Effect3D effect,
			int[] commandList
	) {
		if (layout == null || effect == null || commandList == null) {
			throw new NullPointerException();
		}
		if (boundGraphics == null) throw new IllegalStateException();

        Texture suppliedTexture=currentTex;
        Texture[] suppliedArray=textures;
        if (suppliedTexture != null) {
            validateModelTexture(suppliedTexture);
            textures=new Texture[]{suppliedTexture};
        } else {
            validateSelectionArray(suppliedArray);
            if (textures == null || textures.length == 0) textures=selectedTextures;
            currentTex=selectedTexture(textures,selectedTextureIndex);
        }
        validateCommands(commandList,textures,currentTex,layout);
        if (suppliedTexture != null) selectTexture(suppliedTexture);
        else selectTextureArray(suppliedArray);
        int idx = 0;

		int version = commandList[idx++];
		if (version != COMMAND_LIST_VERSION_1_0) {
			throw new IllegalArgumentException("Unsupported command list version: 0x" + Integer.toHexString(version));
		}

		AffineTrans trans = layout.getAffineTrans();
		setCenter(layout, x, y);
		setProjection(layout);
		setEffect(effect);

		while (idx < commandList.length) {
			int cmd = commandList[idx++];
			int cmdHigh = cmd & 0xFF000000;
			
			switch (cmdHigh) {
				case COMMAND_END:
					return;
				case COMMAND_NOP:
					idx += cmd & 0xFFFFFF;
					break;
				case COMMAND_FLUSH:
					flush();
					break;
				case COMMAND_CENTER:
					drawCenterX = clampLong((long)commandList[idx++] + x + originX, Integer.MIN_VALUE, Integer.MAX_VALUE);
                    drawCenterY = clampLong((long)commandList[idx++] + y + originY, Integer.MIN_VALUE, Integer.MAX_VALUE);
					break;
				case COMMAND_TEXTURE_INDEX: {
					int texIdx = cmd & 0xFFFFFF;
                    selectedTextureIndex=texIdx;
                    currentTex=selectedTexture(selectedTextures,selectedTextureIndex);
					break;
				}
				case COMMAND_AFFINE_INDEX: {
					int transIdx = cmd & 0xFFFFFF;
					
					trans = layout._getAffineTrans(transIdx);
					break;
				}
				case COMMAND_PARALLEL_SCALE: 
					setOrthographicScale(fixedScale(commandList[idx++]), fixedScale(commandList[idx++]));
					break;
				case COMMAND_PARALLEL_SIZE: 
					setOrthographicWH(commandList[idx++], commandList[idx++]);
					break;
				case COMMAND_PERSPECTIVE_FOV: 
					setPerspectiveFov(commandList[idx++], commandList[idx++], commandList[idx++]);
					break;
				case COMMAND_PERSPECTIVE_WH: 
					setPerspectiveWH(commandList[idx++], commandList[idx++], commandList[idx++], commandList[idx++]);
					break;
				case COMMAND_ATTRIBUTE: {
					boolean lighting = (cmd & ENV_ATTR_LIGHTING) != 0; 
					efxLight = lighting ? g3dLight : null;
					efxTransparency = (cmd & ENV_ATTR_SEMI_TRANSPARENT) != 0;
					efxSphereTex = (cmd & ENV_ATTR_SPHERE_MAP) != 0 ? g3dSphereTex : null;
					efxToon = (cmd & ENV_ATTR_TOON_SHADING) != 0;
					
					break;
				}
				case COMMAND_CLIP:
					setClip(commandList[idx++], commandList[idx++], commandList[idx++], commandList[idx++]);
					break;
				case COMMAND_AMBIENT_LIGHT:
					g3dLight.setAmbientIntensity(commandList[idx++]);
					break;
				case COMMAND_DIRECTION_LIGHT:
					g3dLight.getParallelLightDirection().x = commandList[idx++];
					g3dLight.getParallelLightDirection().y = commandList[idx++];
					g3dLight.getParallelLightDirection().z = commandList[idx++];
					g3dLight.setParallelLightIntensity(commandList[idx++]);
					break;
				case COMMAND_THRESHOLD:
					efxToonThreshold = clamp(commandList[idx++], 0, 255);
					efxToonHigh = clamp(commandList[idx++], 0, 255);
					efxToonLow = clamp(commandList[idx++], 0, 255);
					break;
				case PRIMITVE_POINTS:
				case PRIMITVE_LINES:
				case PRIMITVE_TRIANGLES:
				case PRIMITVE_QUADS:
				case PRIMITVE_POINT_SPRITES: {
					int numPrims = (cmd >> 16) & 0xff;
					int numVtx = numPrims;
					
					if (cmdHigh == PRIMITVE_LINES) numVtx *= 2;
					else if (cmdHigh == PRIMITVE_TRIANGLES) numVtx *= 3;
					else if (cmdHigh == PRIMITVE_QUADS) numVtx *= 4;
					
					int vtxOffset = idx;
					int vtxSize = numVtx * 3;
					
					int normalOffset = vtxOffset + vtxSize;
					int normalSize = 0;
					int normalType = cmd & PDATA_NORMAL_MASK;
					if (normalType == PDATA_NORMAL_PER_FACE) normalSize = numPrims * 3;
					else if (normalType == PDATA_NORMAL_PER_VERTEX) normalSize = numVtx * 3;
					
					int texCoordOffset = normalOffset + normalSize;
					int texCoordSize = 0;
					
					if (cmdHigh != PRIMITVE_POINT_SPRITES) {
						if ((cmd & PDATA_TEXURE_COORD) != 0) texCoordSize = numVtx * 2;
					} else {
						int pdataSprParams = cmd & PDATA_SPRITE_PARAMS_MASK;
						
						if (pdataSprParams == PDATA_POINT_SPRITE_PARAMS_PER_CMD) texCoordSize = 8;
						else if (pdataSprParams != 0) texCoordSize = numPrims * 8;
					}
					
					int colorsOffset = texCoordOffset + texCoordSize;
					int colorsSize = 0;
					int colorsType = cmd & PDATA_COLOR_MASK;
					if (colorsType == PDATA_COLOR_PER_COMMAND) colorsSize = 1;
					else if (colorsType == PDATA_COLOR_PER_FACE) colorsSize = numPrims;
					
					idx = colorsOffset + colorsSize;
					
					switch (cmdHigh) {
						case PRIMITVE_POINTS: 
							submitPoints(trans, cmd, numPrims, commandList, vtxOffset, commandList, colorsOffset);
							break;
						case PRIMITVE_LINES: 
							submitLines(trans, cmd, numPrims, commandList, vtxOffset, commandList, colorsOffset);
							break;
						case PRIMITVE_TRIANGLES: 
							submitPrimitivePolygons(
									currentTex, trans,
									cmd, numPrims, false, 
									commandList, vtxOffset, 
									commandList, normalOffset,
									commandList, texCoordOffset, 
									commandList, colorsOffset
							);
							break;
						case PRIMITVE_QUADS: 
							submitPrimitivePolygons(
									currentTex, trans,
									cmd, numPrims, true, 
									commandList, vtxOffset, 
									commandList, normalOffset,
									commandList, texCoordOffset, 
									commandList, colorsOffset
							);
							break;
						case PRIMITVE_POINT_SPRITES:
							submitSprites(currentTex, trans, cmd, numPrims, commandList, vtxOffset, commandList, texCoordOffset);
							break;
					}
					
					break;
				}
				default:
					throw new IllegalArgumentException();
			}
		}
	}
	
    private static void requireRange(int[] data, int offset, int length) {
        if (length == 0) return;
        if (data == null) throw new NullPointerException();
        if (offset < 0 || length < 0 || offset > data.length - length)
            throw new IllegalArgumentException("Truncated primitive or command data");
    }

    private static int vertexCount(int cmd, int count) {
        int type = cmd >>> 24;
        return count * (type == 2 ? 2 : type == 3 ? 3 : type == 4 ? 4 : 1);
    }

    private static int normalWords(int cmd, int count) {
        int type = cmd & PDATA_NORMAL_MASK;
        return type == PDATA_NORMAL_PER_FACE ? count * 3 : type == PDATA_NORMAL_PER_VERTEX ? vertexCount(cmd, count) * 3 : 0;
    }

    private static int textureWords(int cmd, int count) {
        int type = cmd & PDATA_SPRITE_PARAMS_MASK;
        if ((cmd & 0xff000000) == PRIMITVE_POINT_SPRITES)
            return type == PDATA_POINT_SPRITE_PARAMS_PER_CMD ? 8 : type == 0 ? 0 : count * 8;
        return type == PDATA_TEXURE_COORD ? vertexCount(cmd, count) * 2 : 0;
    }

    private static int colorWords(int cmd, int count) {
        int type = cmd & PDATA_COLOR_MASK;
        return type == PDATA_COLOR_PER_COMMAND ? 1 : type == PDATA_COLOR_PER_FACE ? count : 0;
    }

    private static void validatePrimitive(int cmd, int count) {
        int type = cmd >>> 24;
        if (type < 1 || type > 5 || count < 1 || count > 255 ||
            (cmd & PDATA_NORMAL_MASK) == PDATA_NORMAL_INVALID ||
            (cmd & PDATA_COLOR_MASK) == PDATA_COLOR_INVALID ||
            (cmd & (0x00ffc08c & ~PATTR_LEGACY_IGNORED)) != 0)
            throw new IllegalArgumentException("Invalid primitive command");
        int tex = cmd & PDATA_SPRITE_PARAMS_MASK;
        if (type != 5 && tex != 0 && tex != PDATA_TEXURE_COORD)
            throw new IllegalArgumentException("Invalid texture coordinate mode");
    }

    private static void validatePerspective(int near, int far) {
        if (near < 1 || near >= 32767 || far < 2 || far >= 32768 || near >= far)
            throw new IllegalArgumentException("Invalid clipping planes");
    }

    // Preflight the complete stream before any flush or registration is observable.
    private void validateCommands(int[] data, Texture[] texs, Texture texture, FigureLayout layout) {
        requireRange(data, 0, 1);
        if (data[0] != COMMAND_LIST_VERSION_1_0) throw new IllegalArgumentException("Unsupported command list version");
        for (int i = 1; i < data.length;) {
            int cmd = data[i++], type = cmd & 0xff000000;
            int words = 0;
            switch (type) {
                case COMMAND_END: return;
                case COMMAND_NOP: words = cmd & 0xffffff; break;
                case COMMAND_FLUSH: case COMMAND_ATTRIBUTE: break;
                case COMMAND_CENTER: case COMMAND_PARALLEL_SCALE: words = 2; break;
                case COMMAND_CLIP: case COMMAND_DIRECTION_LIGHT: words = 4; break;
                case COMMAND_AMBIENT_LIGHT: words = 1; break;
                case COMMAND_THRESHOLD:
                    requireRange(data, i, 3);
                    for (int j = 0; j < 3; j++) if (data[i+j] < 0 || data[i+j] > 255)
                        throw new IllegalArgumentException("Invalid toon threshold");
                    words = 3; break;
                case COMMAND_TEXTURE_INDEX: {
                    int index = cmd & 0xffffff;
                    if (texs == null || index >= texs.length) throw new IllegalArgumentException("Invalid texture index");
                    texture = texs[index];
                    if (texture == null) throw new NullPointerException();
                    break;
                }
                case COMMAND_AFFINE_INDEX: layout._getAffineTrans(cmd & 0xffffff); break;
                case COMMAND_PARALLEL_SIZE:
                    requireRange(data, i, 2);
                    if (data[i] < 0 || data[i + 1] < 0) throw new IllegalArgumentException("Invalid parallel size");
                    words = 2; break;
                case COMMAND_PERSPECTIVE_FOV:
                    requireRange(data, i, 3);
                    validatePerspective(data[i], data[i + 1]);
                    if (data[i + 2] <= 0 || data[i + 2] >= 2048) throw new IllegalArgumentException("Invalid field of view");
                    words = 3; break;
                case COMMAND_PERSPECTIVE_WH:
                    requireRange(data, i, 4);
                    validatePerspective(data[i], data[i + 1]);
                    if (data[i + 2] < 0 || data[i + 3] < 0) throw new IllegalArgumentException("Invalid perspective size");
                    words = 4; break;
                default: {
                    int count = (cmd >>> 16) & 255;
                    validatePrimitive(cmd & 0xff00ffff, count);
                    words = vertexCount(cmd, count) * 3 + normalWords(cmd, count) + textureWords(cmd, count) + colorWords(cmd, count);
                    if (textureWords(cmd, count) != 0) {
                        if (texture == null) break;
                        if (!texture.isForModel) throw new IllegalArgumentException("Model texture required");
                        if (texture.bitmapData == null || texture.palette == null) throw new IllegalStateException("Disposed texture");
                    }
                    break;
                }
            }
            requireRange(data, i, words);
            i += words;
        }
    }

	private final int clamp(int v, int min, int max) {
		if (v <= min) return min;
		else if (v >= max) return max;
		else return v;
	}
	
	private final void submitPrimitivePolygons(
			Texture tex, AffineTrans trans,
			int command, int numPrims, boolean isQuad,
			int[] verts, int vtxOffset, int[] normals, int normOffset,
			int[] uvs, int uvOffset, int[] colors, int colOffset
	) {
		boolean hasUVs = (command & PDATA_TEXURE_COORD) != 0;
		int colorType = command & PDATA_COLOR_MASK;
		int normalType = command & PDATA_NORMAL_MASK;
		
		if (hasUVs) {
			if (tex == null) return;
			//Texture takes priority over color
			if (colorType != 0) colorType = 0;
		} else if (colorType == 0) return;
		
		if (colorType == PDATA_COLOR_INVALID) throw new IllegalArgumentException("Invalid pdata color type");
		if (normalType == PDATA_NORMAL_INVALID) throw new IllegalArgumentException("Invalid pdata normal type");
		
		boolean flatNormals = normalType == PDATA_NORMAL_PER_FACE;
		boolean lighting = normalType != 0 && efxLight != null && (command & PATTR_LIGHTING) != 0;
		boolean envMapping = lighting && efxSphereTex != null && (command & PATTR_SPHERE_MAP) != 0;
		int envmapTexId = 0;
		
		int material = Figure.MAT_DOUBLE_FACE;
		material |= (command & PATTR_COLORKEY) >> 4;
		if (efxTransparency) material |= ((command & PATTR_BLEND_MASK) >> 5) << 1;
		if (lighting) material |= Figure.MAT_LIGHTING;
		if (envMapping) {
			material |= Figure.MAT_SPECULAR;
			bindTexture(efxSphereTex);
			envmapTexId = bindTexture(efxSphereTex);
		}
		if (flatNormals) material |= Figure.MAT_FLAT_NORMAL;
		
		int polyStride = calcPolygonStride(hasUVs, flatNormals, lighting, envMapping);
		reservePrimBuffers((isQuad ? 2 : 1) * numPrims, polyStride);
		
		int vtxPerPrim = isQuad ? 4 : 3;
		preallocVtxBuffers(numPrims * vtxPerPrim);
		processVertices(null, verts, vtxOffset, 0, numPrims * vtxPerPrim, trans);
		
		if (lighting) {
			int numNormals = numPrims;
			if (normalType == PDATA_NORMAL_PER_VERTEX) numNormals *= vtxPerPrim;
			preallocLightBuffers(numNormals, envMapping);
			processLighting(null, normals, normOffset, 0, numNormals, trans, envMapping);
		}
		
		int texCol = 0;
		if (hasUVs) {
			bindTexture(tex);
			texCol = bindTexture(tex);
		} else if (colorType == PDATA_COLOR_PER_COMMAND) {
			 texCol = 0xff000000 | colors[colOffset];
		}
		
		int[] projVtx = this.projVtx;
		
		if(!isQuad) {
			for (int i = 0; i < numPrims; i++) {
				if (colorType == PDATA_COLOR_PER_FACE) {
					texCol = 0xff000000 | colors[colOffset + i];
				}
				
				if (startTriangle(material, texCol, envmapTexId, i * 3, i * 3 + 1, i * 3 + 2, i, true)) {
					if (hasUVs) {
						int offset = uvOffset + i * 6;
						
						setTriangleUVs(
								uvs[offset] << 8, uvs[offset + 1] << 8,
								uvs[offset + 2] << 8, uvs[offset + 3] << 8,
								uvs[offset + 4] << 8, uvs[offset + 5] << 8
						);
					}
					
					int sortZ = averageDepth(i * 3, 3);
				
					endTriangle(sortZ);
				}
			}
		} else {
			for (int i = 0; i < numPrims; i++) {
				if (colorType == PDATA_COLOR_PER_FACE) {
					texCol = 0xff000000 | colors[colOffset + i];
				}
				
				int sortZ = Integer.MIN_VALUE;
				
				//abd
				if (startTriangle(material, texCol, envmapTexId, i * 4, i * 4 + 1, i * 4 + 3, i, true)) {
					if (hasUVs) {
						int offset = uvOffset + i * 8;
						
						setTriangleUVs(
								uvs[offset] << 8, uvs[offset + 1] << 8,
								uvs[offset + 2] << 8, uvs[offset + 3] << 8,
								uvs[offset + 6] << 8, uvs[offset + 7] << 8
						);
					}
				
					sortZ = averageDepth(i * 4, 4);
					
					endTriangle(sortZ);
				}
				
				//bcd
				if (startTriangle(material, texCol, envmapTexId, i * 4 + 1, i * 4 + 2, i * 4 + 3, i, true)) {
					if (hasUVs) {
						int offset = uvOffset + i * 8;
						
						setTriangleUVs(
								uvs[offset + 2] << 8, uvs[offset + 3] << 8,
								uvs[offset + 4] << 8, uvs[offset + 5] << 8,
								uvs[offset + 6] << 8, uvs[offset + 7] << 8
						);
					}
				
					if (sortZ == Integer.MIN_VALUE) {
						sortZ = averageDepth(i * 4, 4);
					}
					
					endTriangle(sortZ);
				}
			}
		}
		
		flushPrimBufferReserved();
	}
	
	private final void submitPoints(
			AffineTrans trans, int command, int numPrims, 
			int[] verts, int vtxOffset, int[] colors, int colOffset
	) {
		int colorType = command & PDATA_COLOR_MASK;
		if (colorType == PDATA_COLOR_INVALID) throw new IllegalArgumentException("Invalid pdata color type");
		else if(colorType == 0) return;

		reservePrimBuffers(numPrims, 4);

		int primDataUsed = this.primDataUsed;
		int[] primData = this.primData;
		
		int sortPrimCount = this.sortPrimCount;
		int[] sortPrimIdx = this.sortPrimIdx;
		
		int scaleX = projScaleX, scaleY = projScaleY;
		int centerX = drawCenterX, centerY = drawCenterY;
		int fbWidth = this.fbWidth, fbHeight = this.fbHeight;
		
		int projectionMode = this.projectionMode;
		int perspectiveNear = this.projNear;
		int perspectiveFar = this.projFar;
		
		int m00, m01, m02, m03;
		int m10, m11, m12, m13;
		int m20, m21, m22, m23;
		
		if (trans != null) {
			m00 = trans.m00; m01 = trans.m01; m02 = trans.m02; m03 = trans.m03;
			m10 = trans.m10; m11 = trans.m11; m12 = trans.m12; m13 = trans.m13;
			m20 = trans.m20; m21 = trans.m21; m22 = trans.m22; m23 = trans.m23;
		} else {
			m00 = m01 = m02 = m03 = 0;
			m10 = m11 = m12 = m13 = 0;
			m20 = m21 = m22 = m23 = 0;
		}
		
		int primHeader = PRIM_TYPE_POINT;
		if (efxTransparency) primHeader |= command & PATTR_BLEND_MASK;
		
		int color = 0;
		if (colorType == PDATA_COLOR_PER_COMMAND) {
			 color = colors[colOffset];
		}
		
		for (int i = 0; i < numPrims; i++) {
			int vx = verts[vtxOffset + i * 3	];
			int vy = verts[vtxOffset + i * 3 + 1];
			int vz = verts[vtxOffset + i * 3 + 2];
			
			int tx, ty, tz;
			if (trans != null) {
				tx = transformCoordinate(vx, vy, vz, m00, m01, m02, m03);
				ty = transformCoordinate(vx, vy, vz, m10, m11, m12, m13);
				tz = transformCoordinate(vx, vy, vz, m20, m21, m22, m23);
			} else {
				tx = vx; ty = vy; tz = vz;
			}
			
			if (projectionMode == PROJ_PERSPECTIVE) {
				if (tz < perspectiveNear) continue;
				else if (tz > perspectiveFar) continue;
				
				tx = projectCoordinate(tx, scaleX, tz, centerX);
				ty = projectCoordinate(ty, scaleY, tz, centerY);
			} else {
				tx = projectCoordinate(tx, scaleX, 0, centerX);
				ty = projectCoordinate(ty, scaleY, 0, centerY);
			}
			
			if (tx < 0 || tx >= fbWidth) continue;
			if (ty < 0 || ty >= fbHeight) continue;
			
			if (colorType == PDATA_COLOR_PER_FACE) {
				color = colors[colOffset + i];
			}

			primitiveClip[primDataUsed] = activeClip;
            sortPrimIdx[sortPrimCount++] = primDataUsed;
			
			primData[primDataUsed++] = tz;
			primData[primDataUsed++] = (color << 8) | primHeader;
			primData[primDataUsed++] = tx;
			primData[primDataUsed++] = ty;
		}
		
		this.sortPrimCount = sortPrimCount;
		this.primDataUsed = primDataUsed;
		
		flushPrimBufferReserved();
	}
	
	private final void submitLines(
			AffineTrans trans, int command, int numPrims, 
			int[] verts, int vtxOffset, int[] colors, int colOffset
	) {
		int colorType = command & PDATA_COLOR_MASK;
		if (colorType == PDATA_COLOR_INVALID) throw new IllegalArgumentException("Invalid pdata color type");
		else if(colorType == 0) return;

		reservePrimBuffers(numPrims, 6);

		int primDataUsed = this.primDataUsed;
		int[] primData = this.primData;
		
		int sortPrimCount = this.sortPrimCount;
		int[] sortPrimIdx = this.sortPrimIdx;
		
		int scaleX = projScaleX, scaleY = projScaleY;
		int centerX = drawCenterX, centerY = drawCenterY;
		int fbWidth = this.fbWidth, fbHeight = this.fbHeight;
		
		int projectionMode = this.projectionMode;
		int perspectiveNear = this.projNear;
		int perspectiveFar = this.projFar;
		
		int m00, m01, m02, m03;
		int m10, m11, m12, m13;
		int m20, m21, m22, m23;
		
		if (trans != null) {
			m00 = trans.m00; m01 = trans.m01; m02 = trans.m02; m03 = trans.m03;
			m10 = trans.m10; m11 = trans.m11; m12 = trans.m12; m13 = trans.m13;
			m20 = trans.m20; m21 = trans.m21; m22 = trans.m22; m23 = trans.m23;
		} else {
			m00 = m01 = m02 = m03 = 0;
			m10 = m11 = m12 = m13 = 0;
			m20 = m21 = m22 = m23 = 0;
		}
		
		int primHeader = PRIM_TYPE_LINE;
		if (efxTransparency) primHeader |= command & PATTR_BLEND_MASK;
		
		int color = 0;
		if (colorType == PDATA_COLOR_PER_COMMAND) {
			 color = colors[colOffset];
		}
		
		for (int i = 0; i < numPrims; i++) {
			int vx1 = verts[vtxOffset + i * 6	];
			int vy1 = verts[vtxOffset + i * 6 + 1];
			int vz1 = verts[vtxOffset + i * 6 + 2];
			int vx2 = verts[vtxOffset + i * 6 + 3];
			int vy2 = verts[vtxOffset + i * 6 + 4];
			int vz2 = verts[vtxOffset + i * 6 + 5];
			
			int x1, y1, z1, x2, y2, z2;
			if (trans != null) {
				x1 = transformCoordinate(vx1, vy1, vz1, m00, m01, m02, m03);
				y1 = transformCoordinate(vx1, vy1, vz1, m10, m11, m12, m13);
				z1 = transformCoordinate(vx1, vy1, vz1, m20, m21, m22, m23);
				
				x2 = transformCoordinate(vx2, vy2, vz2, m00, m01, m02, m03);
				y2 = transformCoordinate(vx2, vy2, vz2, m10, m11, m12, m13);
				z2 = transformCoordinate(vx2, vy2, vz2, m20, m21, m22, m23);
			} else {
				x1 = vx1; y1 = vy1; z1 = vz1;
				x2 = vx2; y2 = vy2; z2 = vz2;
			}
			
			if (projectionMode == PROJ_PERSPECTIVE) {
				if (z1 < perspectiveNear && z2 < perspectiveNear) continue;
				if (z1 > perspectiveFar && z2 > perspectiveFar) continue;
				
				if (z1 < perspectiveNear) {
					int tmpZ = (int)(((long)perspectiveNear - z1) * 4096 / ((long)z2 - z1));
					x1 = (int)((((long)x2 - x1) * tmpZ >> 12) + x1);
					y1 = (int)((((long)y2 - y1) * tmpZ >> 12) + y1);
					z1 = perspectiveNear;
				} else if (z2 < perspectiveNear) {
					int tmpZ = (int)(((long)perspectiveNear - z1) * 4096 / ((long)z2 - z1));
					x2 = (int)((((long)x2 - x1) * tmpZ >> 12) + x1);
					y2 = (int)((((long)y2 - y1) * tmpZ >> 12) + y1);
					z2 = perspectiveNear;
				}
				
				if (z1 > perspectiveFar) {
					int tmpZ = (int)(((long)perspectiveFar - z1) * 4096 / ((long)z2 - z1));
					x1 = (int)((((long)x2 - x1) * tmpZ >> 12) + x1);
					y1 = (int)((((long)y2 - y1) * tmpZ >> 12) + y1);
					z1 = perspectiveFar;
				} else if (z2 > perspectiveFar) {
					int tmpZ = (int)(((long)perspectiveFar - z1) * 4096 / ((long)z2 - z1));
					x2 = (int)((((long)x2 - x1) * tmpZ >> 12) + x1);
					y2 = (int)((((long)y2 - y1) * tmpZ >> 12) + y1);
					z2 = perspectiveFar;
				}
				
				x1 = projectCoordinate(x1, scaleX, z1, centerX);
				x2 = projectCoordinate(x2, scaleX, z2, centerX);
				y1 = projectCoordinate(y1, scaleY, z1, centerY);
				y2 = projectCoordinate(y2, scaleY, z2, centerY);
			} else {
				x1 = projectCoordinate(x1, scaleX, 0, centerX);
				x2 = projectCoordinate(x2, scaleX, 0, centerX);
				y1 = projectCoordinate(y1, scaleY, 0, centerY);
				y2 = projectCoordinate(y2, scaleY, 0, centerY);
			}
			
			if (x1 < 0 && x2 < 0) continue;
			if (x1 >= fbWidth && x2 >= fbWidth) continue;
			if (y1 < 0 && y2 < 0) continue;
			if (y1 >= fbHeight && y2 >= fbHeight) continue;
			
			if (colorType == PDATA_COLOR_PER_FACE) {
				color = colors[colOffset + i];
			}

			primitiveClip[primDataUsed] = activeClip;
            sortPrimIdx[sortPrimCount++] = primDataUsed;
			
			primData[primDataUsed++] = (int)(((long)z1 + z2) / 2);
			primData[primDataUsed++] = (color << 8) | primHeader;
			primData[primDataUsed++] = x1;
			primData[primDataUsed++] = y1;
			primData[primDataUsed++] = x2;
			primData[primDataUsed++] = y2;
		}
		
		this.sortPrimCount = sortPrimCount;
		this.primDataUsed = primDataUsed;
		
		flushPrimBufferReserved();
	}

	private final void submitSprites(
			Texture tex, AffineTrans trans,
			int command, int numPrims,
			int[] verts, int vtxOffset, int[] sprParams, int paramOffset
	) {
		if (tex == null) return;

		int mode = command & PDATA_SPRITE_PARAMS_MASK;
		if (mode == 0) return;
		
		bindTexture(tex);

		reservePrimBuffers(numPrims, 18);

		int primDataUsed = this.primDataUsed;
		int[] primData = this.primData;
		
		int sortPrimCount = this.sortPrimCount;
		int[] sortPrimIdx = this.sortPrimIdx;

		int scaleX = projScaleX, scaleY = projScaleY;
		int centerX = drawCenterX, centerY = drawCenterY;
		int fbWidth = this.fbWidth, fbHeight = this.fbHeight;
		int pxHScale = 0;
		
		int projectionMode = this.projectionMode;
		int perspectiveNear = this.projNear;
		int perspectiveFar = this.projFar;
		
		int m00, m01, m02, m03;
		int m10, m11, m12, m13;
		int m20, m21, m22, m23;
		
		if (trans != null) {
			m00 = trans.m00; m01 = trans.m01; m02 = trans.m02; m03 = trans.m03;
			m10 = trans.m10; m11 = trans.m11; m12 = trans.m12; m13 = trans.m13;
			m20 = trans.m20; m21 = trans.m21; m22 = trans.m22; m23 = trans.m23;
		} else {
			m00 = m01 = m02 = m03 = 0;
			m10 = m11 = m12 = m13 = 0;
			m20 = m21 = m22 = m23 = 0;
		}

		int sprW = 0, sprH = 0, sprAngle = 0;
		int sprTexX1 = 0, sprTexY1 = 0, sprTexX2 = 0, sprTexY2 = 0;
		int sprFlags = 0;
		
		if (mode == PDATA_POINT_SPRITE_PARAMS_PER_CMD) {
			sprW	 = sprParams[paramOffset	];
			sprH	 = sprParams[paramOffset + 1];
			sprAngle = sprParams[paramOffset + 2];
			sprTexX1 = sprParams[paramOffset + 3];
			sprTexY1 = sprParams[paramOffset + 4];
			sprTexX2 = sprParams[paramOffset + 5];
			sprTexY2 = sprParams[paramOffset + 6];
			sprFlags = sprParams[paramOffset + 7];
		}
		
		int primHeader = (command & PATTR_COLORKEY) | PRIM_TYPE_SPRITE;
		if (efxTransparency) primHeader |= command & PATTR_BLEND_MASK;
		primHeader |= bindTexture(tex) << 8;

		for (int i = 0; i < numPrims; i++) {
			int vx = verts[vtxOffset + i * 3	];
			int vy = verts[vtxOffset + i * 3 + 1];
			int vz = verts[vtxOffset + i * 3 + 2];

			int tx, ty, tz;
			if (trans != null) {
				tx = transformCoordinate(vx, vy, vz, m00, m01, m02, m03);
				ty = transformCoordinate(vx, vy, vz, m10, m11, m12, m13);
				tz = transformCoordinate(vx, vy, vz, m20, m21, m22, m23);
			} else {
				tx = vx;
				ty = vy;
				tz = vz;
			}

			if (mode != PDATA_POINT_SPRITE_PARAMS_PER_CMD) {
				int offset = paramOffset + i * 8;
				
				sprW	 = sprParams[offset	   ];
				sprH	 = sprParams[offset + 1];
				sprAngle = sprParams[offset + 2];
				sprTexX1 = sprParams[offset + 3];
				sprTexY1 = sprParams[offset + 4];
				sprTexX2 = sprParams[offset + 5];
				sprTexY2 = sprParams[offset + 6];
				sprFlags = sprParams[offset + 7];
			}

			if (sprTexX2 <= sprTexX1 || sprTexY2 <= sprTexY1) continue;

			boolean sizeInPixels = (sprFlags & POINT_SPRITE_PIXEL_SIZE) != 0;
			boolean noPerspective = (sprFlags & POINT_SPRITE_NO_PERS) != 0;

			long screenX, screenY;
			long projWX, projWY, projHX, projHY;

			if (projectionMode == PROJ_PERSPECTIVE) {
				if (tz < perspectiveNear) continue;
				if (tz > perspectiveFar) continue;

				screenX = projectCoordinateLong(tx, scaleX, tz, centerX);
				screenY = projectCoordinateLong(ty, scaleY, tz, centerY);

				if (sizeInPixels) {
					if (noPerspective) {
						projWX = sprW;
						projWY = sprW >> pxHScale;
						projHX = sprH;
						projHY = sprH >> pxHScale;
					} else {
						projWX = projectCoordinateLong(sprW, perspectiveNear, tz, 0);
						projWY = projectCoordinateLong(sprW, perspectiveNear, tz, 0);
						projHX = projectCoordinateLong(sprH, perspectiveNear, tz, 0);
						projHY = projectCoordinateLong(sprH, perspectiveNear, tz, 0);
					}
				} else {
					if (noPerspective) {
						projWX = projectCoordinateLong(sprW, scaleX, perspectiveNear, 0);
						projWY = projectCoordinateLong(sprW, scaleY, perspectiveNear, 0);
						projHX = projectCoordinateLong(sprH, scaleX, perspectiveNear, 0);
						projHY = projectCoordinateLong(sprH, scaleY, perspectiveNear, 0);
					} else {
						projWX = projectCoordinateLong(sprW, scaleX, tz, 0);
						projWY = projectCoordinateLong(sprW, scaleY, tz, 0);
						projHX = projectCoordinateLong(sprH, scaleX, tz, 0);
						projHY = projectCoordinateLong(sprH, scaleY, tz, 0);
					}
				}
			} else {
				screenX = projectCoordinateLong(tx, scaleX, 0, centerX);
				screenY = projectCoordinateLong(ty, scaleY, 0, centerY);

				if (sizeInPixels) {
					projWX = sprW;
					projWY = sprW >> pxHScale;
					projHX = sprH;
					projHY = sprH >> pxHScale;
				} else {
					projWX = projectCoordinateLong(sprW, scaleX, 0, 0);
					projWY = projectCoordinateLong(sprW, scaleY, 0, 0);
					projHX = projectCoordinateLong(sprH, scaleX, 0, 0);
					projHY = projectCoordinateLong(sprH, scaleY, 0, 0);
				}
			}
			
			if (projWX <= 0 || projHX <= 0 || projWY <= 0 || projHY <= 0) continue;

			//Calculate screen bounds (centered on screenX, screenY)
			//sqrt(2) * 4096 ~= 5793
            double radiusX=projWX/2.0+projHX/2.0+1, radiusY=projWY/2.0+projHY/2.0+1;
            if (screenX+radiusX<0 || screenX-radiusX>=fbWidth || screenY+radiusY<0 || screenY-radiusY>=fbHeight) continue;

			primitiveClip[primDataUsed] = activeClip;
            sortPrimIdx[sortPrimCount++] = primDataUsed;
			
			primData[primDataUsed++] = tz;
			primData[primDataUsed++] = (sprAngle << 20) | primHeader;
            putCoordinate(primData,primDataUsed,screenX); primDataUsed+=2;
            putCoordinate(primData,primDataUsed,screenY); primDataUsed+=2;
            putCoordinate(primData,primDataUsed,projWX); primDataUsed+=2;
            putCoordinate(primData,primDataUsed,projHX); primDataUsed+=2;
            putCoordinate(primData,primDataUsed,projWY); primDataUsed+=2;
            putCoordinate(primData,primDataUsed,projHY); primDataUsed+=2;
			primData[primDataUsed++] = sprTexX1;
            primData[primDataUsed++] = sprTexY1;
            primData[primDataUsed++] = sprTexX2;
            primData[primDataUsed++] = sprTexY2;
		}
		
		this.sortPrimCount = sortPrimCount;
		this.primDataUsed = primDataUsed;
		
		flushPrimBufferReserved();
	}
	
	private final void processVertices(
			short[] vtxShort, int[] vtxInt, 
			int readIdx, int writeIdx, int vtxCount, 
			AffineTrans trans
	) {
		int[] tranVtx = this.tranVtx;
		int[] projVtx = this.projVtx;
		
		int scaleX = projScaleX, scaleY = projScaleY;
		int centerX = drawCenterX, centerY = drawCenterY;
		int projectionMode = this.projectionMode;
		int perspectiveNear = this.projNear;
		
		int m00, m01, m02, m03;
		int m10, m11, m12, m13;
		int m20, m21, m22, m23;
		
		if (trans != null) {
			m00 = trans.m00; m01 = trans.m01; m02 = trans.m02; m03 = trans.m03;
			m10 = trans.m10; m11 = trans.m11; m12 = trans.m12; m13 = trans.m13;
			m20 = trans.m20; m21 = trans.m21; m22 = trans.m22; m23 = trans.m23;
		} else {
			m00 = m01 = m02 = m03 = 0;
			m10 = m11 = m12 = m13 = 0;
			m20 = m21 = m22 = m23 = 0;
		}
		
		int writeIdx3 = writeIdx * 3, writeIdx2 = writeIdx * 2;
		
		for (int i = 0; i < vtxCount; i++, readIdx += 3, writeIdx3 += 3, writeIdx2 += 2) {
			int vx, vy, vz;
			
			if (vtxShort != null) {
				vx = vtxShort[readIdx    ];
				vy = vtxShort[readIdx + 1];
				vz = vtxShort[readIdx + 2];
			} else {
				vx = vtxInt[readIdx    ];
				vy = vtxInt[readIdx + 1];
				vz = vtxInt[readIdx + 2];
			}

			int tx, ty, tz;
			if (trans != null) {
				tx = transformCoordinate(vx, vy, vz, m00, m01, m02, m03);
				ty = transformCoordinate(vx, vy, vz, m10, m11, m12, m13);
				tz = transformCoordinate(vx, vy, vz, m20, m21, m22, m23);
			} else {
				tx = vx; ty = vy; tz = vz;
			}

			tranVtx[writeIdx2	 ] = tx;
			tranVtx[writeIdx2 + 1] = ty;

            int divisor = projectionMode == PROJ_PERSPECTIVE ? Math.max(tz, perspectiveNear) : 0;
            long sx = projectCoordinateLong(tx, scaleX, divisor, centerX);
            long sy = projectCoordinateLong(ty, scaleY, divisor, centerY);
            projectedXY[writeIdx2] = sx; projectedXY[writeIdx2 + 1] = sy;
            projVtx[writeIdx3] = clampLong(sx, Integer.MIN_VALUE, Integer.MAX_VALUE);
            projVtx[writeIdx3 + 1] = clampLong(sy, Integer.MIN_VALUE, Integer.MAX_VALUE);
			projVtx[writeIdx3 + 2] = tz;
		}
	}
	
	private final void processLighting(
			short[] normShort, int[] normInt, 
			int readPos, int writeIdx, int vtxCount, 
			AffineTrans trans, boolean useEnvMap
	) {
		short[] lightVtx = this.lightVtx;
		short[] envUVs = this.envUVs;

		int dirIntensity = efxLight.getParallelLightIntensity();
		int ambIntensity = efxLight.getAmbientIntensity();
		
		Vector3D lightDir = efxLight.getParallelLightDirection();
		Vector3D tmpVec = this.tmpVec;
		
		tmpVec.set(lightDir);
		if (trans != null) {
            int vx = tmpVec.x, vy = tmpVec.y, vz = tmpVec.z;
            tmpVec.set((int)(((long)trans.m00*vx+(long)trans.m10*vy+(long)trans.m20*vz+2048)>>12),
                       (int)(((long)trans.m01*vx+(long)trans.m11*vy+(long)trans.m21*vz+2048)>>12),
                       (int)(((long)trans.m02*vx+(long)trans.m12*vy+(long)trans.m22*vz+2048)>>12));
        }
		tmpVec.unit();

		int lx = -tmpVec.x, ly = -tmpVec.y, lz = -tmpVec.z;
		//Disable negative clamping when toon shading is enabled to improve toon shading quality
		int minLight = efxToon ? Integer.MIN_VALUE : 0;

		int envUx = 0, envUy = 0, envUz = 0;
		int envVx = 0, envVy = 0, envVz = 0;

		if(useEnvMap) {
			if (trans != null) {
				tmpVec.set(trans.m00, trans.m01, trans.m02);
				tmpVec.unit();
			} else {
				tmpVec.set(4096, 0, 0);
			}
			
			envUx = tmpVec.x;
			envUy = tmpVec.y;
			envUz = tmpVec.z;

			if (trans != null) {
				tmpVec.set(trans.m10, trans.m11, trans.m12);
				tmpVec.unit();
			} else {
				tmpVec.set(0, 4096, 0);
			}
			
			envVx = tmpVec.x;
			envVy = tmpVec.y;
			envVz = tmpVec.z;
		}

		for(int i = 0; i < vtxCount; i++, readPos += 3, writeIdx++) {
			int nx, ny, nz;
			
			if (normShort != null) {
				nx = normShort[readPos];
				ny = normShort[readPos + 1];
				nz = normShort[readPos + 2];
			} else {
				nx = normInt[readPos];
				ny = normInt[readPos + 1];
				nz = normInt[readPos + 2];
			}

			int intensity = ambIntensity;

			long dot = (long)nx * lx + (long)ny * ly + (long)nz * lz;
			if(dot > minLight) intensity += (int)(((dot >> 12) * dirIntensity) >> 12);

			if(intensity > 4095) intensity = 4095;
            if (intensity < (efxToon ? -32768 : 0)) intensity = efxToon ? -32768 : 0;
			lightVtx[writeIdx] = (short) intensity;

			if(useEnvMap) {
				int dotU = clampLong(((long)nx * envUx + (long)ny * envUy + (long)nz * envUz) >> 12, -4096, 4096);

				dotU = (dotU + 4096) >> 1;
				if(dotU > 4095) dotU = 4095;
				else if(dotU < 0) dotU = 0;
				
				// Map to the 64-pixel sphere image, clamped below its right edge.
                dotU >>= 2;
				envUVs[writeIdx * 2] = (short) dotU;

				int dotV = clampLong(((long)nx * envVx + (long)ny * envVy + (long)nz * envVz) >> 12, -4096, 4096);

				dotV = (dotV + 4096) >> 1;
				if(dotV > 4095) dotV = 4095;
				else if(dotV < 0) dotV = 0;
				
				dotV >>= 2;
				envUVs[writeIdx * 2 + 1] = (short) dotV;
			}
		}
	}
	
	private final int calcPolygonStride(boolean texturing, boolean flatNormals, boolean lighting, boolean envMapping) {
		return 
				1 + //Z sort
				1 + //Textures + material flags
				12 + //Three full 64-bit projected xy pairs
				(lighting ? 1 : 0) +
				(envMapping ? (flatNormals ? 1 : 2) : 0) +
				(texturing ? 3: 1)//Uvs or color;
				;
	}
	
	//Todo simplify this somehow...
	private final boolean startTriangle(
			int mat, int texCol, int envMapTexId,
			int v0, int v1, int v2, 
			int normalId, boolean clipping
	) {
		int[] projVtx = this.projVtx;
		
		int v03 = v0 * 3, v13 = v1 * 3, v23 = v2 * 3;
		
        long x0 = projectedXY[v0 * 2], y0 = projectedXY[v0 * 2 + 1];
        long x1 = projectedXY[v1 * 2], y1 = projectedXY[v1 * 2 + 1];
        long x2 = projectedXY[v2 * 2], y2 = projectedXY[v2 * 2 + 1];

		int clippingStages = 0;
		
		if (clipping) {

			if (projectionMode == PROJ_PERSPECTIVE) {
				int z0 = projVtx[v03 + 2];
				int z1 = projVtx[v13 + 2];
				int z2 = projVtx[v23 + 2];
			
				int perspectiveNear = this.projNear;
				boolean nearCull = z0 < perspectiveNear && z1 < perspectiveNear && z2 < perspectiveNear;
				if (nearCull) return false;
				
				int perspectiveFar = this.projFar;
				boolean farCull = z0 > perspectiveFar && z1 > perspectiveFar && z2 > perspectiveFar;
				if (farCull) return false;
				
				nearCull = z0 < perspectiveNear || z1 < perspectiveNear || z2 < perspectiveNear;
				if (nearCull) clippingStages |= NEAR_CLIP;
				
				farCull = z0 > perspectiveFar || z1 > perspectiveFar || z2 > perspectiveFar;
				if (farCull) clippingStages |= FAR_CLIP;
			}
		}
        // A vertex behind the near plane was provisionally projected at near.
        // Its provisional screen bounds do not bound the clipped polygon.
        if ((clippingStages & NEAR_CLIP) == 0) {
            if (x0<0 && x1<0 && x2<0) return false;
            if (y0<0 && y1<0 && y2<0) return false;
            if (x0>=fbWidth && x1>=fbWidth && x2>=fbWidth) return false;
            if (y0>=fbHeight && y1>=fbHeight && y2>=fbHeight) return false;
        }

		if ((clippingStages & NEAR_CLIP) == 0 && (mat & Figure.MAT_DOUBLE_FACE) == 0) {
			double cross = ((double)x1 - x0) * ((double)y2 - y1) - ((double)y1 - y0) * ((double)x2 - x1);
			if (cross <= 0) return false;
		}
		
		boolean lighting = (mat & Figure.MAT_LIGHTING) != 0;
		boolean flat = (mat & Figure.MAT_FLAT_NORMAL) != 0;
		boolean toon = lighting && efxToon;
		
		boolean toonLow = false, toonHigh = true;
		if (toon) {
			if (flat) {
				toonLow = (lightVtx[normalId] >> 4) <= efxToonThreshold;
			} else {
				int threshold = efxToonThreshold;
				short[] lightVtx = this.lightVtx;
				int sa = lightVtx[v0] >> 4;
				int sb = lightVtx[v1] >> 4;
				int sc = lightVtx[v2] >> 4;
				
				toonLow = (sa <= threshold) && (sb <= threshold) && (sc <= threshold);
				toonHigh = (sa >= threshold) && (sb >= threshold) && (sc >= threshold);
			}
		}
			
		if (clipping) {
			if (!toonHigh && !toonLow) clippingStages |= TOON_SPLIT;
			
			clippingStages &= allowedClippingStages;
			
			if (clippingStages != 0) {
				this.activeClippingStages = clippingStages;
				startTriangleClip(
						mat, texCol, envMapTexId, 
						v0, v1, v2, normalId
				);
				
				return true;
			}
		}
		
		boolean hasTex = texCol >= 0;
		boolean envMapping = (mat & Figure.MAT_SPECULAR) != 0;
		
        // Clipping can expand a preceding triangle and consume a batch reservation.
        // Every emitted record checks its actual complete payload capacity.
        reservePrimBuffers(1, calcPolygonStride(hasTex, flat, lighting, envMapping));
		int primDataUsed = this.primDataUsed;
		int[] primData = this.primData;
		
		primitiveClip[primDataUsed] = activeClip;
            sortPrimIdx[sortPrimCount++] = primDataUsed;
		
		primDataUsed++;
		
		mat &= 0xE7; //Keep only necessary masks
		mat |= PRIM_TYPE_POLY_FLAG;
		if (toon) mat |= PRIM_MAT_TOON;
		
		if (hasTex) {
			primData[primDataUsed] = (envMapTexId << 20) | (texCol << 8) | mat;
		} else {
			primData[primDataUsed] = (envMapTexId << 20) | (4095 << 8) | mat;
		}
		
        putCoordinate(primData, primDataUsed + 1, x0); putCoordinate(primData, primDataUsed + 3, y0);
        putCoordinate(primData, primDataUsed + 5, x1); putCoordinate(primData, primDataUsed + 7, y1);
        putCoordinate(primData, primDataUsed + 9, x2); putCoordinate(primData, primDataUsed + 11, y2);
        primDataUsed += 13;

		if (!hasTex) primData[primDataUsed++] = texCol;
		
		if (lighting) {
			int lightData;
			
			if (!toon) {
				if (flat) {
					lightData = lightVtx[normalId] >> 2;
				} else {
					short[] lightVtx = this.lightVtx;
					lightData = lightVtx[v0] >> 2;
					lightData |= (lightVtx[v1] >> 2) << 10;
					lightData |= (lightVtx[v2] >> 2) << 20;
				}
			} else {
				lightData = (toonLow ? efxToonLow : efxToonHigh) << 2;
			}
			
			primData[primDataUsed++] = lightData;
			
			if (envMapping) {
				short[] envUVs = this.envUVs;
				if (flat) {
					int envData = envUVs[normalId * 2] << 20;
					envData |= envUVs[normalId * 2 + 1] << 10;
					primData[primDataUsed++] = envData;
				} else {
					v0 <<= 1;
					v1 <<= 1;
					v2 <<= 1;
					
					int envData = envUVs[v0] << 20;
					envData |= envUVs[v0 + 1] << 10;
					envData |= envUVs[v1];
					primData[primDataUsed] = envData;
					
					envData = envUVs[v1 + 1] << 20;
					envData |= envUVs[v2] << 10;
					envData |= envUVs[v2 + 1];
					primData[primDataUsed + 1] = envData;
					primDataUsed += 2;
				}
			}
		}
		
		this.primDataUsed = primDataUsed;
		return true;
	}
	
	private final void setTriangleUVs(int au, int av, int bu, int bv, int cu, int cv) {
		if (activeClippingStages == 0) {
			int[] primData = this.primData;
			int primDataUsed = this.primDataUsed;
			
			primData[primDataUsed    ] = (au << 16) | av;
			primData[primDataUsed + 1] = (bu << 16) | bv;
			primData[primDataUsed + 2] = (cu << 16) | cv;
			
			this.primDataUsed += 3;
		} else {
			setTriangleUVsClip(au, av, bu, bv, cu, cv);
		}
	}
	
	private final void endTriangle(int sortZ) {
		if (activeClippingStages == 0) {
			primData[sortPrimIdx[sortPrimCount - 1]] = sortZ;
			return;
		}
		
		endTriangleClip(sortZ);
	}

	private final void startTriangleClip(
			int mat, int texCol, int envMapTexId,
			int v0, int v1, int v2, int normalId
	) {
		this.clipPolyMat = mat;
		this.clipPolyTexCol = texCol;
		this.clipPolyEnvmapId = envMapTexId;
		this.clipPolyNormalId = normalId;

		clipPolyHasUVs = texCol >= 0;
		clipPolyFlatNorm = (mat & Figure.MAT_FLAT_NORMAL) != 0;
		clipPolyHasLight = !clipPolyFlatNorm && ((mat & Figure.MAT_LIGHTING) != 0);
		clipPolyEnvmap = clipPolyHasLight && ((mat & Figure.MAT_SPECULAR) != 0);

		int attsCount = 3;
		if(clipPolyHasUVs) attsCount += 2;
		if(clipPolyHasLight) attsCount++;
		if(clipPolyEnvmap) attsCount += 2;
		this.clipAttsCount = attsCount;

		int[] input = clipBuffers[0];

		int[] tranVtx2 = tranVtx;
		int[] projVtx2 = projVtx;
		input[0] = tranVtx2[v0 * 2];
		input[1] = tranVtx2[v0 * 2 + 1];
		input[2] = projVtx2[v0 * 3 + 2];

		input[attsCount] = tranVtx2[v1 * 2];
		input[attsCount + 1] = tranVtx2[v1 * 2 + 1];
		input[attsCount + 2] = projVtx2[v1 * 3 + 2];

		input[attsCount * 2] = tranVtx2[v2 * 2];
		input[attsCount * 2 + 1] = tranVtx2[v2 * 2 + 1];
		input[attsCount * 2 + 2] = projVtx2[v2 * 3 + 2];

		int offset = 3;

		if (clipPolyHasLight) {
			short[] lightVtx2 = lightVtx;
			input[offset] = lightVtx2[v0];
			input[attsCount + offset] = lightVtx2[v1];
			input[attsCount * 2 + offset] = lightVtx2[v2];
			offset++;

			if (clipPolyEnvmap) {
				short[] envUVs2 = envUVs;
				input[offset] = envUVs2[v0 * 2];
				input[offset + 1] = envUVs2[v0 * 2 + 1];

				input[attsCount + offset] = envUVs2[v1 * 2];
				input[attsCount + offset + 1] = envUVs2[v1 * 2 + 1];

				input[attsCount * 2 + offset] = envUVs2[v2 * 2];
				input[attsCount * 2 + offset + 1] = envUVs2[v2 * 2 + 1];
				offset += 2;
			}
		}
	}

	private final void setTriangleUVsClip(int au, int av, int bu, int bv, int cu, int cv) {
		int offset = clipAttsCount - 2;

		int[] input = clipBuffers[0];

		input[offset] = au;
		input[offset + 1] = av;

		input[clipAttsCount + offset] = bu;
		input[clipAttsCount + offset + 1] = bv;

		input[clipAttsCount * 2 + offset] = cu;
		input[clipAttsCount * 2 + offset + 1] = cv;
	}

	private final void endTriangleClip(int sortZ) {
		int[] data1 = clipBuffers[0], data2 = null;
		int vertices1 = 3, vertices2 = 0;
		int clippingStages = this.activeClippingStages;

		if ((clippingStages & NEAR_CLIP) != 0) {
			int[] output = clipBuffers[1];
			vertices1 = clipTriangleVerts(data1, vertices1, 2, projNear, output, true);
			data1 = output;
		}

		if ((clippingStages & FAR_CLIP) != 0) {
			int[] output = clipBuffers[2];
			vertices1 = clipTriangleVerts(data1, vertices1, 2, projFar, output, false);
			data1 = output;
		}

		if ((clippingStages & TOON_SPLIT) != 0) {
			int[] output1 = clipBuffers[3];
			int[] output2 = clipBuffers[4];
			vertices2 = splitTriangleVerts(data1, vertices1, 3, efxToonThreshold << 4, output1, output2);
			vertices1 = vertices2 >>> 16;
			vertices2 &= 0xffff;
			data1 = output1;
			data2 = output2;
		}

		this.activeClippingStages = 0;

		submitClippedVerts(data1, vertices1, sortZ);
		if (data2 != null) submitClippedVerts(data2, vertices2, sortZ);
	}

	private final int splitTriangleVerts(int[] vertices, int vtxCount, int clipAttrib, int clipThreshold, int[] left, int[] right) {
		int vtxCountLeft = 0, vtxCountRight = 0;
		int attsCount = this.clipAttsCount;

		for(int i = 0; i < vtxCount; i++) {
			int offsetOrig = i * attsCount;
			int nextI = i == (vtxCount - 1) ? 0 : i + 1;
			int offsetNext = nextI * attsCount;

			int a = vertices[offsetOrig + clipAttrib];
			int b = vertices[offsetNext + clipAttrib];

			//Toon shaded triangles can overlap when whole edge are on the threshold but I dont care
			if (a <= clipThreshold) {
				int offsetLeft = vtxCountLeft * attsCount;
				vtxCountLeft++;

				for(int t = 0; t < attsCount; t++) {
					left[offsetLeft + t] = vertices[offsetOrig + t];
				}
			}

			if (a >= clipThreshold) {
				int offsetRight = vtxCountRight * attsCount;
				vtxCountRight++;

				for(int t = 0; t < attsCount; t++) {
					right[offsetRight + t] = vertices[offsetOrig + t];
				}
			}

			boolean clip = ((a < clipThreshold && b > clipThreshold) || (a > clipThreshold && b < clipThreshold));

			if (clip) {
				int offsetLeft = vtxCountLeft * attsCount;
				int offsetRight = vtxCountRight * attsCount;

				if (a < clipThreshold) {
					//Swap a and b to reduce seams
					int tmp = a; a = b; b = tmp;
					tmp = offsetOrig; offsetOrig = offsetNext; offsetNext = tmp;
				}

				int tmpZ = (int)(((long)clipThreshold - a) * 4096 / ((long)b - a));

				for (int t = 0; t < attsCount; t++) {
					int att;

					int ax = vertices[offsetOrig + t];
					int bx = vertices[offsetNext + t];

					att = (int)((((long)bx - ax) * tmpZ >> 12) + ax);

					left[offsetLeft + t] = att;
					right[offsetRight + t] = att;
				}

				left[offsetLeft + clipAttrib] = clipThreshold;
				vtxCountLeft++;

				right[offsetRight + clipAttrib] = clipThreshold;
				vtxCountRight++;
			}
		}

		return (vtxCountLeft << 16) | vtxCountRight;
	}

	private final int clipTriangleVerts(int[] vertices, int vtxCount, int clipAttrib, int clipThreshold, int[] outVerts, boolean larger) {
		int vtxCountRes = 0;
		int attsCount = this.clipAttsCount;
		
		int sign = larger ? -1 : 1;

		for(int i = 0; i < vtxCount; i++) {
			int offsetOrig = i * attsCount;
			int nextI = i == (vtxCount - 1) ? 0 : i + 1;
			int offsetNext = nextI * attsCount;

			int a = vertices[offsetOrig + clipAttrib];
			int b = vertices[offsetNext + clipAttrib];

			//Toon shaded triangles can overlap when whole edge are on the threshold but I dont care
			if ((larger ? a >= clipThreshold : a <= clipThreshold)) {
				int offsetLeft = vtxCountRes * attsCount;
				vtxCountRes++;

				for(int t = 0; t < attsCount; t++) {
					outVerts[offsetLeft + t] = vertices[offsetOrig + t];
				}
			}

			boolean clip = ((a < clipThreshold && b > clipThreshold) || (a > clipThreshold && b < clipThreshold));

			if (clip) {
				int offsetLeft = vtxCountRes * attsCount;

				if (a < clipThreshold) {
					//Swap a and b to reduce seams
					int tmp = a; a = b; b = tmp;
					tmp = offsetOrig; offsetOrig = offsetNext; offsetNext = tmp;
				}

				int tmpZ = (int)(((long)clipThreshold - a) * 4096 / ((long)b - a));

				for (int t = 0; t < attsCount; t++) {
					int att;

					int ax = vertices[offsetOrig + t];
					int bx = vertices[offsetNext + t];

					att = (int)((((long)bx - ax) * tmpZ >> 12) + ax);

					outVerts[offsetLeft + t] = att;
				}

				outVerts[offsetLeft + clipAttrib] = clipThreshold;
				vtxCountRes++;
			}
		}

		return vtxCountRes;
	}

	private final void submitClippedVerts(int[] verts, int vertsCount, int sortZ) {
		if (vertsCount < 3) return;

		int projMode = projectionMode;
		int scaleX = projScaleX, scaleY = projScaleY;
		int centerX = drawCenterX, centerY = drawCenterY;

		int attsCount = this.clipAttsCount;
		int[] tmpProjVtx = this.tmpClipProjVtx;
		short[] tmpLightVtx = this.tmpClipLightVtx, tmpEnvUVs = this.tmpClipEnvUVs;

        for (int i = 0; i < vertsCount; i++) {
            int z = verts[i * attsCount + 2];
            int divisor = projMode == PROJ_PERSPECTIVE ? z : 0;
            long x = projectCoordinateLong(verts[i * attsCount], scaleX, divisor, centerX);
            long y = projectCoordinateLong(verts[i * attsCount + 1], scaleY, divisor, centerY);
            tmpClipProjectedXY[i * 2] = x; tmpClipProjectedXY[i * 2 + 1] = y;
            tmpProjVtx[i * 3] = clampLong(x, Integer.MIN_VALUE, Integer.MAX_VALUE);
            tmpProjVtx[i * 3 + 1] = clampLong(y, Integer.MIN_VALUE, Integer.MAX_VALUE);
            tmpProjVtx[i * 3 + 2] = z;
        }

		int offset = 3;

		if (clipPolyHasLight) {
			for (int i = 0; i < vertsCount; i++) {
				tmpLightVtx[i] = (short) verts[i * attsCount + 3];
			}
			offset++;

			if (clipPolyEnvmap) {
				for (int i = 0; i < vertsCount; i++) {
					tmpEnvUVs[i * 2] =	 (short) verts[i * attsCount + 4];
					tmpEnvUVs[i * 2 + 1] = (short) verts[i * attsCount + 5];
				}

				offset += 2;
			}
		}

		int[] bckProjVtx = projVtx;
        long[] bckProjectedXY = projectedXY;
		short[] bckLightVtx = lightVtx, bckEnvUVs = envUVs;
		projVtx = tmpProjVtx; projectedXY = tmpClipProjectedXY;
		if (clipPolyHasLight) {
			lightVtx = tmpLightVtx;
			envUVs = tmpEnvUVs;
		}

		boolean tmpLight = (clipPolyMat & Figure.MAT_LIGHTING) != 0;
		boolean tmpEnv = tmpLight && ((clipPolyMat & Figure.MAT_SPECULAR) != 0);
		int polyStride = calcPolygonStride(clipPolyHasUVs, clipPolyFlatNorm, tmpLight, tmpEnv);
		reservePrimBuffers(vertsCount - 1, polyStride);

		for (int i = 1; i < vertsCount - 1; i++) {
			int v0 = 0, v1 = i, v2 = i + 1;

			if (startTriangle(clipPolyMat, clipPolyTexCol, clipPolyEnvmapId, v0, v1, v2, clipPolyNormalId, false)) {
				if (clipPolyHasUVs) {
					setTriangleUVs(
						verts[v0 * attsCount + offset], verts[v0 * attsCount + offset + 1],
						verts[v1 * attsCount + offset], verts[v1 * attsCount + offset + 1],
						verts[v2 * attsCount + offset], verts[v2 * attsCount + offset + 1]
					);
				}

				endTriangle(sortZ);
			}
		}

		projVtx = bckProjVtx; projectedXY = bckProjectedXY;
		if (clipPolyHasLight) {
			lightVtx = bckLightVtx;
			envUVs = bckEnvUVs;
		}
	}
}
