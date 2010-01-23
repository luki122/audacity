// Microsoft Windows specific include file

#define MP3SUPPORT 1
#define USE_FFMPEG 1	//define this to build with ffmpeg import/export
#define USE_LADSPA 1
#define USE_LIBFLAC 1
#define USE_LIBID3TAG 1
#define USE_LIBLRDF 1
#define USE_LIBMAD 1
#define USE_LIBRESAMPLE 1
#undef USE_LIBSAMPLERATE
#define USE_LIBTWOLAME 1
#define USE_LIBVORBIS 1
#define USE_NYQUIST 1
#define USE_PORTMIXER 1
// #define USE_SLV2 1
#define USE_SBSMS 1
#define USE_SOUNDTOUCH 1
#define USE_VAMP 1
#define USE_VST 1
#define USE_MIDI 1 // define this to use portSMF and PortMidi for midi file support

#define INSTALL_PREFIX "."

#define rint(x)   (floor((x)+0.5f)) 

#ifdef _DEBUG
    #ifdef _MSC_VER
        #include <crtdbg.h>
    #endif
#endif

// arch-tag: dcb2defc-1c07-4bae-a9ca-c5377cb470e4

