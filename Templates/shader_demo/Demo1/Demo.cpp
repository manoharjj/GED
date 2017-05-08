#include <windows.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dcompiler.h>
#include <xnamath.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdint>
#include "resource.h"


#include "DXUT/Core/dxut.h"
#include "DXUTmisc.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"

#include "Effects11/Inc/d3dx11effect.h"
#include "debug.h"

void FillTechniques();

// Convenience macros for safe effect variable retrieval
#define SAFE_GET_PASS(Technique, name, var)   {assert(Technique!=NULL); var = Technique->GetPassByName( name );						assert(var->IsValid());}
#define SAFE_GET_TECHNIQUE(effect, name, var) {assert(effect!=NULL); var = effect->GetTechniqueByName( name );						assert(var->IsValid());}
#define SAFE_GET_SCALAR(effect, name, var)    {assert(effect!=NULL); var = effect->GetVariableByName( name )->AsScalar();			assert(var->IsValid());}
#define SAFE_GET_VECTOR(effect, name, var)    {assert(effect!=NULL); var = effect->GetVariableByName( name )->AsVector();			assert(var->IsValid());}
#define SAFE_GET_MATRIX(effect, name, var)    {assert(effect!=NULL); var = effect->GetVariableByName( name )->AsMatrix();			assert(var->IsValid());}
#define SAFE_GET_SAMPLER(effect, name, var)   {assert(effect!=NULL); var = effect->GetVariableByName( name )->AsSampler();			assert(var->IsValid());}
#define SAFE_GET_RESOURCE(effect, name, var)  {assert(effect!=NULL); var = effect->GetVariableByName( name )->AsShaderResource();	assert(var->IsValid());}


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           2
#define IDC_CHANGEDEVICE        3
#define IDC_TECHNIQUE			4
#define IDC_RELOAD_SHADERS		101


// User Interface
CDXUTDialogResourceManager              g_dialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg                         g_settingsDlg;          // Device settings dialog
CDXUTTextHelper*                        g_txtHelper = NULL;
CDXUTDialog                             g_hud;                  // dialog for standard controls
CDXUTDialog                             g_sampleUI;             // dialog for sample specific controls


//--------------------------------------------------------------------------------------
// Effect
//--------------------------------------------------------------------------------------

struct MyEffect
{
	// A D3DX rendering effect
	ID3DX11Effect*                          effect; // The whole rendering effect
	ID3DX11EffectTechnique*                 technique; // One technique to render the effect
	ID3DX11EffectMatrixVariable*            worldViewProjectionEV; // WorldViewProjection matrix effect variable
	ID3DX11EffectScalarVariable*			frameIndex;

	MyEffect() { ZeroMemory(this, sizeof(*this)); }		// WARNING: This will set ALL members to 0!


	HRESULT create(ID3D11Device* device)
	{
		HRESULT hr;
		WCHAR path[MAX_PATH];

		// Find and load the rendering effect
		V_RETURN(DXUTFindDXSDKMediaFileCch(path, MAX_PATH, L"demoEffect.fxo"));
		std::ifstream is(path, std::ios_base::binary);
		is.seekg(0, std::ios_base::end);
		std::streampos pos = is.tellg();
		is.seekg(0, std::ios_base::beg);
		std::vector<char> effectBuffer((unsigned int)pos);
		is.read(&effectBuffer[0], pos);
		is.close();
		V_RETURN(D3DX11CreateEffectFromMemory((const void*)&effectBuffer[0], effectBuffer.size(), 0, device, &effect));    
		assert(effect->IsValid());

		// Obtain the effect technique
		SAFE_GET_TECHNIQUE(effect, "Render", technique);

		// Obtain the effect variables
		SAFE_GET_MATRIX(effect, "g_WorldViewProjection", worldViewProjectionEV);   
		SAFE_GET_SCALAR(effect, "g_FrameIndex", frameIndex);

		FillTechniques();

		return S_OK;
	}


