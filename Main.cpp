#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

//--------------------------------------------------------------------------------------
// Using namespaces
//--------------------------------------------------------------------------------------
using namespace DirectX;

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct SimpleVertex
{
	XMFLOAT3 Pos;
};

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE g_hInstance;
HWND g_hWnd;
D3D_DRIVER_TYPE g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device* g_pDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenterTargetView = nullptr;
ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11InputLayout* g_pVertexLayout = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11Buffer* g_pVertexBuffer = nullptr;


//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);
HRESULT InitDevice();
void CleanupDevice();
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void Render();

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	if (FAILED(InitWindow(hInstance, nCmdShow)))
	{
		return 0;
	}

	if (FAILED(InitDevice()))
	{
		CleanupDevice();
		return 0;
	}

	MSG msg = { nullptr };
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			Render();
		}
	}

	CleanupDevice();

	return static_cast<int>(msg.wParam);
}

HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow)
{
	// Register Window class
	WNDCLASSEX wcex;
	ZeroMemory(&wcex, sizeof(wcex));

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = reinterpret_cast<HBRUSH>((COLOR_WINDOW + 1));
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = L"TutorialWindowClass";
	wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

	if (!RegisterClassEx(&wcex))
	{
		return E_FAIL;
	}

	g_hInstance = hInstance;
	RECT rc = { 0, 0, 640, 480 };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	g_hWnd = CreateWindowEx(NULL,
		L"TutorialWindowClass",
		L"Direct3D 11 Tutorial 1: Direct3D 11 Basics",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rc.right - rc.left,
		rc.bottom - rc.top,
		nullptr,
		nullptr,
		hInstance,
		nullptr);

	if (g_hWnd == nullptr)
	{
		return E_FAIL;
	}

	ShowWindow(g_hWnd, nCmdShow);

	return S_OK;
}

HRESULT CompileShaderFromFile(LPCTSTR szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;

	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	ID3DBlob* pErrorBlob;
	hr = D3DCompileFromFile(
		szFileName,						// 파일명
		nullptr,						// 매크로 정의
		nullptr,						// Include 파일 정의
		szEntryPoint,					// 셰이더 Entry point 이름(메인 함수 이름)
		szShaderModel,					// 셰이더 컴파일 버전(vs_4_0 = Vertex shader version 4.0)
		dwShaderFlags,					// 컴파일 옵션
		0,								// 이펙트 컴파일 옵션
		ppBlobOut,						// 컴파일 된 Byte코드
		&pErrorBlob						// 에러 메세지
	);
	if (FAILED(hr))
	{
		if (pErrorBlob != nullptr)
		{
			OutputDebugStringA(static_cast<LPCSTR>(pErrorBlob->GetBufferPointer()));
		}
		else
		{
			pErrorBlob->Release();
		}
		return hr;
	}

	if (pErrorBlob)
	{
		pErrorBlob->Release();
	}

	return S_OK;
}

