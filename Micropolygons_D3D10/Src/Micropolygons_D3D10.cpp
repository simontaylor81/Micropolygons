//--------------------------------------------------------------------------------------
// File: Micropolygons_D3D10.cpp
//
// Direct3D 10 micropolygon renderer front end.
//
// Copyright 2009 (c) Simon Taylor. All rights reserved.
//--------------------------------------------------------------------------------------

#include "stdafx.h"
#include "resource.h"

#include "cScene.h"
#include "cSceneRenderer.h"
#include "cD3D10Rasterizer.h"
#include "Utility.h"

#include <vector>

using namespace MicropolygonCommon;

const INT InitialWidth = 640;
const INT InitialHeight = 480;

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE   g_hInst = NULL;
HWND        g_hWnd = NULL;

// Window dimensions.
UINT	g_Width;
UINT	g_Height;

// The scene and renderer.
cScene			g_Scene;
cSceneRenderer	g_Renderer(&g_Scene);

// Render parameters.
UINT	g_MultiSampleFactor = 1;
float	g_FilterWidth = 3.0f;

// D3D globals.
IDXGISwapChain*			g_pSwapChain = NULL;
ID3D10Device*			g_pd3dDevice = NULL;
ID3D10RenderTargetView*	g_pRenderTargetView = NULL;

// The rasterzier.
cD3D10Rasterizer		g_Rasterizer;

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow );
LRESULT CALLBACK    WndProc( HWND, UINT, WPARAM, LPARAM );
void Resize(UINT Width, UINT Height);
void OnKeyboard(WPARAM wParam, LPARAM lParam);
BOOL InitD3D();
void CleanupD3D();
void InitScene();
void Render();
void DrawHUD();


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPWSTR /*lpCmdLine*/, int nCmdShow )
{
    if( FAILED( InitWindow( hInstance, nCmdShow ) ) )
        return 0;

	if (!InitD3D())
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
    wcex.hIcon = LoadIcon( hInstance, ( LPCTSTR )IDI_MICROPOLYGONS_D3D10 );
    wcex.hCursor = LoadCursor( NULL, IDC_ARROW );
    wcex.hbrBackground = ( HBRUSH )( COLOR_WINDOW + 1 );
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = L"TutorialWindowClass";
    wcex.hIconSm = LoadIcon( wcex.hInstance, ( LPCTSTR )IDI_SMALL);
    if( !RegisterClassEx( &wcex ) )
        return E_FAIL;

    // Create window
    g_hInst = hInstance;
    RECT rc = { 0, 0, InitialWidth, InitialHeight };
    AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );
    g_hWnd = CreateWindow( L"TutorialWindowClass", L"Direct3D 10 Micropolygon Renderer", WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance,
                           NULL );
    if( !g_hWnd )
        return E_FAIL;

    ShowWindow( g_hWnd, nCmdShow );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Initialise Direct3D.
//--------------------------------------------------------------------------------------
BOOL InitD3D()
{
	HRESULT hr;

    RECT rc;
    GetClientRect( g_hWnd, &rc );
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory( &sd, sizeof(sd) );
	sd.BufferCount = 1;
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = g_hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	if( FAILED( D3D10CreateDeviceAndSwapChain( NULL, D3D10_DRIVER_TYPE_REFERENCE, NULL,
					 0, D3D10_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice ) ) )
	{
		return FALSE;
	}

	// Create a render target view
	ID3D10Texture2D *pBackBuffer;
	if( FAILED( g_pSwapChain->GetBuffer( 0, __uuidof( ID3D10Texture2D ), (LPVOID*)&pBackBuffer ) ) )
		return FALSE;
	hr = g_pd3dDevice->CreateRenderTargetView( pBackBuffer, NULL, &g_pRenderTargetView );
	pBackBuffer->Release();
	if( FAILED( hr ) )
		return FALSE;

	// Set as render target.
	g_pd3dDevice->OMSetRenderTargets( 1, &g_pRenderTargetView, NULL );

	// Set viewport.
	D3D10_VIEWPORT vp;
	vp.Width = width;
	vp.Height = height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_pd3dDevice->RSSetViewports( 1, &vp );

	// Initialise rasterizer;
	if (!g_Rasterizer.Init(g_pd3dDevice))
	{
		return FALSE;
	}

	return TRUE;
}

