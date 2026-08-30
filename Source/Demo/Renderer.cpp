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
#include "SDL3/SDL_video.h"

// System Headers
#include <array>
#include <cstdint>
#include <numbers>
#include <thread>



namespace Demo
{

Renderer::Renderer(const SDL_Window* pWindow, uint32_t windowProperties)
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
	, m_frameNumber(0)
{
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
		.Width = 2560u,
		.Height = 1440u,
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

	const D3D12_CPU_DESCRIPTOR_HANDLE rtvHeapStart = m_pRTVHeap->GetCPUDescriptorHandleForHeapStart();
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

		m_swapchainRTVDescriptors[i].ptr = rtvHeapStart.ptr + (i * rtvHeapIncrement);

		m_pDevice->CreateRenderTargetView(m_swapchainRTVs[i], &rtvDesc, m_swapchainRTVDescriptors[i]);
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
		m_pSRVHeap->GetCPUDescriptorHandleForHeapStart());
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

	// Sine wave between blue and magenta.
	const double red = std::sin(static_cast<float>(m_frameNumber) * ((2.0f * std::numbers::pi) / 255.0f));

	const float clearColour[4] = { static_cast<float>(red + 1.0f) / 2.0f, 0.0f, 1.0f, 1.0f };
	frameState.pCommandList->ClearRenderTargetView(m_swapchainRTVDescriptors[imageIndex], clearColour, 0, nullptr);

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