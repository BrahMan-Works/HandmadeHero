#if !defined(hANDMADE_H)
#include <stdint.h>
#include <math.h>

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

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

struct game_sound_output_buffer
{
    int SamplesPerSecond;
    int SampleCount;
    int16_t* Samples;
};

struct game_button_state
{
    int HalfTransitionCount;
    bool EndedDown;
};

struct game_controller_input
{
    bool IsAnalog;

    float StartX;
    float StartY;
 
    float MinX;
    float MinY;
 
    float MaxX;
    float MaxY;
 
    float EndX;
    float EndY;

    union
    {
        game_button_state Buttons[6];
        struct
        {
            game_button_state Up;
            game_button_state Down;
            game_button_state Left;
            game_button_state Right;
            game_button_state LeftShoulder;
            game_button_state RightShoulder;
        };
    };
};

struct game_input
{
    game_controller_input Controllers[4];
};


static void GameUpdateAndRender(game_input* Input, game_sound_output_buffer* SoundBuffer, game_offscreen_buffer* Buffer);

#define hANDMADE_H
#endif
