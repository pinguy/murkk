#ifndef MINI_SDL_H
#define MINI_SDL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" { 
#endif

typedef uint8_t Uint8; typedef int8_t Sint8; typedef uint16_t Uint16; typedef int16_t Sint16; typedef uint32_t Uint32; typedef int32_t Sint32; typedef uint64_t Uint64; typedef int64_t Sint64;
typedef struct SDL_Window SDL_Window; typedef void* SDL_GLContext; typedef Uint32 SDL_AudioDeviceID;
#define SDL_INIT_TIMER 0x00000001u
#define SDL_INIT_AUDIO 0x00000010u
#define SDL_INIT_VIDEO 0x00000020u
#define SDL_WINDOWPOS_CENTERED 0x2FFF0000u
#define SDL_WINDOW_OPENGL 0x00000002u
#define SDL_GL_DOUBLEBUFFER 5
#define SDL_GL_DEPTH_SIZE 6
#define SDL_FALSE 0
#define SDL_TRUE 1
#define SDL_QUIT 0x100
#define SDL_KEYDOWN 0x300
#define SDL_KEYUP 0x301
#define SDL_MOUSEMOTION 0x400
#define SDL_MOUSEBUTTONDOWN 0x401
#define SDL_MOUSEBUTTONUP 0x402
#define SDL_BUTTON_LEFT 1
#define SDLK_ESCAPE 27
#define SDLK_w 'w'
#define SDLK_a 'a'
#define SDLK_s 's'
#define SDLK_d 'd'
#define AUDIO_F32SYS 0x8120

typedef struct SDL_AudioSpec { int freq; Uint16 format; Uint8 channels; Uint8 silence; Uint16 samples; Uint16 padding; Uint32 size; void (*callback)(void*,Uint8*,int); void *userdata; } SDL_AudioSpec;
typedef struct SDL_Keysym { int scancode; int sym; Uint16 mod; Uint32 unused; } SDL_Keysym;
typedef struct SDL_KeyboardEvent { Uint32 type; Uint32 timestamp; Uint32 windowID; Uint8 state; Uint8 repeat; Uint8 padding2; Uint8 padding3; SDL_Keysym keysym; } SDL_KeyboardEvent;
typedef struct SDL_MouseMotionEvent { Uint32 type; Uint32 timestamp; Uint32 windowID; Uint32 which; Uint32 state; Sint32 x; Sint32 y; Sint32 xrel; Sint32 yrel; } SDL_MouseMotionEvent;
typedef struct SDL_MouseButtonEvent { Uint32 type; Uint32 timestamp; Uint32 windowID; Uint32 which; Uint8 button; Uint8 state; Uint8 clicks; Uint8 padding1; Sint32 x; Sint32 y; } SDL_MouseButtonEvent;
typedef union SDL_Event { Uint32 type; SDL_KeyboardEvent key; SDL_MouseMotionEvent motion; SDL_MouseButtonEvent button; Uint8 padding[56]; } SDL_Event;

int SDL_Init(Uint32 flags); int SDL_InitSubSystem(Uint32 flags); void SDL_Quit(void);
int SDL_setenv(const char *name, const char *value, int overwrite);
const char *SDL_GetError(void); Uint32 SDL_GetTicks(void);
int SDL_GL_SetAttribute(int attr, int value); SDL_GLContext SDL_GL_CreateContext(SDL_Window *window); void SDL_GL_DeleteContext(SDL_GLContext context); int SDL_GL_SetSwapInterval(int interval); void SDL_GL_SwapWindow(SDL_Window *window); void *SDL_GL_GetProcAddress(const char *proc);
SDL_Window *SDL_CreateWindow(const char *title, int x, int y, int w, int h, Uint32 flags); void SDL_DestroyWindow(SDL_Window *window);
int SDL_PollEvent(SDL_Event *event); int SDL_SetRelativeMouseMode(int enabled);
SDL_AudioDeviceID SDL_OpenAudioDevice(const char *device, int iscapture, const SDL_AudioSpec *desired, SDL_AudioSpec *obtained, int allowed_changes); void SDL_PauseAudioDevice(SDL_AudioDeviceID dev, int pause_on); void SDL_CloseAudioDevice(SDL_AudioDeviceID dev);
#ifdef __cplusplus
}
#endif
#endif
