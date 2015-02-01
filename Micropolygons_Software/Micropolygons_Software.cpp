//--------------------------------------------------------------------------------------
// File: Micropolygons_Software.cpp
//
// Software micropolygon renderer front end.
//
// Copyright 2009 (c) Simon Taylor. All rights reserved.
//--------------------------------------------------------------------------------------

#include "stdafx.h"
#include "resource.h"

#include "cSoftwareRasterizer.h"
#include "cScene.h"
#include "cSceneRenderer.h"
#include "Utility.h"

#include <vector>

using namespace MicropolygonCommon;

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE   g_hInst = NULL;
HWND        g_hWnd = NULL;

// Window dimensions.
UINT	g_Width;
UINT	g_Height;

// "Back buffer"
std::vector<DWORD>	g_Buffer;

// The scene and renderer.
cScene			g_Scene;
cSceneRenderer	g_Renderer(&g_Scene);

// Render parameters.
UINT	g_SuperSampleFactor = 4;
float	g_FilterWidth = 3.0f;

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow );
LRESULT CALLBACK    WndProc( HWND, UINT, WPARAM, LPARAM );
void Resize(UINT Width, UINT Height);
void OnKeyboard(WPARAM wParam, LPARAM lParam);
void InitScene();
void Render();
void DrawHUD(HDC hdc);


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPWSTR /*lpCmdLine*/, int nCmdShow )
{
    if( FAILED( InitWindow( hInstance, nCmdShow ) ) )
        return 0;

	InitScene();

    // Main message loop
    MSG msg = {0};
    while( WM_QUIT != msg.message )
    {
        if( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
        {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
        else
        {
            Render();
        }
    }

    return ( int )msg.wParam;
}


//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow )
{
    // Register class
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof( WNDCLASSEX );
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon( hInstance, ( LPCTSTR )IDI_TUTORIAL1 );
    wcex.hCursor = LoadCursor( NULL, IDC_ARROW );
    wcex.hbrBackground = ( HBRUSH )( COLOR_WINDOW + 1 );
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = L"TutorialWindowClass";
    wcex.hIconSm = LoadIcon( wcex.hInstance, ( LPCTSTR )IDI_TUTORIAL1 );
    if( !RegisterClassEx( &wcex ) )
        return E_FAIL;

    // Create window
    g_hInst = hInstance;
    RECT rc = { 0, 0, 640, 480 };
    AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );
    g_hWnd = CreateWindow( L"TutorialWindowClass", L"Direct3D 10 Tutorial 0: Setting Up Window", WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance,
                           NULL );
    if( !g_hWnd )
        return E_FAIL;

    ShowWindow( g_hWnd, nCmdShow );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    PAINTSTRUCT ps;
    HDC hdc;

    switch( message )
    {
        case WM_PAINT:
            hdc = BeginPaint( hWnd, &ps );
            EndPaint( hWnd, &ps );
            break;

        case WM_DESTROY:
            PostQuitMessage( 0 );
            break;

		case WM_SIZE:
			Resize(LOWORD(lParam), HIWORD(lParam));
			break;

		case WM_KEYDOWN:
			OnKeyboard(wParam, lParam);
			break;

        default:
            return DefWindowProc( hWnd, message, wParam, lParam );
    }

    return 0;
}

//--------------------------------------------------------------------------------------
// Resize notifications.
//--------------------------------------------------------------------------------------
void Resize(UINT Width, UINT Height)
{
	g_Width = Width;
	g_Height = Height;
	g_Buffer.resize(Width * Height);
}

//--------------------------------------------------------------------------------------
// Keyboard input handler.
//--------------------------------------------------------------------------------------
void OnKeyboard(WPARAM wParam, LPARAM /*lParam*/)
{
	bool bShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

	switch (wParam)
	{
	case VK_ESCAPE:
		PostQuitMessage(0);
		break;

	case 'S':
		if (bShift)
			g_SuperSampleFactor = Min(16u, g_SuperSampleFactor << 1);
		else
			g_SuperSampleFactor = Max(1u, g_SuperSampleFactor >> 1);
		break;

	case 'M':
		if (bShift)
			g_Renderer.SetMicropolygonSize(g_Renderer.GetMicropolygonSize() * 2.0f);
		else
			g_Renderer.SetMicropolygonSize(g_Renderer.GetMicropolygonSize() * 0.5f);
		break;

	case 'F':
		if (bShift)
			g_FilterWidth = Min(10.0f, g_FilterWidth + 0.25f);
		else
			g_FilterWidth = Max(1.0f, g_FilterWidth - 0.25f);
		break;
	}
}

//--------------------------------------------------------------------------------------
// Initialise the scene.
//--------------------------------------------------------------------------------------
void InitScene()
{
	// Add a single quad to the scene (exciting eh?)
	g_Scene.m_Quads.push_back(cQuad(
		cQuadVertex(XMFLOAT3(-0.50f, -0.5f, 0.0f),  XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f)),
		cQuadVertex(XMFLOAT3( 0.25f, -0.51f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f)),
		cQuadVertex(XMFLOAT3(-0.75f,  0.75f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f)),
		cQuadVertex(XMFLOAT3( 0.75f,  0.5f, 0.0f),  XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f))));
	//g_Scene.m_Quads.push_back(cQuad(
	//	cQuadVertex(XMFLOAT3(-0.5f, -0.5f, 0.0f),  XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f)),
	//	cQuadVertex(XMFLOAT3( 0.5f, -0.5f, 0.0f),  XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f)),
	//	cQuadVertex(XMFLOAT3(-0.51f,  0.5f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f)),
	//	cQuadVertex(XMFLOAT3( 0.51f,  0.5f, 0.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f))));

	// Set some very basic transforms.
	XMStoreFloat4x4(&g_Scene.m_Transform, XMMatrixTranslation(0.0f, 0.0f, 0.0f));
	XMStoreFloat4x4(&g_Scene.m_PrevTransform, XMMatrixTranslation(0.0f, 0.0f, 0.0f));
	//XMStoreFloat4x4(&g_Scene.m_PrevTransform, XMMatrixTranslation(-0.2f, -0.2f, 0.0f));
}