HRESULT InitDevice()
{
	HRESULT hr = S_OK;

	RECT rc;
	GetClientRect(g_hWnd, &rc);
	const UINT width = rc.right - rc.left;
	const UINT height = rc.bottom - rc.top;

	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_DRIVER_TYPE driverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	constexpr UINT numDriverTypes = ARRAYSIZE(driverTypes);

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	constexpr UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(DXGI_SWAP_CHAIN_DESC));
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

	for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
	{
		g_driverType = driverTypes[driverTypeIndex];
		hr = D3D11CreateDeviceAndSwapChain(
			nullptr,						// device를 생성하기 위한 video adapter(nullptr = default adapter)
			g_driverType,					// driver type
			nullptr,						// software handle
			createDeviceFlags,				// device flag
			featureLevels,					// feature level array
			numFeatureLevels,				// feature level array length
			D3D11_SDK_VERSION,				// sdk version
			&sd,							// swap chain structure
			&g_pSwapChain,					// 생성된 swap chain 
			&g_pDevice,						// 생성된 device
			&g_featureLevel,				// 생성된 feature level
			&g_pImmediateContext);			// 생성된 device context

		if (SUCCEEDED(hr))
		{
			break;
		}
	}

	if (FAILED(hr))
	{
		return hr;
	}

	ID3D11Texture2D* pBackBuffer = nullptr;
	hr = g_pSwapChain->GetBuffer(
		0,													// 백버퍼 index
		__uuidof(ID3D11Texture2D),							// back buffer를 다루기 위한 interface
		reinterpret_cast<LPVOID*>(&pBackBuffer)				// back buffer interface output
	);
	if (FAILED(hr))
	{
		return hr;
	}

	hr = g_pDevice->CreateRenderTargetView(
		pBackBuffer,			// View에서 접근할 리소스
		nullptr,				// RTV 정의
		&g_pRenterTargetView);	// RTV를 받아올 변수
	// 사용한 back buffer를 Release 
	pBackBuffer->Release();
	if (FAILED(hr))
	{
		return hr;
	}

	g_pImmediateContext->OMSetRenderTargets(
		1,						// Render target의 개수(최대 8개)
		&g_pRenterTargetView,	// Render target view의 배열
		nullptr					// Depth stencil view 포인터
	);

	D3D11_VIEWPORT vp;
	vp.Width = static_cast<FLOAT>(width);
	vp.Height = static_cast<FLOAT>(height);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_pImmediateContext->RSSetViewports(1, &vp);

	// compile the vertex shader
	ID3DBlob* pVSBlob = nullptr;
	hr = CompileShaderFromFile(L"Tutorial02.fx", "VS", "vs_4_0", &pVSBlob);
	if (FAILED(hr))
	{
		MessageBox(nullptr, L"The FX file cannot be compiled. Please run this executable from the directory that contains the FX file.",
			L"Error", MB_OK);
		return hr;
	}

	// create the vertex shader
	hr = g_pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pVertexShader);
	if (FAILED(hr))
	{
		pVSBlob->Release();
		return hr;
	}

	// define the input layout (정점 데이터를 GPU에게 알려주는 구조체)
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{"POSITION",					// Element의 목적이 무엇인이 알려주는 문자열 (POSITION, NORMAL, ...)
		0,								// 동일한 Element의 목적을 가진 정점의 Index
		DXGI_FORMAT_R32G32B32_FLOAT,	// 자료형
		0,								// 정점 버퍼 슬롯 index
		0,								// 정점 버퍼 offset
		D3D11_INPUT_PER_VERTEX_DATA,	// D3D11_INPUT_PER_VERTEX_DATA를 사용함.
		0								// Instancing에 사용됨
		},
	};
	constexpr UINT numElements = ARRAYSIZE(layout);

	// create the input layout
	hr = g_pDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &g_pVertexLayout);
	pVSBlob->Release();
	if (FAILED(hr))
	{
		return hr;
	}

	// set the input layout
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout);

	// compile the pixel shader
	ID3DBlob* pPSBlob = nullptr;
	hr = CompileShaderFromFile(L"Tutorial02.fx", "PS", "ps_4_0", &pPSBlob);
	if (FAILED(hr))
	{
		MessageBox(nullptr, L"The FX file cannot be compiled. Please run this executable from the directory that contains the FX file.",
			L"Error", MB_OK);
		return hr;
	}

	// create the pixel shader
	hr = g_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader);
	pPSBlob->Release();
	if (FAILED(hr))
	{
		return hr;
	}

	// create vertex buffer
	constexpr SimpleVertex vertices[] =
	{
		XMFLOAT3(0.0f, 0.5f, 0.5f),
		XMFLOAT3(0.5f, -0.5f, 0.5f),
		XMFLOAT3(-0.5f, -0.5f, 0.5f),
	};
	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(D3D11_BUFFER_DESC));
	bd.Usage = D3D11_USAGE_DEFAULT;				// default로 사용
	bd.ByteWidth = sizeof(SimpleVertex) * 3;	// vertex 3개
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;	// vertex buffer로 바인드
	bd.CPUAccessFlags = 0;						// cpu access하지 않음.

	D3D11_SUBRESOURCE_DATA initData;
	ZeroMemory(&initData, sizeof(D3D11_SUBRESOURCE_DATA));
	initData.pSysMem = vertices;	// 버퍼 데티
	hr = g_pDevice->CreateBuffer(&bd, &initData, &g_pVertexBuffer);
	if (FAILED(hr))
	{
		return hr;
	}

	// set vertex buffer
	UINT stride = sizeof(SimpleVertex);
	UINT offset = 0;
	g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	// set primitive topology
	g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	return S_OK;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (uMsg)
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	return 0;
}

void Render()
{
	constexpr float clearColor[4] = { 0.0f, 0.125f,0.3f,1.0f };
	g_pImmediateContext->ClearRenderTargetView(g_pRenterTargetView, clearColor);

	g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
	g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);
	g_pImmediateContext->Draw(3, 0);

	g_pSwapChain->Present(0, 0);
}

void CleanupDevice()
{
	if (g_pImmediateContext)
	{
		g_pImmediateContext->ClearState();
	}

	if (g_pVertexBuffer)
	{
		g_pVertexBuffer->Release();
	}
	if (g_pVertexLayout)
	{
		g_pVertexLayout->Release();
	}
	if (g_pVertexShader)
	{
		g_pVertexShader->Release();
	}
	if (g_pPixelShader)
	{
		g_pPixelShader->Release();
	}
	if (g_pRenterTargetView)
	{
		g_pRenterTargetView->Release();
	}
	if (g_pSwapChain)
	{
		g_pSwapChain->Release();
	}
	if (g_pImmediateContext)
	{
		g_pImmediateContext->Release();
	}
	if (g_pDevice)
	{
		g_pDevice->Release();
	}
}