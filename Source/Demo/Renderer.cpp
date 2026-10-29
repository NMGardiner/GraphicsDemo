#include "Demo/Renderer.hpp"

// CoreLib
#include "CoreLib/Logging.hpp"
#include "CoreLib/UtilMacros.hpp"

// D3D12
#define _ATL_DEBUG_INTERFACES
#include <d3d12.h>
#include <dxgi1_6.h>

// D3D12MemoryAllocator
#include "D3D12MemAlloc.h"

// Dear ImGui
#include "imgui.h"

// SDL
#include "SDL3/SDL_properties.h"
#include "SDL3/SDL_video.h"

// System Headers
#include <array>
#include <cstdint>
#include <fstream>
#include <numbers>
#include <thread>
#include <vector>



namespace Demo
{

Renderer::Renderer(SDL_Window* pWindow, SDL_PropertiesID windowProperties)
	: m_pDebug(nullptr)
	, m_pFactory(nullptr)
	, m_pDevice(nullptr)
	, m_pAllocator(nullptr)
	, m_pCommandQueue(nullptr)
	, m_pSwapchain(nullptr)
	, m_pRTVHeap(nullptr)
	, m_swapchainRTVs({})
	, m_swapchainRTVDescriptors({})
	, m_frames({})
	, m_pFence(nullptr)
	, m_pSRVHeap(nullptr)
	, m_pImGuiFontTexture(nullptr)
	, m_imGuiFrames({})
	, m_pImGuiRootSignature(nullptr)
	, m_pImGuiPipelineState(nullptr)
	, m_pSceneTexture(nullptr)
	, m_SceneTextureDescriptor({})
	, m_frameNumber(0)
{
	int windowWidth = 0;
	int windowHeight = 0;
	bool queryResult = SDL_GetWindowSizeInPixels(
		pWindow,
		&windowWidth,
		&windowHeight);
	CORE_ASSERT(queryResult);

	HRESULT result = S_OK;

	// TODO: Do we want any later interfaces? ID3D12Debug3 may be useful.
	{
		ID3D12Debug* pDebugBase = nullptr;
		result = D3D12GetDebugInterface(IID_PPV_ARGS(&pDebugBase));
		CORE_ASSERT(SUCCEEDED(result));

		result = pDebugBase->QueryInterface(IID_PPV_ARGS(&m_pDebug));
		CORE_ASSERT(SUCCEEDED(result));

		pDebugBase->Release();
	}

	m_pDebug->EnableDebugLayer();
	m_pDebug->SetEnableGPUBasedValidation(true);

	result = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&m_pFactory));
	CORE_ASSERT(SUCCEEDED(result));

	IDXGIAdapter1* pAdapter = nullptr;
	DXGI_ADAPTER_DESC1 adapterDesc = {};
	for (uint32_t i = 0u; m_pFactory->EnumAdapters1(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; i++)
	{
		result = pAdapter->GetDesc1(&adapterDesc);
		CORE_ASSERT(SUCCEEDED(result));

		if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			pAdapter->Release();
			pAdapter = nullptr;

			continue;
		}

		// Pick the first D3D12-capable hardware adapter.
		// TODO: Use IDXGIFactory6::EnumAdapterByGpuPreference() for this.
		result = D3D12CreateDevice(
			pAdapter,
			D3D_FEATURE_LEVEL_12_0,
			__uuidof(ID3D12Device),
			nullptr);

		if (SUCCEEDED(result))
		{
			break;
		}
		else
		{
			pAdapter->Release();
			pAdapter = nullptr;
		}
	}

	Core::LogInfo(
		L"Selected DXGI Adapter:\n"
		"\tDescription {}\n"
		"\tVendorId {}\n"
		"\tDeviceId {}\n"
		"\tSubSysId {}\n"
		"\tRevision {}\n"
		"\tDedicatedVideoMemory {}\n"
		"\tDedicatedSystemMemory {}\n"
		"\tSharedSystemMemory {}\n"
		"\tAdapterLuid {}\n"
		"\tFlags {}",
		adapterDesc.Description,
		adapterDesc.VendorId,
		adapterDesc.DeviceId,
		adapterDesc.SubSysId,
		adapterDesc.Revision,
		adapterDesc.DedicatedVideoMemory,
		adapterDesc.DedicatedSystemMemory,
		adapterDesc.SharedSystemMemory,
		adapterDesc.AdapterLuid.LowPart,
		adapterDesc.Flags);

	result = D3D12CreateDevice(
		pAdapter,
		D3D_FEATURE_LEVEL_12_0,
		IID_PPV_ARGS(&m_pDevice));
	CORE_ASSERT(SUCCEEDED(result));

	const D3D12MA::ALLOCATOR_DESC allocatorDesc
	{
		.Flags = D3D12MA_RECOMMENDED_ALLOCATOR_FLAGS,
		.pDevice = m_pDevice,
		.PreferredBlockSize = 0,
		.pAllocationCallbacks = nullptr,
		.pAdapter = pAdapter
	};

	result = D3D12MA::CreateAllocator(&allocatorDesc, &m_pAllocator);
	CORE_ASSERT(SUCCEEDED(result));

	pAdapter->Release();

	const D3D12_COMMAND_QUEUE_DESC commandQueueDesc
	{
		.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
		.Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH,
		.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
		.NodeMask = 0u
	};

	result = m_pDevice->CreateCommandQueue(
		&commandQueueDesc,
		IID_PPV_ARGS(&m_pCommandQueue));
	CORE_ASSERT(SUCCEEDED(result));

	const DXGI_SWAP_CHAIN_DESC1 swapchainDesc
	{
		.Width = static_cast<UINT>(windowWidth),
		.Height = static_cast<UINT>(windowHeight),
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.Stereo = FALSE,
		.SampleDesc = { 1u, 0u },
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = static_cast<uint32_t>(kNumSwapchainBuffers),
		.Scaling = DXGI_SCALING_NONE,
		.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
		.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
		// TODO: Confirm support with IDXGIFactory5::CheckFeatureSupport()
		.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
	};

	// Query the HWND from the SDL window handle.
	HWND pWindowHandle = static_cast<HWND>(SDL_GetPointerProperty(
		windowProperties,
		SDL_PROP_WINDOW_WIN32_HWND_POINTER,
		nullptr));
	CORE_ASSERT_NOT_NULL(pWindowHandle);

	{
		IDXGISwapChain1* pSwapchainBase = nullptr;
		result = m_pFactory->CreateSwapChainForHwnd(
			m_pCommandQueue,
			pWindowHandle,
			&swapchainDesc,
			nullptr,
			nullptr,
			&pSwapchainBase);
		CORE_ASSERT(SUCCEEDED(result));

		pSwapchainBase->QueryInterface(IID_PPV_ARGS(&m_pSwapchain));
		CORE_ASSERT(SUCCEEDED(result));

		pSwapchainBase->Release();
	}

	// Allocate space for 64 RTVs, increase size as needed.
	const D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc
	{
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
		.NumDescriptors = 64u,
		.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		.NodeMask = 0u
	};

	result = m_pDevice->CreateDescriptorHeap(
		&rtvHeapDesc,
		IID_PPV_ARGS(&m_pRTVHeap));
	CORE_ASSERT(SUCCEEDED(result));

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHeapNextFree = m_pRTVHeap->GetCPUDescriptorHandleForHeapStart();
	const uint32_t rtvHeapIncrement = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// Create 'kNumSwapchainBuffers' RTVs at the start of the heap.
	// TODO: Do we even need this? This article:
	//       https://asawicki.info/news_1772_secrets_of_direct3d_12_do_rtv_and_dsv_descriptors_make_any_sense
	//       suggests that we can basically just create RTV/DSV desscriptors
	//       on-the-fly when using e.g. OMSetRenderTargets.
	for (uint32_t i = 0; i < kNumSwapchainBuffers; i++)
	{
		result = m_pSwapchain->GetBuffer(i, IID_PPV_ARGS(&m_swapchainRTVs[i]));
		CORE_ASSERT(SUCCEEDED(result));

		const D3D12_RENDER_TARGET_VIEW_DESC rtvDesc
		{
			.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
			.Texture2D
			{
				.MipSlice = 0,
				.PlaneSlice = 0
			}
		};

		m_swapchainRTVDescriptors[i].ptr = rtvHeapNextFree.ptr;

		m_pDevice->CreateRenderTargetView(m_swapchainRTVs[i], &rtvDesc, m_swapchainRTVDescriptors[i]);

		rtvHeapNextFree.ptr += rtvHeapIncrement;
	}

	for (size_t i = 0; i < kNumFrames; i++)
	{
		FrameState& frameState = m_frames[i];

		result = m_pDevice->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(&frameState.pCommandAllocator));
		CORE_ASSERT(SUCCEEDED(result));

		// TODO: Ignoring pInitialState for now, possibly worth setting that.
		result = m_pDevice->CreateCommandList(
			0u,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			frameState.pCommandAllocator,
			nullptr,
			IID_PPV_ARGS(&frameState.pCommandList));
		CORE_ASSERT(SUCCEEDED(result));

		frameState.pCommandList->Close();
	}

	result = m_pDevice->CreateFence(
		0u,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&m_pFence));
	CORE_ASSERT(SUCCEEDED(result));

	m_pFenceEvent = CreateEvent(nullptr, false, false, nullptr);

	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	unsigned char* pFontTextureData;
	int fontTextureWidth;
	int fontTextureHeight;
	ImGui::GetIO().Fonts->GetTexDataAsRGBA32(
		&pFontTextureData,
		&fontTextureWidth,
		&fontTextureHeight);
	CORE_ASSERT_NOT_NULL(pFontTextureData);

	const D3D12_RESOURCE_DESC textureDesc
	{
		.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
		.Width = static_cast<UINT64>(fontTextureWidth),
		.Height = static_cast<UINT>(fontTextureHeight),
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.SampleDesc = DXGI_SAMPLE_DESC
		{
			.Count = 1,
			.Quality = 0
		},
		.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		.Flags = D3D12_RESOURCE_FLAG_NONE
	};

	const D3D12MA::ALLOCATION_DESC defaultAllocationDesc
	{
		.Flags = D3D12MA::ALLOCATION_FLAG_NONE,
		.HeapType = D3D12_HEAP_TYPE_GPU_UPLOAD,
		.ExtraHeapFlags = D3D12_HEAP_FLAG_NONE,
		.CustomPool = nullptr,
		.pPrivateData = nullptr
	};

	result = m_pAllocator->CreateResource(
		&defaultAllocationDesc,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		&m_pImGuiFontTexture,
		IID_NULL,
		nullptr);
	CORE_ASSERT(SUCCEEDED(result));

	const D3D12_RESOURCE_DESC uploadBufferDesc
	{
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
		.Width = static_cast<UINT64>(fontTextureWidth * fontTextureHeight * 4),
		.Height = 1,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_UNKNOWN,
		.SampleDesc = DXGI_SAMPLE_DESC
		{
			.Count = 1,
			.Quality = 0
		},
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		.Flags = D3D12_RESOURCE_FLAG_NONE
	};

	D3D12MA::Allocation* pUploadBufferAllocation;

	result = m_pAllocator->CreateResource(
		&defaultAllocationDesc,
		&uploadBufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		&pUploadBufferAllocation,
		IID_NULL,
		nullptr);
	CORE_ASSERT(SUCCEEDED(result));

	const D3D12_RANGE uploadBufferMapRange = { 0, 0 };
	void* pMappedUploadBuffer = nullptr;

	result = pUploadBufferAllocation->GetResource()->Map(
		0,
		&uploadBufferMapRange,
		&pMappedUploadBuffer);
	CORE_ASSERT(SUCCEEDED(result));

	memcpy(
		pMappedUploadBuffer,
		pFontTextureData,
		fontTextureWidth * fontTextureHeight * 4);

	pUploadBufferAllocation->GetResource()->Unmap(0, nullptr);

	const D3D12_TEXTURE_COPY_LOCATION srcCopyLocation
	{
		.pResource = pUploadBufferAllocation->GetResource(),
		.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
		.PlacedFootprint = D3D12_PLACED_SUBRESOURCE_FOOTPRINT
		{
			.Offset = 0,
			.Footprint = D3D12_SUBRESOURCE_FOOTPRINT
			{
				.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
				.Width = static_cast<UINT>(fontTextureWidth),
				.Height = static_cast<UINT>(fontTextureHeight),
				.Depth = 1,
				.RowPitch = static_cast<UINT>(fontTextureWidth * 4)
			}
		}
	};

	const D3D12_TEXTURE_COPY_LOCATION dstCopyLocation
	{
		.pResource = m_pImGuiFontTexture->GetResource(),
		.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
		.SubresourceIndex = 0
	};

	// Just used frame 0 for the initial copy.
	FrameState& frame = m_frames[0];

	frame.pCommandAllocator->Reset();
	frame.pCommandList->Reset(frame.pCommandAllocator, nullptr);

	frame.pCommandList->CopyTextureRegion(
		&dstCopyLocation,
		0,
		0,
		0,
		&srcCopyLocation,
		nullptr);

	const D3D12_RESOURCE_BARRIER copyBarrier
	{
		.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
		.Transition = D3D12_RESOURCE_TRANSITION_BARRIER
		{
			.pResource = m_pImGuiFontTexture->GetResource(),
			.Subresource = 0,
			.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
			.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		}
	};

	frame.pCommandList->ResourceBarrier(1, &copyBarrier);

	frame.pCommandList->Close();

	std::array<ID3D12CommandList* const, 1> ppCommandLists = { frame.pCommandList };
	m_pCommandQueue->ExecuteCommandLists(1, ppCommandLists.data());

	result = m_pCommandQueue->Signal(m_pFence, frame.fenceValue);
	CORE_ASSERT(SUCCEEDED(result));

	pUploadBufferAllocation->Release();

	const D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc
	{
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		.NumDescriptors = 64u,
		.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
		.NodeMask = 0u
	};

	result = m_pDevice->CreateDescriptorHeap(
		&srvHeapDesc,
		IID_PPV_ARGS(&m_pSRVHeap));
	CORE_ASSERT(SUCCEEDED(result));

	D3D12_CPU_DESCRIPTOR_HANDLE srvHeapNextFree = m_pSRVHeap->GetCPUDescriptorHandleForHeapStart();
	const uint32_t srvHeapIncrement = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	const D3D12_SHADER_RESOURCE_VIEW_DESC textureResourceViewDesc
	{
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
		.Texture2D = D3D12_TEX2D_SRV
		{
			.MostDetailedMip = 0,
			.MipLevels = 1,
			.PlaneSlice = 0,
			.ResourceMinLODClamp = 0.0f
		}
	};

	m_pDevice->CreateShaderResourceView(
		m_pImGuiFontTexture->GetResource(),
		&textureResourceViewDesc,
		srvHeapNextFree);
	srvHeapNextFree.ptr += srvHeapIncrement;

	for (size_t i = 0; i < kNumFrames; i++)
	{
		ImGuiFrameState& frameState = m_imGuiFrames[i];

		const D3D12_RESOURCE_DESC bufferDesc
		{
			.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
			.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
			.Width = 1024 * 1024,
			.Height = 1,
			.DepthOrArraySize = 1,
			.MipLevels = 1,
			.Format = DXGI_FORMAT_UNKNOWN,
			.SampleDesc = DXGI_SAMPLE_DESC
			{
				.Count = 1,
				.Quality = 0
			},
			.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
			.Flags = D3D12_RESOURCE_FLAG_NONE
		};

		// Assume ReBAR supprt.
		CORE_ASSERT(m_pAllocator->IsGPUUploadHeapSupported());

		result = m_pAllocator->CreateResource(
			&defaultAllocationDesc,
			&bufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			&frameState.pVertexBufferAllocation,
			IID_NULL,
			nullptr);
		CORE_ASSERT(SUCCEEDED(result));

		// Upload only, no readback.
		const D3D12_RANGE bufferMapRange = { 0, 0 };

		result = frameState.pVertexBufferAllocation->GetResource()->Map(
			0,
			&bufferMapRange,
			&frameState.pMappedVertexBuffer);
		CORE_ASSERT(SUCCEEDED(result));

		result = m_pAllocator->CreateResource(
			&defaultAllocationDesc,
			&bufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			&frameState.pIndexBufferAllocation,
			IID_NULL,
			nullptr);
		CORE_ASSERT(SUCCEEDED(result));

		result = frameState.pIndexBufferAllocation->GetResource()->Map(
			0,
			&bufferMapRange,
			&frameState.pMappedIndexBuffer);
		CORE_ASSERT(SUCCEEDED(result));

		frameState.vertexBufferView =
		{
			.BufferLocation = frameState.pVertexBufferAllocation->GetResource()->GetGPUVirtualAddress(),
			.SizeInBytes = 1024 * 1024,
			.StrideInBytes = sizeof(ImDrawVert)
		};

		frameState.indexBufferView =
		{
			.BufferLocation = frameState.pIndexBufferAllocation->GetResource()->GetGPUVirtualAddress(),
			.SizeInBytes = 1024 * 1024,
			.Format = DXGI_FORMAT_R16_UINT
		};

		const D3D12_RESOURCE_DESC constantBufferDesc
		{
			.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
			.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
			.Width = 4 * sizeof(float),
			.Height = 1,
			.DepthOrArraySize = 1,
			.MipLevels = 1,
			.Format = DXGI_FORMAT_UNKNOWN,
			.SampleDesc = DXGI_SAMPLE_DESC
			{
				.Count = 1,
				.Quality = 0
			},
			.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
			.Flags = D3D12_RESOURCE_FLAG_NONE
		};
	}

	auto LoadShaderFile = [&](const char* filePath) -> std::vector<char>
	{
		std::ifstream stream = std::ifstream(filePath, std::ios::binary | std::ios::ate);
		CORE_ASSERT(stream.is_open());

		const std::streampos fileSize = stream.tellg();
		std::vector<char> fileBytes(static_cast<size_t>(fileSize));

		stream.seekg(0);
		stream.read(fileBytes.data(), static_cast<std::streamsize>(fileSize));
		stream.close();

		return fileBytes;
	};

	const std::vector<char> vertexShader = LoadShaderFile("Data\\Shaders\\Compiled\\ImGuiVert.dxil");
	const std::vector<char> fragmentShader = LoadShaderFile("Data\\Shaders\\Compiled\\ImGuiFrag.dxil");

	const D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] =
	{
		{
			.SemanticName = "POSITION",
			.SemanticIndex = 0,
			.Format = DXGI_FORMAT_R32G32_FLOAT,
			.InputSlot = 0,
			.AlignedByteOffset = 0,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0
		},
		{
			.SemanticName = "TEXCOORD",
			.SemanticIndex = 0,
			.Format = DXGI_FORMAT_R32G32_FLOAT,
			.InputSlot = 0,
			.AlignedByteOffset = 8,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0
		},
		{
			.SemanticName = "COLOR",
			.SemanticIndex = 0,
			.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
			.InputSlot = 0,
			.AlignedByteOffset = 16,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0
		}
	};

	const D3D12_ROOT_CONSTANTS vertexRootConstants
	{
		.ShaderRegister = 0,
		.RegisterSpace = 0,
		.Num32BitValues = 4
	};

	const D3D12_ROOT_CONSTANTS fragmentRootConstants
	{
		.ShaderRegister = 0,
		.RegisterSpace = 0,
		.Num32BitValues = 1
	};

	const D3D12_DESCRIPTOR_RANGE descriptorRange
	{
		.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		.NumDescriptors = 64u,
		.BaseShaderRegister = 0,
		.RegisterSpace = 0,
		.OffsetInDescriptorsFromTableStart = 0
	};

	const D3D12_ROOT_DESCRIPTOR_TABLE rootDescriptorTable
	{
		.NumDescriptorRanges = 1,
		.pDescriptorRanges = &descriptorRange
	};

	const D3D12_ROOT_PARAMETER rootParameters[3] =
	{
		{
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
			.Constants = vertexRootConstants,
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX
		},
		{
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
			.Constants = fragmentRootConstants,
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL
		},
		{
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			.DescriptorTable = rootDescriptorTable,
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL
		}
	};

	const D3D12_STATIC_SAMPLER_DESC staticSamplerDesc
	{
		.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		.MipLODBias = 0.0f,
		.MaxAnisotropy = 0,
		.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS,
		.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
		.MinLOD = 0.0f,
		.MaxLOD = D3D12_FLOAT32_MAX,
		.ShaderRegister = 0,
		.RegisterSpace = 0,
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL
	};

	const D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc
	{
		.NumParameters = 3,
		.pParameters = &rootParameters[0],
		.NumStaticSamplers = 1,
		.pStaticSamplers = &staticSamplerDesc,
		.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	};

	ID3DBlob* pBlob = nullptr;
	ID3DBlob* pErrorBlob = nullptr;
	result = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&pBlob,
		&pErrorBlob);
	CORE_ASSERT(SUCCEEDED(result));

	result = m_pDevice->CreateRootSignature(
		0,
		pBlob->GetBufferPointer(),
		pBlob->GetBufferSize(),
		IID_PPV_ARGS(&m_pImGuiRootSignature));
	CORE_ASSERT(SUCCEEDED(result));

	const D3D12_GRAPHICS_PIPELINE_STATE_DESC guiPipelineDesc
	{
		.pRootSignature = m_pImGuiRootSignature,
		.VS = D3D12_SHADER_BYTECODE
		{
			.pShaderBytecode = vertexShader.data(),
			.BytecodeLength = vertexShader.size()
		},
		.PS = D3D12_SHADER_BYTECODE
		{
			.pShaderBytecode = fragmentShader.data(),
			.BytecodeLength = fragmentShader.size()
		},
		.DS = {},
		.HS = {},
		.GS = {},
		.StreamOutput = {},
		.BlendState = D3D12_BLEND_DESC
		{
			.AlphaToCoverageEnable = false,
			.IndependentBlendEnable = false,
			.RenderTarget =
			{
				D3D12_RENDER_TARGET_BLEND_DESC
				{
					.BlendEnable = true,
					.LogicOpEnable = false,
					.SrcBlend = D3D12_BLEND_SRC_ALPHA,
					.DestBlend = D3D12_BLEND_INV_SRC_ALPHA,
					.BlendOp = D3D12_BLEND_OP_ADD,
					.SrcBlendAlpha = D3D12_BLEND_ONE,
					.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA,
					.BlendOpAlpha = D3D12_BLEND_OP_ADD,
					.LogicOp = D3D12_LOGIC_OP_NOOP,
					.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
				}
			}
		},
		.SampleMask = UINT_MAX,
		.RasterizerState = D3D12_RASTERIZER_DESC
		{
			.FillMode = D3D12_FILL_MODE_SOLID,
			.CullMode = D3D12_CULL_MODE_NONE,
			.FrontCounterClockwise = false,
			.DepthBias = 0,
			.DepthBiasClamp = 0.0f,
			.SlopeScaledDepthBias = 0.0f,
			.DepthClipEnable = false,
			.MultisampleEnable = false,
			.AntialiasedLineEnable = false,
			.ForcedSampleCount = 0,
			.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
		},
		.DepthStencilState = D3D12_DEPTH_STENCIL_DESC
		{
			.DepthEnable = false,
			.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
			.DepthFunc = D3D12_COMPARISON_FUNC_NONE,
			.StencilEnable = false,
			.StencilWriteMask = 0x00,
			.FrontFace = {},
			.BackFace = {}
		},
		.InputLayout = D3D12_INPUT_LAYOUT_DESC
		{
			.pInputElementDescs = &inputElementDescs[0],
			.NumElements = 3
		},
		.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.NumRenderTargets = 1,
		.RTVFormats =
		{
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			DXGI_FORMAT_UNKNOWN,
			DXGI_FORMAT_UNKNOWN,
			DXGI_FORMAT_UNKNOWN,
			DXGI_FORMAT_UNKNOWN,
			DXGI_FORMAT_UNKNOWN,
			DXGI_FORMAT_UNKNOWN,
			DXGI_FORMAT_UNKNOWN
		},
		.DSVFormat = DXGI_FORMAT_UNKNOWN,
		.SampleDesc = DXGI_SAMPLE_DESC
		{
			.Count = 1,
			.Quality = 0,
		},
		.NodeMask = 0,
		.CachedPSO = D3D12_CACHED_PIPELINE_STATE
		{
			.pCachedBlob = nullptr,
			.CachedBlobSizeInBytes = 0
		},
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	result = m_pDevice->CreateGraphicsPipelineState(
		&guiPipelineDesc,
		IID_PPV_ARGS(&m_pImGuiPipelineState));
	CORE_ASSERT(SUCCEEDED(result));

	// Match the window resolution for rendering for now, but this
	// should probably be configurable.
	const D3D12_RESOURCE_DESC sceneTextureDesc
	{
		.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
		.Width = static_cast<UINT64>(windowWidth),
		.Height = static_cast<UINT>(windowHeight),
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		.SampleDesc = DXGI_SAMPLE_DESC
		{
			.Count = 1,
			.Quality = 0
		},
		.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
	};

	result = m_pAllocator->CreateResource(
		&defaultAllocationDesc,
		&sceneTextureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		&m_pSceneTexture,
		IID_NULL,
		nullptr);
	CORE_ASSERT(SUCCEEDED(result));

	m_SceneTextureDescriptor = rtvHeapNextFree;
	m_pDevice->CreateRenderTargetView(
		m_pSceneTexture->GetResource(),
		nullptr,
		m_SceneTextureDescriptor);
	rtvHeapNextFree.ptr += rtvHeapIncrement;

	m_pDevice->CreateShaderResourceView(
		m_pSceneTexture->GetResource(),
		nullptr,
		srvHeapNextFree);
	srvHeapNextFree.ptr += srvHeapIncrement;
}



