// Global objects (mainly D3D stuff).

#pragma once

// D3D globals.
extern IDXGISwapChain*			g_pSwapChain;
extern ID3D10Device*			g_pd3dDevice;
extern ID3D10RenderTargetView*	g_pRenderTargetView;

// Window dimensions.
extern UINT	g_Width;
extern UINT	g_Height;
extern UINT	g_MultiSampleFactor;