	void destroy()
	{
		SAFE_RELEASE(effect);
	}
};


// Help macros
#define DEG2RAD( a ) ( (a) * D3DX_PI / 180.f )

using namespace std;

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------

// Camera
struct CAMERAPARAMS {
	float   fovy;
	float   aspect;
	float   nearPlane;
	float   farPlane;
}                                       g_cameraParams;
float                                   g_cameraMoveScaler = 1.0f;
float                                   g_cameraRotateScaler = 0.01f;
CFirstPersonCamera                      g_camera;               // A first person camera

MyEffect								g_myEffect; // CPU part of Shader



//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );

bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *, UINT , const CD3D11EnumDeviceInfo *,
                                       DXGI_FORMAT, bool, void* );
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext );
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                         const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D11DestroyDevice( void* pUserContext );
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                 float fElapsedTime, void* pUserContext );

void InitApp();
void RenderText();

void ReleaseShader();
HRESULT ReloadShader(ID3D11Device* pd3dDevice);

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
	UNREFERENCED_PARAMETER(hInstance);
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

	std::stringstream s;
	s << "Hallo!";
	std::string test;

	s >> test;
	std::cout << test;

    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // Old Direct3D Documentation:
    // Start > All Programs > Microsoft DirectX SDK (June 2010) > Windows DirectX Graphics Documentation

    // DXUT Documentaion:
    // Start > All Programs > Microsoft DirectX SDK (June 2010) > DirectX Documentation for C++ : The DirectX Software Development Kit > Programming Guide > DXUT
	
    // New Direct3D Documentaion (just for reference, use old documentation to find explanations):
    // http://msdn.microsoft.com/en-us/library/windows/desktop/hh309466%28v=vs.85%29.aspx

    // Set DXUT callbacks
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );

    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );

    InitApp();
    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true );
    DXUTCreateWindow( L"TODO: Insert Title Here" ); // You may change the title

    DXUTCreateDevice( D3D_FEATURE_LEVEL_10_0, true, 1280, 720 );

    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}

//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    HRESULT hr;
 
    // Intialize the user interface

    g_settingsDlg.Init( &g_dialogResourceManager );
    g_hud.Init( &g_dialogResourceManager );
    g_sampleUI.Init( &g_dialogResourceManager );

    g_hud.SetCallback( OnGUIEvent );
    int iY = 30;
    int iYo = 26;
    g_hud.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 170, 22 );
    g_hud.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += iYo, 170, 22, VK_F3 );
    g_hud.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += iYo, 170, 22, VK_F2 );

	g_hud.AddButton (IDC_RELOAD_SHADERS, L"Reload shaders (F5)", 0, iY += 24, 170, 22, VK_F5);
    
    g_sampleUI.SetCallback( OnGUIEvent ); iY = 10;
    iY += 24;
	g_sampleUI.AddComboBox(IDC_TECHNIQUE, 0, iY += 24, 125, 22);
}

//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for 
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_txtHelper->Begin();
    g_txtHelper->SetInsertionPos( 5, 5 );
    g_txtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_txtHelper->DrawTextLine( DXUTGetFrameStats(true)); //DXUTIsVsyncEnabled() ) );
    g_txtHelper->DrawTextLine( DXUTGetDeviceStats() );
    g_txtHelper->End();
}

//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *, UINT, const CD3D11EnumDeviceInfo *,
        DXGI_FORMAT, bool, void* )
{
    return true;
}