//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void CleanupDevice()
{
	g_Rasterizer.Release();

    if( g_pd3dDevice ) g_pd3dDevice->ClearState();

    if( g_pRenderTargetView ) g_pRenderTargetView->Release();
    if( g_pSwapChain ) g_pSwapChain->Release();
    if( g_pd3dDevice ) g_pd3dDevice->Release();
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
			g_MultiSampleFactor = Min(16u, g_MultiSampleFactor << 1);
		else
			g_MultiSampleFactor = Max(1u, g_MultiSampleFactor >> 1);
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
		cQuadVertex(XMFLOAT3(-0.50f, -0.5f, 0.0f), D3DXCOLOR(0xFFFF0000)),
		cQuadVertex(XMFLOAT3( 0.25f, -0.51f, 0.0f), D3DXCOLOR(0xFF00FF00)),
		cQuadVertex(XMFLOAT3(-0.75f,  0.75f, 0.0f), D3DXCOLOR(0xFF0000FF)),
		cQuadVertex(XMFLOAT3( 0.75f,  0.5f, 0.0f), D3DXCOLOR(0xFFFFFF00))));
	//g_Scene.m_Quads.push_back(cQuad(
	//	cQuadVertex(XMFLOAT3(-0.5f, -0.5f, 0.0f), D3DXCOLOR(0xFFFF0000)),
	//	cQuadVertex(XMFLOAT3( 0.5f, -0.5f, 0.0f), D3DXCOLOR(0xFF00FF00)),
	//	cQuadVertex(XMFLOAT3(-0.51f,  0.5f, 0.0f), D3DXCOLOR(0xFF0000FF)),
	//	cQuadVertex(XMFLOAT3( 0.51f,  0.5f, 0.0f), D3DXCOLOR(0xFFFFFF00))));

	// Set some very basic transforms.
	D3DXMatrixTranslation(&g_Scene.m_Transform, 0.0f, 0.0f, 0.0f);
	D3DXMatrixTranslation(&g_Scene.m_PrevTransform, 0.0f, 0.0f, 0.0f);
	//D3DXMatrixTranslation(&g_Scene.m_PrevTransform, -0.2f, -0.2f, 0.0f);
}

//--------------------------------------------------------------------------------------
// Render the scene.
//--------------------------------------------------------------------------------------
void Render()
{
	//
    // Clear the backbuffer
    //
    float ClearColor[4] = { 0.0f, 0.125f, 0.6f, 1.0f }; // RGBA
    g_pd3dDevice->ClearRenderTargetView(g_pRenderTargetView, ClearColor);

	// Time the render call.
	double StartTime = cTiming::Instance().GetSeconds();

	// Render the scene using this rasterizer.
	g_Renderer.Render(&g_Rasterizer, g_Width, g_Height);

	double RenderTime = cTiming::Instance().GetSeconds() - StartTime;

	// Output render time.
	wchar_t Buffer[256];
	swprintf(Buffer, 1024, L"Render took %.1f seconds.\n", RenderTime);
	OutputDebugString(Buffer);

	// Draw the HUD.
	DrawHUD();

    g_pSwapChain->Present(0, 0);
}

//--------------------------------------------------------------------------------------
// Draw some bits of text on the screen to show the status of stuff.
//--------------------------------------------------------------------------------------
void DrawHUD()
{
	/*
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
		NumChars = swprintf_s(Buffer, BufferSize, L"Supersample factor: %d", g_MultiSampleFactor);
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
	*/
}
