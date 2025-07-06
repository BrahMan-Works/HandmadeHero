#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#include <dsound.h>
#include <math.h>
#include <stdio.h>

#include "handmade.h"
#include "handmade.cpp"
#include "win32_handmade.h"

#define internal static
#define local_persist static
#define global_variable static

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef float real32;
typedef double real64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)

typedef X_INPUT_GET_STATE(x_input_get_state);
typedef X_INPUT_SET_STATE(x_input_set_state);

X_INPUT_GET_STATE(XInputGetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}

X_INPUT_SET_STATE(XInputSetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}

global_variable x_input_get_state* XInputGetState_ = XInputGetStateStub;
global_variable x_input_set_state* XInputSetState_ = XInputSetStateStub;

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

internal void
Win32LoadXInput(void)
{
    HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
    if (!XInputLibrary)
    {
        //logging
        XInputLibrary = LoadLibraryA("xinput1_3.dll");
    }

    if (XInputLibrary)
    {
        x_input_get_state* GetState = (x_input_get_state*)GetProcAddress(XInputLibrary, "XInputGetState");
        if (GetState)
        {
            XInputGetState_ = GetState;
        }
        else
        {
            //logging
            XInputGetState_ = XInputGetStateStub;
        }

        x_input_set_state* SetState = (x_input_set_state*)GetProcAddress(XInputLibrary, "XInputSetState");
        if (SetState)
        {
            XInputSetState_ = SetState;
        }
        else
        {
            //logging
            XInputSetState_ = XInputSetStateStub;
        }

        //logging
    }
    else
    {
        //logging
    }
}

//Macro indirection after load to prevent conflicts
#define XInputGetState XInputGetState_
#define XInputSetState XInputSetState_

internal void
Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
    if (DSoundLibrary)
    {
        direct_sound_create* DirectSoundCreate = (direct_sound_create*)GetProcAddress(DSoundLibrary, "DirectSoundCreate");

        LPDIRECTSOUND DirectSound;
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
        {
            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
            WaveFormat.cbSize = 0;

            if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
            {
                DSBUFFERDESC BufferDescription = {};
                BufferDescription.dwSize = sizeof(BufferDescription);
                BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

                //create a primary buffer
                LPDIRECTSOUNDBUFFER PrimaryBuffer;
                if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
                {
                    HRESULT Error = PrimaryBuffer->SetFormat(&WaveFormat);
                    if (SUCCEEDED(Error))
                    {
                        // NOTE(casey): We have finally set the format!
                        OutputDebugStringA("Primary buffer format was set.\n");
                    }
                    else
                    {
                        //logging
                    }
                }
                else
                {
                    //logging
                }
            }
            else
            {
                //logging
            }

            //create a secondary buffer

            DSBUFFERDESC BufferDescription = {};
            BufferDescription.dwSize = sizeof(BufferDescription);
            BufferDescription.dwFlags = 0;
            BufferDescription.dwBufferBytes = BufferSize;
            BufferDescription.lpwfxFormat = &WaveFormat;
            HRESULT Error = DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0);
            if (SUCCEEDED(Error))
            {

            }
        }
        else
        {
            //logging
        }
    }
    else
    {
        //logging
    }
}

