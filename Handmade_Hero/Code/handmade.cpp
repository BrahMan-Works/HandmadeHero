#include "handmade.h"


static void
RenderWeirdGradient(game_offscreen_buffer* Buffer, int XOffset, int YOffset)
{
    uint8* Row = (uint8*)Buffer->Memory;
    for (int Y = 0;
        Y < Buffer->Height;
        ++Y)
    {
        uint32* Pixel = (uint32*)Row;
        for (int X = 0;
            X < Buffer->Width;
            ++X)
        {
            uint8 Blue = (X + XOffset);
            uint8 Green = (Y + YOffset);

            *Pixel++ = ((Green << 8) | Blue);
        }

        Row += Buffer->Pitch;
    }
}

internal void
GameUpdateAndRender(game_offscreen_buffer* Buffer, int XOffset, int YOffset)
{
    XOffset = 0;
    YOffset = 0;
    RenderWeirdGradient(Buffer, XOffset, YOffset);
}
