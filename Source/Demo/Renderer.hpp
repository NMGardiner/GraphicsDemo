#ifndef GRAPHICSDEMO_RENDERER_HPP
#define GRAPHICSDEMO_RENDERER_HPP

// CoreLib
#include "CoreLib/UtilMacros.hpp"

// D3D12
#define NOMINMAX
#include <d3d12.h>
#include <dxgi1_6.h>

// D3D12MemoryAllocator
#include "D3D12MemAlloc.h"

// System Headers
#include <array>
#include <cstdint>



// Forward Declarations

// SDL
typedef struct SDL_Window SDL_Window;
typedef struct SDL_WindowProperties SDL_WindowProperties;



// Constants

constexpr size_t kNumSwapchainBuffers = 2;
constexpr size_t kNumFrames = 2;




namespace Demo
{

class Renderer
{

public:
	Renderer(const SDL_Window* pWindow, uint32_t windowProperties);
	~Renderer();

	CORE_CLASS_CONSTRUCTORS_DELETE(Renderer);

	void Draw();

private:
	ID3D12Debug1* m_pDebug;
	IDXGIFactory6* m_pFactory;
	ID3D12Device* m_pDevice;
	D3D12MA::Allocator* m_pAllocator;
	ID3D12CommandQueue* m_pCommandQueue;
	IDXGISwapChain3* m_pSwapchain;
	ID3D12DescriptorHeap* m_pRTVHeap;
	std::array<ID3D12Resource*, kNumSwapchainBuffers> m_swapchainRTVs;
	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kNumSwapchainBuffers> m_swapchainRTVDescriptors;

	struct FrameState
	{
		ID3D12CommandAllocator* pCommandAllocator;
		ID3D12GraphicsCommandList7* pCommandList = nullptr;
		uint64_t fenceValue = 0u;
	};

	std::array<FrameState, kNumFrames> m_frames;
	ID3D12Fence* m_pFence;
	HANDLE m_pFenceEvent;

	ID3D12DescriptorHeap* m_pSRVHeap;
	D3D12MA::Allocation* m_pImGuiFontTexture;

	// Persistently-mapped vertex/index buffers.
	struct ImGuiFrameState
	{
		D3D12MA::Allocation* pVertexBufferAllocation;
		D3D12MA::Allocation* pIndexBufferAllocation;
		void* pMappedVertexBuffer;
		void* pMappedIndexBuffer;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
		D3D12_INDEX_BUFFER_VIEW indexBufferView;
	};

	std::array<ImGuiFrameState, kNumFrames> m_imGuiFrames;

	ID3D12RootSignature* m_pImGuiRootSignature;
	ID3D12PipelineState* m_pImGuiPipelineState;

	size_t m_frameNumber;

}; // class Renderer

} // namespace Demo

#endif // GRAPHICSDEMO_RENDERER_HPP