Renderer::~Renderer()
{
	HRESULT result = S_OK;

	// Wait on the final fence signal so we know the queue is idle.
	const uint64_t lastFenceValue = m_frames[(m_frameNumber - 1) % kNumFrames].fenceValue;
	if (m_pFence->GetCompletedValue() < lastFenceValue)
	{
		result = m_pFence->SetEventOnCompletion(lastFenceValue, m_pFenceEvent);
		CORE_ASSERT(SUCCEEDED(result));

		WaitForSingleObject(m_pFenceEvent, INFINITE);
	}

	m_pFence->Release();

	m_pSceneTexture->Release();

	m_pImGuiPipelineState->Release();
	m_pImGuiRootSignature->Release();

	for (ImGuiFrameState& imGuiFrameState : m_imGuiFrames)
	{
		imGuiFrameState.pVertexBufferAllocation->GetResource()->Unmap(
			0,
			nullptr);

		imGuiFrameState.pVertexBufferAllocation->Release();

		imGuiFrameState.pIndexBufferAllocation->GetResource()->Unmap(
			0,
			nullptr);

		imGuiFrameState.pIndexBufferAllocation->Release();
	}

	m_pImGuiFontTexture->Release();
	m_pSRVHeap->Release();

	for (FrameState& frameState : m_frames)
	{
		frameState.pCommandList->Release();
		frameState.pCommandAllocator->Release();
	}

	for (uint32_t i = 0; i < kNumSwapchainBuffers; i++)
	{
		m_swapchainRTVs[i]->Release();
	}

	m_pRTVHeap->Release();
	m_pSwapchain->Release();
	m_pCommandQueue->Release();
	m_pAllocator->Release();
	m_pDevice->Release();
	m_pFactory->Release();
	m_pDebug->Release();
}