//--------------------------------------------------------------------------------------
// Specify the initial device settings
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
	UNREFERENCED_PARAMETER(pDeviceSettings);
	UNREFERENCED_PARAMETER(pUserContext);

    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( ( DXUT_D3D9_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF ) ||
            ( DXUT_D3D11_DEVICE == pDeviceSettings->ver &&
            pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE ) )
        {
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
        }

    }
    //// Enable anti aliasing
    //pDeviceSettings->d3d11.sd.SampleDesc.Count = 4;
    //pDeviceSettings->d3d11.sd.SampleDesc.Quality = 1;

    return true;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice,
        const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
	UNREFERENCED_PARAMETER(pBackBufferSurfaceDesc);
	UNREFERENCED_PARAMETER(pUserContext);

    HRESULT hr;

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext(); // http://msdn.microsoft.com/en-us/library/ff476891%28v=vs.85%29
    V_RETURN( g_dialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_settingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_txtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_dialogResourceManager, 15 );

    V_RETURN( ReloadShader(pd3dDevice) );
    
    
    // Initialize the camera
	D3DXVECTOR3 Eye( 0.0f, 2.0f, -2.0); // Camera eye is here
    D3DXVECTOR3 At( 0.0f, 0.0f, 0.0f );      // ... facing at this position
    g_camera.SetViewParams( &Eye, &At ); // http://msdn.microsoft.com/en-us/library/windows/desktop/bb206342%28v=vs.85%29.aspx
	g_camera.SetScalers( g_cameraRotateScaler, g_cameraMoveScaler );

    return S_OK;
}


void FillTechniques()
{
	// Fill technique combo box
	auto box = g_sampleUI.GetComboBox(IDC_TECHNIQUE);

	if ((box != nullptr) && (g_myEffect.technique->IsValid()))
	{
		box->RemoveAllItems();
		D3DX11_TECHNIQUE_DESC desc;
		g_myEffect.technique->GetDesc(&desc);
		for (int i = 0; i < desc.Passes; ++i)
		{
			D3DX11_PASS_DESC passDesc;
			g_myEffect.technique->GetPassByIndex(i)->GetDesc(&passDesc);
			std::string s(passDesc.Name);
			std::wstring ws(s.begin(), s.end());

			box->AddItem(ws.c_str(), nullptr);
		}

		box->SetSelectedByIndex(0);
	}
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
	UNREFERENCED_PARAMETER(pUserContext);

    g_dialogResourceManager.OnD3D11DestroyDevice();
    g_settingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();

    SAFE_DELETE( g_txtHelper );
    ReleaseShader();
}

//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
        const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
	UNREFERENCED_PARAMETER(pSwapChain);
	UNREFERENCED_PARAMETER(pUserContext);

    HRESULT hr;
    
    // Intialize the user interface

    V_RETURN( g_dialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_settingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    g_hud.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_hud.SetSize( 170, 170 );
    g_sampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_sampleUI.SetSize( 170, 300 );

    // Initialize the camera

    g_cameraParams.aspect = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_cameraParams.fovy = 0.785398;
    g_cameraParams.nearPlane = 0.1f;
    g_cameraParams.farPlane = 100.0f;

    g_camera.SetProjParams(g_cameraParams.fovy, g_cameraParams.aspect, g_cameraParams.nearPlane, g_cameraParams.farPlane);
	g_camera.SetEnablePositionMovement(true);
	g_camera.SetRotateButtons(true, false, false);
	g_camera.SetScalers( g_cameraRotateScaler, g_cameraMoveScaler );
	g_camera.SetDrag( true );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
	UNREFERENCED_PARAMETER(pUserContext);
    g_dialogResourceManager.OnD3D11ReleasingSwapChain();
}