internal win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;

    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;

    return Result;
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer* Buffer, int Width, int Height)
{
    if (Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;
    Buffer->BytesPerPixel = 4;

    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    int BitmapMemorySize = (Buffer->Width * Buffer->Height) * Buffer->BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

    Buffer->Pitch = Width * Buffer->BytesPerPixel;
}

internal void
Win32DisplayBufferInWindow(win32_offscreen_buffer* Buffer, HDC DeviceContext, int WindowWidth, int WindowHeight)
{
    StretchDIBits(DeviceContext,
        0, 0, WindowWidth, WindowHeight,
        0, 0, Buffer->Width, Buffer->Height,
        Buffer->Memory,
        &Buffer->Info,
        DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK
Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT result = 0;

    switch (Message)
    {
    case WM_SIZE:
    {
        //logging
    } break;

    case WM_DESTROY:
    {
        //Handle this as an error - try to recreate window
        //logging
        GlobalRunning = false;
    } break;

    case WM_CLOSE:
    {
        //Handle this as a message to the user
        //logging
        GlobalRunning = false;
    } break;

    case WM_ACTIVATEAPP:
    {
        OutputDebugStringA("WM_ACTIVATEAPP\n");
        //logging
    } break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
    {
        uint32 VKCode = WParam;
        bool WasDown = ((LParam & (1 << 30)) != 0);
        bool IsDown = ((LParam & (1 << 31)) == 0);

        if (WasDown != IsDown)
        {
            if (VKCode == VK_DOWN)
            {
                OutputDebugStringA("VK_DOWN\n");
            }
        }

        bool32 AltKeyWasDown = LParam & (1 << 29);
        if ((VKCode == VK_F4) && AltKeyWasDown)
        {
            GlobalRunning = false;
        }
    } break;

    case WM_PAINT:
    {
        PAINTSTRUCT Paint;
        HDC DeviceContext = BeginPaint(Window, &Paint);

        win32_window_dimension Dimension = Win32GetWindowDimension(Window);
        Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);

        EndPaint(Window, &Paint);
    } break;

    default:
    {
        //logging
        result = DefWindowProcA(Window, Message, WParam, LParam);
    } break;
    }

    return result;
}

internal void
Win32ClearBuffer(win32_sound_output* SoundOutput)
{
    VOID* Region1;
    DWORD Region1Size;
    VOID* Region2;
    DWORD Region2Size;
    if (SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize,
        &Region1, &Region1Size,
        &Region2, &Region2Size,
        0)))
    {
        uint8* DestSample = (uint8*)Region1;
        for (DWORD ByteIndex = 0;
            ByteIndex < Region1Size;
            ++ByteIndex)
        {
            *DestSample++ = 0;
        }

        DestSample = (uint8*)Region2;
        for (DWORD ByteIndex = 0;
            ByteIndex < Region2Size;
            ++ByteIndex)
        {
            *DestSample++ = 0;
        }

        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal void
Win32FillSoundBuffer(win32_sound_output* SoundOutput, DWORD ByteToLock, DWORD BytesToWrite, game_sound_output_buffer* SourceBuffer)
{
    VOID* Region1;
    DWORD Region1Size;
    VOID* Region2;
    DWORD Region2Size;
    if (SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite,
        &Region1, &Region1Size,
        &Region2, &Region2Size,
        0)))
    {
        DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
        int16* DestSample = (int16*)Region1;
        int16* SourceSample = SourceBuffer->Samples;
        for (DWORD SampleIndex = 0;
            SampleIndex < Region1SampleCount;
            ++SampleIndex)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }

        DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
        DestSample = (int16*)Region2;
        for (DWORD SampleIndex = 0;
            SampleIndex < Region2SampleCount;
            ++SampleIndex)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }

        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal void
