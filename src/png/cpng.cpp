#include "png.h"

class Cpng { // TPng = class(TGraphic)
  private:
    byte *tmpBuf;
    int tmpi;
    TPngImage FImage;
    long FBgColor; // DL Background color Added 30/05/2000
    int FTransparent; // DL Is this Image Transparent   Added 30/05/2000
    long FRowBytes;   //DL Added 30/05/2000
    double FGamma; //DL Added 07/06/2000
    double FScreenGamma; //DL Added 07/06/2000
    byte *FRowPtrs; // DL Changed for consistancy 30/05/2000
  protected:
    void InitializeDemData();
  public:
    byte* Data; //property Data: pByte read fData;
    char* Title;
    char* Author;
    char* Description;
    int BitDepth;
    int BytesPerPixel;
    int ColorType;
    int Height;
    int Width;
    int Interlace;
    int Compression;
    int Filter;
    double LastModified;
    int Transparent;
    Cpng();//constructor Create; override;
    ~Cpng();//destructor Destroy; override;
    int LoadFromBuffer(byte* buf,int len);
};


void Cpng::InitializeDemData() {
  long* cvaluep; //ub
  long y;

  // Initialize Data and RowPtrs
  if(Data) {
    free(Data);
    Data = 0;
  }
  if(FRowPtrs) {
    free(FRowPtrs);
    FRowPtrs = 0;
  }
  Data = malloc(Height * FRowBytes ); // DL Added 30/5/2000
  FRowPtrs = malloc(sizeof(void*) * Height);

  if(Data)&&(FRowPtrs) {
    cvaluep = FRowPtrs;
    for(y=0;y<Height;y++){
      *cvaluep = long(Data) + ( y * FRowBytes ); //DL Added 08/07/2000
      cvaluep++;
    }
  }
}

void __stdcall fReadData(png_structp png_ptr;byte *data;png_size_t length) { // called by pnglib
  int i;
  for(i=0;i<length;i++) data[i] = tmpBuf[tmpi++];    // give pnglib a some more bytes
}

int Cpng::LoadFromBuffer(byte* buf,int len) { //procedure TPng.LoadFromStream( Stream: TStream );
  png_structp png;
  png_infop pnginfo;
  char tmp[32];
  byte sig[4];
  png_textp Txt;
  int i,nTxt;
  char* s;
  png_timep Time;
  png_color_16p pBackground;
  Byte RGBValue;
  byte ioBuffer[8192];

  tmpBuf = buf;
  tmpi=0;

  if png_sig_cmp( buf, 0, sizeof( sig )) <> 0 then
   raise Exception.Create( 'Is not a valid PNG !' );

  png := png_create_read_struct(PNG_LIBPNG_VER_STRING,0,0,0);
  if(!png) return 1;

  pnginfo := png_create_info_struct( png );

  if(!pnginfo) {
    png_destroy_read_struct(png,pnginfo, nil);
    return 2;
  }
  png_set_sig_bytes(png,0/*sizeof( sig )*/);
  png_set_read_fn( png, ioBuffer, fReadData );
  png_read_info( png, pnginfo );

  png_get_IHDR(png, pnginfo, Width, Height,BitDepth, ColorType, Interlace, Compression, Filter );

  // ...removed bgColor code here...

  // if bit depth is less than or equal 8 then expand...
  if (( FColorType == PNG_COLOR_TYPE_PALETTE ) && ( FBitDepth <= 8 ))
    png_set_palette_to_rgb( png ); // DL Changed to be more readable

  if (( FColorType == PNG_COLOR_TYPE_GRAY ) && ( FBitDepth < 8 ))
    png_set_gray_1_2_4_to_8( png );  // DL Changed to be more readable

  // Add alpha channel if present
  if(png_get_valid( png, pnginfo, PNG_INFO_tRNS ))
    png_set_tRNS_to_alpha(png); // DL Changed to be more readable

  if (( FColorType == PNG_COLOR_TYPE_GRAY) || (FColorType == PNG_COLOR_TYPE_GRAY_ALPHA ))
    png_set_gray_to_rgb( png );

  // ...removed gamma code here...

  // Change to level of transparency
  png_set_invert_alpha( png ); // Moved 30/5/2000

  // expand images to 1 pixel per byte
  if (FBitDepth < 8) png_set_packing(png);

  // Swap 16 bit images to PC Format
  if (FBitDepth == 16) png_set_swap( png );

  // update the info structure
  png_read_update_info( png, pnginfo );
  //png_get_IHDR(png, pnginfo, FWidth, FHeight, FBitDepth, FColorType, Finterlace, Fcompression, Ffilter );

  FRowBytes := png_get_rowbytes( png, pnginfo );
  BytesPerPixel := png_get_channels( png, pnginfo );  // DL Added 30/08/2000

  InitializeDemData();
  if((Data)&&(FRowPtrs)) png_read_image(png, png_bytepp(FRowPtrs));
  png_read_end(png, pnginfo); // read last information chunks
  png_destroy_read_struct(@png, @pnginfo, nil);
}