//--------------------------------------------------------------------------------------
// Loads the effect from file
// and retrieves all dependent variables
//--------------------------------------------------------------------------------------
HRESULT ReloadShader(ID3D11Device* pd3dDevice)
{
    assert(pd3dDevice != NULL);

    HRESULT hr;

    ReleaseShader();
	V_RETURN(g_myEffect.create(pd3dDevice));

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Release resources created in ReloadShader
//--------------------------------------------------------------------------------------
void ReleaseShader()
{
	g_myEffect.destroy();
}

//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
	UNREFERENCED_PARAMETER(pUserContext);

    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_dialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to settings dialog if its active
    if( g_settingsDlg.IsActive() )
    {
        g_settingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_hud.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    *pbNoFurtherProcessing = g_sampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
        
    // Use the mouse weel to control the movement speed
    if(uMsg == WM_MOUSEWHEEL) {
        int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        g_cameraMoveScaler *= (1 + zDelta / 500.0f);
        if (g_cameraMoveScaler < 0.1f)
          g_cameraMoveScaler = 0.1f;
        g_camera.SetScalers(g_cameraRotateScaler, g_cameraMoveScaler);
    }

    // Pass all remaining windows messages to camera so it can respond to user input
    g_camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}

//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
	UNREFERENCED_PARAMETER(nChar);
	UNREFERENCED_PARAMETER(bKeyDown);
	UNREFERENCED_PARAMETER(bAltDown);
	UNREFERENCED_PARAMETER(pUserContext);
}

//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
	UNREFERENCED_PARAMETER(nEvent);
	UNREFERENCED_PARAMETER(pControl);
	UNREFERENCED_PARAMETER(pUserContext);

    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen(); break;
        case IDC_TOGGLEREF:
            DXUTToggleREF(); break;
        case IDC_CHANGEDEVICE:
            g_settingsDlg.SetActive( !g_settingsDlg.IsActive() ); break;
		case IDC_RELOAD_SHADERS:
			ReloadShader(DXUTGetD3D11Device ());
			break;
    }
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
	UNREFERENCED_PARAMETER(pUserContext);
    // Update the camera's position based on user input 
    g_camera.FrameMove( fElapsedTime );
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
        float fElapsedTime, void* pUserContext )
{
	UNREFERENCED_PARAMETER(pd3dDevice);
	UNREFERENCED_PARAMETER(fTime);
	UNREFERENCED_PARAMETER(pUserContext);

    HRESULT hr;

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_settingsDlg.IsActive() )
    {
        g_settingsDlg.OnRender( fElapsedTime );
        return;
    }     

    ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
	float clearColor[4] = {0.f, 0.f, 0.f, 1.0f};
    pd3dImmediateContext->ClearRenderTargetView( pRTV, clearColor );
        
	if(g_myEffect.effect == NULL) {
        g_txtHelper->Begin();
        g_txtHelper->SetInsertionPos( 5, 5 );
        g_txtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
        g_txtHelper->DrawTextLine( L"SHADER ERROR" );
        g_txtHelper->End();
        return;
    }

    // Clear the depth stencil
    ID3D11DepthStencilView* pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );
    
    // Update variables that change once per frame
    D3DXMATRIX const * view = g_camera.GetViewMatrix(); // http://msdn.microsoft.com/en-us/library/windows/desktop/bb206342%28v=vs.85%29.aspx
    D3DXMATRIX const * proj = g_camera.GetProjMatrix(); // http://msdn.microsoft.com/en-us/library/windows/desktop/bb147302%28v=vs.85%29.aspx
    D3DXMATRIX worldViewProj = (*view) * (*proj);
	static int currentFrameIndex = 0;
	V(g_myEffect.worldViewProjectionEV->SetMatrix( ( float* )&worldViewProj ));
	V(g_myEffect.frameIndex->SetInt(currentFrameIndex++));

    // Set input layout
    pd3dImmediateContext->IASetInputLayout(nullptr);

	auto box = g_sampleUI.GetComboBox(IDC_TECHNIQUE);
	g_myEffect.technique->GetPassByIndex(box->GetSelectedIndex())->Apply(0, pd3dImmediateContext);
	pd3dImmediateContext->Draw(3, 0);
    
    DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
    V(g_hud.OnRender( fElapsedTime ));
    V(g_sampleUI.OnRender( fElapsedTime ));
    RenderText();
    DXUT_EndPerfEvent();

    static DWORD dwTimefirst = GetTickCount();
    if ( GetTickCount() - dwTimefirst > 5000 )
    {    
        OutputDebugString( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
        OutputDebugString( L"\n" );
        dwTimefirst = GetTickCount();
    }
}