//--------------------------------------------------------------------------------------
// Render the scene.
//--------------------------------------------------------------------------------------
void Render()
{
	HDC hdc = GetDC(g_hWnd);

	// Clear the "backbuffer"
	ZeroMemory(&g_Buffer[0], g_Buffer.size() * sizeof(DWORD));

	// Construct new rasterizer.
	cSoftwareRasterizer Rasterizer(g_Width, g_Height, g_SuperSampleFactor, g_FilterWidth, &g_Buffer[0]);

	// Time the render call.
	double StartTime = cTiming::Instance().GetSeconds();

	// Render the scene using this rasterizer.
	g_Renderer.Render(&Rasterizer, g_Width, g_Height);

	double RenderTime = cTiming::Instance().GetSeconds() - StartTime;

	// Output render time.
	wchar_t Buffer[256];
	swprintf(Buffer, 1024, L"Render took %.1f seconds.\n", RenderTime);
	OutputDebugString(Buffer);

	// Splat the buffer to the screen.
	const BITMAPINFO bmi = {{sizeof(BITMAPINFOHEADER),g_Width,-(INT)g_Height,1,32,BI_RGB,0,0,0,0,0},{0,0,0,0}};
	StretchDIBits(hdc, 0, 0, g_Width, g_Height, 0, 0, g_Width, g_Height, &g_Buffer[0], &bmi, DIB_RGB_COLORS, SRCCOPY);

	// Draw the HUD.
	DrawHUD(hdc);
}

//--------------------------------------------------------------------------------------
// Draw some bits of text on the screen to show the status of stuff.
//--------------------------------------------------------------------------------------
void DrawHUD(HDC hdc)
{
	HFONT hFont;

	// Retrieve a handle to the variable stock font.
	hFont = (HFONT)GetStockObject(ANSI_VAR_FONT);

	// Select the variable stock font into the specified device context.
	if (HFONT hOldFont = (HFONT)SelectObject(hdc, hFont))
	{
		// Set colours.
		SetTextColor(hdc, RGB(255, 255, 0));
		SetBkMode(hdc, TRANSPARENT);

		const int BufferSize = 1024;
		wchar_t Buffer[BufferSize];
		int NumChars;

		// Output super-sample factor.
		NumChars = swprintf_s(Buffer, BufferSize, L"Supersample factor: %d", g_SuperSampleFactor);
		if (NumChars > 0)
			TextOut(hdc, 10, 10, Buffer, NumChars);

		// Output micropolygon size.
		NumChars = swprintf_s(Buffer, BufferSize, L"Micropolygon size: %.1f", g_Renderer.GetMicropolygonSize());
		if (NumChars > 0)
			TextOut(hdc, 10, 30, Buffer, NumChars);

		// Output filter width.
		NumChars = swprintf_s(Buffer, BufferSize, L"Filter width: %.2f", g_FilterWidth);
		if (NumChars > 0)
			TextOut(hdc, 10, 50, Buffer, NumChars);

		// Restore the original font.
		SelectObject(hdc, hOldFont);
	}
}