// constructor
Cpng::Cpng() {
  Data    = 0;
  RowPtrs = 0;
  Height  = 0;
  Width   = 0;
  // ub default values
  ColorType = PNG_COLOR_TYPE_RGB;
  interlace = PNG_INTERLACE_NONE;
  compression = PNG_COMPRESSION_TYPE_DEFAULT;
  filter  = PNG_FILTER_TYPE_DEFAULT;
}

// destructor
Cpng::~Cpng() {
  FImage.Release;
  if FData <> nil then
    FreeMem(FData);
  if FRowPtrs <> nil then
    FreeMem(FRowPtrs);
  inherited;
}

/*procedure TPng.CopyToBmp(var aBmp: TBitmap);
var
  valuep:  PByte;
  h, w, x, y:    Integer;
  ndx:     Integer;
  sl:      PByteArray;  // Scanline of bitmap
  slbpp:   Integer;     // Scanline bytes per pixel
  a, r, g, b: Byte;
begin
  if Height > Cardinal( MaxInt ) then
    raise Exception.Create( 'Image too high' );
  if Width > Cardinal( MaxInt ) then
    raise Exception.Create( 'Image too wide' );
  h := FHeight;
  w := FWidth;
  if aBmp.Height < h then
    aBmp.Height := h;
  if aBmp.Width < w then
    aBmp.Width  := w;


  case FBytesPerPixel of
    2: begin
      aBmp.Transparent := Transparent;
      aBmp.PixelFormat := pf16Bit;
      slbpp := 2;
    end;
    4: begin
      aBmp.PixelFormat := pf32Bit;
      slbpp := 4;
    end;
    else begin
      aBmp.Transparent := Transparent;
      aBmp.PixelFormat := pf24Bit;
      slbpp := 3;
    end;
  end;

  // point to data
  valuep := FData;
  for y := 0 to FHeight - 1 do
  begin
    sl := aBmp.Scanline[ y ];  // current scanline
    for x := 0 to FWidth - 1 do
    begin
      ndx := x * slbpp;    // index into current scanline
      if FBytesPerPixel = 2 then
      begin
        // handle 16bit grayscale images, this will display them
        // as a 16bit color image, kinda hokie but fits my needs
        // without altering the data.
        sl[ndx]     := valuep^;
        Inc(valuep);
        sl[ndx + 1] := valuep^;
        Inc(valuep);
      end
      else if FBytesPerPixel = 3 then
      begin
        // RGB - swap blue and red for windows format
        sl[ndx + 2] := valuep^;
        Inc(valuep);
        sl[ndx + 1] := valuep^;
        Inc(valuep);
        sl[ndx]     := valuep^;
        Inc(valuep);
      end
      else  // 4 bytes per pixel of image data
      begin
        sl[ndx + 0] := valuep^;
        Inc(valuep);
        sl[ndx + 1] := valuep^;
        Inc(valuep);
        sl[ndx + 2] := valuep^;
        Inc(valuep);
        sl[ndx+3]     := 255-valuep^;
        Inc(valuep);
      end;
    end;
  end;
end;*/
