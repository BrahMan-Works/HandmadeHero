#if !defined(HANDMADE_H)
#include <stdint.h>

typedef unsigned char uint8;
typedef unsigned int uint32;

#define internal static
#define local_persist static
#define global_variable static

/*
    Services that the platform layer provides to the game
*/

/*
    Services that the game provides to the platform layer.
*/

struct game_offscreen_buffer
{
    void* Memory;
    int Width;
    int Height;
    int Pitch;
};

static void GameUpdateAndRender(game_offscreen_buffer* Buffer, int XOffset, int YOffset);

#define HANDMADE_H
#endif
