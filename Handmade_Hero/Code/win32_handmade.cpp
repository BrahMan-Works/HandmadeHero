#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#include <dsound.h>

#define internal static
#define local_persist static
#define global_variable static

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void* Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};

struct win32_window_dimension {
    int Width;
    int Height;
};

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
                if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
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
RenderWeirdGradient(win32_offscreen_buffer* Buffer, int BlueOffset, int GreenOffset)
{
    uint8* Row = (uint8*)Buffer->Memory;
    for (int Y = 0; Y < Buffer->Height; ++Y)
    {
        uint32* Pixel = (uint32*)Row;
        for (int X = 0; X < Buffer->Width; ++X)
        {
            uint8 Blue = (X + BlueOffset);
            uint8 Green = (Y + GreenOffset);

            *Pixel++ = ((Green << 8) | Blue);
        }

        Row += Buffer->Pitch;
    }
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

int CALLBACK
main()
{
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
            int XOffset = 0;
            int YOffset = 0;
            
            int SamplesPerSecond = 48000;
            int ToneHz = 256;
            int ToneVolume = 10000;
            uint32 RunningSampleIndex = 0;
            int SquareWavePeriod = SamplesPerSecond / ToneHz;
            int HalfSquareWavePeriod = SquareWavePeriod / 2;
            int BytesPerSample = sizeof(int16) * 2;
            int SecondaryBufferSize = SamplesPerSecond * BytesPerSample;

            Win32InitDSound(Window, SamplesPerSecond, SecondaryBufferSize);
            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

            GlobalRunning = true;
            while (GlobalRunning)
            {
                HDC DeviceContext = GetDC(Window);
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
                for (DWORD ControllerIndex = 0;
                    ControllerIndex < XUSER_MAX_COUNT;
                    ControllerIndex++)
                {
                    XINPUT_STATE ControllerState;
                    if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                    {
                        XINPUT_GAMEPAD* Pad = &ControllerState.Gamepad;

                        bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                        bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                        bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                        bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                        bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
                        bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
                        bool LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
                        bool RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
                        bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
                        bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
                        bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
                        bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);

                        int16_t StickX = Pad->sThumbLX;
                        int16_t StickY = Pad->sThumbLY;

                        XOffset += StickX >> 12;
                        YOffset += StickY >> 12;

                    }
                    else
                    {
                        //Controller is not available
                        //logging
                    }
                }

                XINPUT_VIBRATION Vibration;
                Vibration.wLeftMotorSpeed = 60000;
                Vibration.wRightMotorSpeed = 60000;
                XInputSetState(0, &Vibration);

                RenderWeirdGradient(&GlobalBackBuffer, XOffset, YOffset);
                XOffset++;

                //do directsound output test
                DWORD PlayCursor;
                DWORD WriteCursor;
                if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
                {
                    DWORD BytesToLock = RunningSampleIndex * BytesPerSample % SecondaryBufferSize;
                    DWORD BytesToWrite;
                    if (BytesToLock > PlayCursor)
                    {
                        BytesToWrite = SecondaryBufferSize - BytesToLock;
                        BytesToWrite += PlayCursor;
                    }
                    else
                    {
                        BytesToWrite = PlayCursor - BytesToLock;
                    }

                    // int16 int16 int16 ...
                    // [LEFT RIGHT] LEFT RIGHT LEFT RIGHT LEFT ...
                    //
                    VOID* Region1;
                    DWORD Region1Size;
                    VOID* Region2;
                    DWORD Region2Size;
                    if (SUCCEEDED(GlobalSecondaryBuffer->Lock(BytesToLock, BytesToWrite,
                        &Region1, &Region1Size,
                        &Region2, &Region2Size,
                        0)))
                    {
                        // TODO(casey): assert that Region1Size/Region2Size is valid
                        int16* SampleOut = (int16*)Region1;
                        DWORD Region1SampleCount = Region1Size / BytesPerSample;
                        DWORD Region2SampleCount = Region2Size / BytesPerSample;
                        for (DWORD SampleIndex = 0;
                            SampleIndex < Region1SampleCount;
                            ++SampleIndex)
                        {
                            int16 SampleValue = ((RunningSampleIndex++ / HalfSquareWavePeriod) % 2 )? ToneVolume : -ToneVolume;
                            *SampleOut++ = SampleValue;
                            *SampleOut++ = SampleValue;
                        }

                        SampleOut = (int16*)Region2;
                        for (DWORD SampleIndex = 0;
                            SampleIndex < Region2SampleCount;
                            ++SampleIndex)
                        {
                            int16 SampleValue = ((RunningSampleIndex++ / HalfSquareWavePeriod) % 2) ? ToneVolume : -ToneVolume;
                            *SampleOut++ = SampleValue;
                            *SampleOut++ = SampleValue;
                        }

                        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
                    }
                }

                
                win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height);
                ReleaseDC(Window, DeviceContext);
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