Win32ProcessXInputDigitalButton(DWORD XInputButtonState, game_button_state* OldState, DWORD ButtonBit, game_button_state* NewState)
{
    NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
    NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

int CALLBACK
main()
{
    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    int64 PerfCountFrequency = PerfCountFrequencyResult.QuadPart;
    uint64 LastCycleCount = __rdtsc();

    Win32LoadXInput();

    WNDCLASSA WindowClass = {};
    WindowClass.style = CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = GetModuleHandleA(0);
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

    if (RegisterClassA(&WindowClass))
    {
        HWND Window = CreateWindowExA(
            0,
            WindowClass.lpszClassName,
            "Handmade Hero",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT,
            0, 0, GetModuleHandleA(0), 0);

        if (Window)
        {
            HDC DeviceContext = GetDC(Window);
            win32_sound_output SoundOutput = {};         
            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.BytesPerSample = sizeof(int16) * 2;
            SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
            SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;
            Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
            Win32ClearBuffer(&SoundOutput);
            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

            GlobalRunning = true;
            int16* Samples = (int16*)VirtualAlloc(0, SoundOutput.SecondaryBufferSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
           
            game_input Input[2];
            game_input* NewInput = &Input[0];
            game_input* OldInput = &Input[1];
          
            LARGE_INTEGER LastCounter;
            QueryPerformanceCounter(&LastCounter);
            while (GlobalRunning)
            {
                MSG Message;

                

                while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if (Message.message == WM_QUIT)
                    {
                        GlobalRunning = false;
                        //logging
                    }

                    TranslateMessage(&Message);
                    DispatchMessageA(&Message);

                }

                //Poll gamepad input
                int MaxControllerCount = XUSER_MAX_COUNT;
                if (MaxControllerCount > ArrayCount(NewInput->Controllers))
                {
                    MaxControllerCount = ArrayCount(NewInput->Controllers);
                }

                for (DWORD ControllerIndex = 0;
                    ControllerIndex < MaxControllerCount;
                    ControllerIndex++)
                {
                    game_controller_input* OldController = &OldInput->Controllers[ControllerIndex];
                    game_controller_input* NewController = &NewInput->Controllers[ControllerIndex];

                    XINPUT_STATE ControllerState;
                    if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                    {
                        XINPUT_GAMEPAD* Pad = &ControllerState.Gamepad;

                        bool32 Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                        bool32 Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                        bool32 Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                        bool32 Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

                        NewController->IsAnalog = true;
                        NewController->StartX = OldController->EndX;
                        NewController->StartY = OldController->EndY;

                        // TODO(casey): Min/Max macros!!!
                        real32 X;
                        if (Pad->sThumbLX < 0)
                        {
                            X = (real32)Pad->sThumbLX / 32768.0f;
                        }
                        else
                        {
                            X = (real32)Pad->sThumbLX / 32767.0f;
                        }
                        NewController->MinX = NewController->MaxX = NewController->EndX = X;

                        real32 Y;
                        if (Pad->sThumbLY < 0)
                        {
                            Y = (real32)Pad->sThumbLY / 32768.0f;
                        }
                        else
                        {
                            Y = (real32)Pad->sThumbLY / 32767.0f;
                        }
                        NewController->MinY = NewController->MaxY = NewController->EndY = Y;

                        Win32ProcessXInputDigitalButton(Pad->wButtons,
                            &OldController->Down, XINPUT_GAMEPAD_A,
                            &NewController->Down);
                        Win32ProcessXInputDigitalButton(Pad->wButtons,
                            &OldController->Right, XINPUT_GAMEPAD_B,
                            &NewController->Right);
                        Win32ProcessXInputDigitalButton(Pad->wButtons,
                            &OldController->Left, XINPUT_GAMEPAD_X,
                            &NewController->Left);
                        Win32ProcessXInputDigitalButton(Pad->wButtons,
                            &OldController->Up, XINPUT_GAMEPAD_Y,
                            &NewController->Up);
                        Win32ProcessXInputDigitalButton(Pad->wButtons,
                            &OldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER,
                            &NewController->LeftShoulder);
                        Win32ProcessXInputDigitalButton(Pad->wButtons,
                            &OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER,
                            &NewController->RightShoulder);

                        
                        /*bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
                        bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);*/
                    }
                    else
                    {
                        //Controller is not available
                        //logging
                    }
                }

                /*XINPUT_VIBRATION Vibration;
                Vibration.wLeftMotorSpeed = 60000;
                Vibration.wRightMotorSpeed = 60000;
                XInputSetState(0, &Vibration);*/

                DWORD ByteToLock;
                DWORD TargetCursor;
                DWORD BytesToWrite;
                DWORD PlayCursor;
                DWORD WriteCursor;
                bool32 SoundIsValid = false;
                if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
                {
                    ByteToLock = ((SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) %
                        SoundOutput.SecondaryBufferSize);

                    TargetCursor =
                        ((PlayCursor +
                            (SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample)) %
                            SoundOutput.SecondaryBufferSize);
                    if (ByteToLock > TargetCursor)
                    {
                        BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
                        BytesToWrite += TargetCursor;
                    }
                    else
                    {
                        BytesToWrite = TargetCursor - ByteToLock;
                    }

                    SoundIsValid = true;
                }

                
                game_offscreen_buffer Buffer = {};
                Buffer.Height = GlobalBackBuffer.Height;
                Buffer.Memory = GlobalBackBuffer.Memory;
                Buffer.Pitch = GlobalBackBuffer.Pitch;
                Buffer.Width = GlobalBackBuffer.Width;

                game_sound_output_buffer SoundBuffer = {};
                SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
                SoundBuffer.Samples = Samples;

                GameUpdateAndRender(NewInput, &SoundBuffer, &Buffer);

                if (SoundIsValid)
                {
                    Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
                }

                win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);
                ReleaseDC(Window, DeviceContext);

                LARGE_INTEGER EndCounter;
                QueryPerformanceCounter(&EndCounter);
                uint64 EndCycleCount = __rdtsc();

                uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
                int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
                LastCounter = EndCounter;
                LastCycleCount = EndCycleCount;

                game_input* Temp = OldInput;
                OldInput = NewInput;
                NewInput = Temp;

                real32 MSPerFrame= (real32)((1000.0f * (real32)CounterElapsed) / (real32)PerfCountFrequency);
                real32 FPS = (real32)PerfCountFrequency / (real32)CounterElapsed;
                real32 McyclesPerFrame = (real32)(CyclesElapsed / (1000.0f * 1000.0f));

                /*char Buffer[256];
                sprintf_s(Buffer, "MiliSeconds/Frame,   FPS,   MegaCycles/Frame: %.02fms / %.02fFPS / %.02fmc/f\n", MSPerFrame, FPS, McyclesPerFrame);
                OutputDebugStringA(Buffer);*/
            }
        }
        else
        {
            //logging
        }
    }
    else
    {
        //logging
    }

    return 0;
}