void Renderer::Draw()
{
	HRESULT result = S_OK;

	const uint32_t imageIndex = m_pSwapchain->GetCurrentBackBufferIndex();
	const size_t frameIndex = m_frameNumber % kNumFrames;

	FrameState& frameState = m_frames[frameIndex];

	if (m_pFence->GetCompletedValue() < frameState.fenceValue)
	{
		result = m_pFence->SetEventOnCompletion(frameState.fenceValue, m_pFenceEvent);
		CORE_ASSERT(SUCCEEDED(result));

		WaitForSingleObject(m_pFenceEvent, INFINITE);
	}

	const size_t lastFrameIndex = (frameIndex + kNumFrames - 1) % kNumFrames;
	frameState.fenceValue = m_frames[lastFrameIndex].fenceValue + 1;

	// Equivalent to resetting a command pool.
	frameState.pCommandAllocator->Reset();

	// Equivalent to beginning a command buffer.
	frameState.pCommandList->Reset(frameState.pCommandAllocator, nullptr);

	ImGui::NewFrame();

	ImGui::DockSpaceOverViewport();

	if (ImGui::Begin("Scene", nullptr))
	{
		const ImVec2 windowSize = ImGui::GetContentRegionAvail();

		ImGui::Image(static_cast<ImTextureRef>(1), windowSize);

		ImGui::End();
	}

	ImGui::ShowDemoWindow();

	ImGui::Render();
	ImDrawData* pDrawData = ImGui::GetDrawData();
	CORE_ASSERT_NOT_NULL(pDrawData);

	if (pDrawData->TotalVtxCount > 0)
	{
		ImGuiFrameState& imGuiFrame = m_imGuiFrames[frameIndex];

		size_t vertexBufferOffset = 0;
		size_t indexBufferOffset = 0;

		for (const ImDrawList* pDrawList : pDrawData->CmdLists)
		{
			size_t vertexDataSize = pDrawList->VtxBuffer.Size * sizeof(ImDrawVert);
			size_t indexDataSize = pDrawList->IdxBuffer.Size * sizeof(ImDrawIdx);

			memcpy(
				(char*)imGuiFrame.pMappedVertexBuffer + vertexBufferOffset,
				pDrawList->VtxBuffer.Data,
				vertexDataSize);

			memcpy(
				(char*)imGuiFrame.pMappedIndexBuffer + indexBufferOffset,
				pDrawList->IdxBuffer.Data,
				indexDataSize);

			vertexBufferOffset += vertexDataSize;
			indexBufferOffset += indexDataSize;
		}
	}

	const D3D12_RESOURCE_BARRIER clearBarrier
	{
		.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
		.Transition = D3D12_RESOURCE_TRANSITION_BARRIER
		{
			.pResource = m_swapchainRTVs[imageIndex],
			.Subresource = 0,
			.StateBefore = D3D12_RESOURCE_STATE_PRESENT,
			.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET
		}
	};

	frameState.pCommandList->ResourceBarrier(1, &clearBarrier);

	frameState.pCommandList->OMSetRenderTargets(1, &m_swapchainRTVDescriptors[imageIndex], false, nullptr);

	// Sine wave between blue and magenta.
	const double red = std::sin(static_cast<float>(m_frameNumber) * ((2.0f * std::numbers::pi) / 255.0f));

	const float clearColour[4] = { static_cast<float>(red + 1.0f) / 2.0f, 0.0f, 1.0f, 1.0f };
	frameState.pCommandList->ClearRenderTargetView(m_SceneTextureDescriptor, clearColour, 0, nullptr);

	frameState.pCommandList->SetPipelineState(m_pImGuiPipelineState);

	frameState.pCommandList->SetGraphicsRootSignature(m_pImGuiRootSignature);

	frameState.pCommandList->SetDescriptorHeaps(1, &m_pSRVHeap);

	frameState.pCommandList->SetGraphicsRootDescriptorTable(2, m_pSRVHeap->GetGPUDescriptorHandleForHeapStart());

	// { translation.x, translation.y, scale.x, scale.y }
	const float rootConstants[4] =
	{
		-1.0f - pDrawData->DisplayPos.x * ( 2.0f / pDrawData->DisplaySize.x),
		 1.0f - pDrawData->DisplayPos.y * (-2.0f / pDrawData->DisplaySize.y),
		 2.0f / pDrawData->DisplaySize.x,
		-2.0f / pDrawData->DisplaySize.y
	};

	frameState.pCommandList->SetGraphicsRoot32BitConstants(0, 4, &rootConstants[0], 0);

	const D3D12_VIEWPORT viewport =
	{
		.TopLeftX = 0.0f,
		.TopLeftY = 0.0f,
		.Width = 2560.0f,
		.Height = 1440.0f,
		.MinDepth = 0.0f,
		.MaxDepth = 1.0f
	};

	const D3D12_RECT scissor =
	{
		.left = 0,
		.top = 0,
		.right = 2560,
		.bottom = 1440
	};

	frameState.pCommandList->RSSetViewports(1, &viewport);
	frameState.pCommandList->RSSetScissorRects(1, &scissor);
	frameState.pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	frameState.pCommandList->IASetVertexBuffers(0, 1, &m_imGuiFrames[frameIndex].vertexBufferView);
	frameState.pCommandList->IASetIndexBuffer(&m_imGuiFrames[frameIndex].indexBufferView);

	uint32_t vertexBufferOffset = 0;
	uint32_t indexBufferOffset = 0;

	const int framebufferWidth = static_cast<int>(pDrawData->DisplaySize.x * pDrawData->FramebufferScale.x);
	const int framebufferHeight = static_cast<int>(pDrawData->DisplaySize.y * pDrawData->FramebufferScale.y);

	for (const ImDrawList* pDrawList : pDrawData->CmdLists)
	{
		for (const ImDrawCmd& drawCommand : pDrawList->CmdBuffer)
		{
			const ImVec2 clipMin =
			{
				std::max((drawCommand.ClipRect.x - pDrawData->DisplayPos.x) * pDrawData->FramebufferScale.x, 0.0f),
				std::max((drawCommand.ClipRect.y - pDrawData->DisplayPos.y) * pDrawData->FramebufferScale.y, 0.0f)
			};

			const ImVec2 clipMax =
			{
				std::min((drawCommand.ClipRect.z - pDrawData->DisplayPos.x) * pDrawData->FramebufferScale.x, (float)framebufferWidth),
				std::min((drawCommand.ClipRect.w - pDrawData->DisplayPos.y) * pDrawData->FramebufferScale.y, (float)framebufferHeight)
			};

			const D3D12_RECT scissorRect =
			{
				.left = static_cast<LONG>(clipMin.x),
				.top = static_cast<LONG>(clipMin.y),
				.right = static_cast<LONG>(clipMax.x),
				.bottom = static_cast<LONG>(clipMax.y)
			};

			frameState.pCommandList->RSSetScissorRects(1, &scissorRect);

			const uint32_t textureIndex = static_cast<uint32_t>(drawCommand.TexRef.GetTexID());

			frameState.pCommandList->SetGraphicsRoot32BitConstants(1, 1, &textureIndex, 0);

			frameState.pCommandList->DrawIndexedInstanced(
				drawCommand.ElemCount,
				1,
				indexBufferOffset + drawCommand.IdxOffset,
				vertexBufferOffset + drawCommand.VtxOffset,
				0);
		}

		vertexBufferOffset += pDrawList->VtxBuffer.Size;
		indexBufferOffset += pDrawList->IdxBuffer.Size;
	}

	const D3D12_RESOURCE_BARRIER presentBarrier
	{
		.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
		.Transition = D3D12_RESOURCE_TRANSITION_BARRIER
		{
			.pResource = m_swapchainRTVs[imageIndex],
			.Subresource = 0,
			.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
			.StateAfter = D3D12_RESOURCE_STATE_PRESENT
		}
	};

	frameState.pCommandList->ResourceBarrier(1, &presentBarrier);

	result = frameState.pCommandList->Close();
	CORE_ASSERT(SUCCEEDED(result));

	std::array<ID3D12CommandList* const, 1> ppCommandLists = { frameState.pCommandList };

	m_pCommandQueue->ExecuteCommandLists(1, ppCommandLists.data());

	result = m_pSwapchain->Present(1u, 0u);
	CORE_ASSERT(SUCCEEDED(result));

	result = m_pCommandQueue->Signal(m_pFence, frameState.fenceValue);
	CORE_ASSERT(SUCCEEDED(result));

	m_frameNumber++;
}

} // namespace Demo