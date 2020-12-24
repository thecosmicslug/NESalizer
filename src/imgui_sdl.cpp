#include "SDL.h"
#include "imgui.h"
#include "imgui_sdl.h"

#include <map>
#include <list>
#include <cmath>
#include <array>
#include <vector>
#include <memory>
#include <iostream>
#include <algorithm>
#include <functional>
#include <unordered_map>

static bool g_MousePressed[3] = { false, false, false };
static SDL_Cursor*  g_MouseCursors[ImGuiMouseCursor_COUNT] = {};
static SDL_Window*  g_Window = NULL;
static Uint64       g_Time = 0;
static char*        g_ClipboardTextData = NULL;
static bool         g_MouseCanUseGlobalState = true;

static const char* ImGui_GetClipboardText(void*)
{
    if (g_ClipboardTextData)
        SDL_free(g_ClipboardTextData);
    g_ClipboardTextData = SDL_GetClipboardText();
    return g_ClipboardTextData;
}

static void ImGui_SetClipboardText(void*, const char* text)
{
    SDL_SetClipboardText(text);
}

static void UpdateMouseCursor()
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)
        return;

    ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
    if (io.MouseDrawCursor || imgui_cursor == ImGuiMouseCursor_None)
    {
        // Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
        SDL_ShowCursor(SDL_FALSE);
    }
    else
    {
        // Show OS mouse cursor
        SDL_SetCursor(g_MouseCursors[imgui_cursor] ? g_MouseCursors[imgui_cursor] : g_MouseCursors[ImGuiMouseCursor_Arrow]);
        SDL_ShowCursor(SDL_TRUE);
    }
}

static void UpdateGamepads()
{
    ImGuiIO& io = ImGui::GetIO();
    memset(io.NavInputs, 0, sizeof(io.NavInputs));
    if ((io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) == 0)
        return;

    // Get gamepad
    SDL_GameController* game_controller = SDL_GameControllerOpen(0);
    if (!game_controller)
    {
        io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
        return;
    }

    // Update gamepad inputs
    #define MAP_BUTTON(NAV_NO, BUTTON_NO)       { io.NavInputs[NAV_NO] = (SDL_GameControllerGetButton(game_controller, BUTTON_NO) != 0) ? 1.0f : 0.0f; }
    #define MAP_ANALOG(NAV_NO, AXIS_NO, V0, V1) { float vn = (float)(SDL_GameControllerGetAxis(game_controller, AXIS_NO) - V0) / (float)(V1 - V0); if (vn > 1.0f) vn = 1.0f; if (vn > 0.0f && io.NavInputs[NAV_NO] < vn) io.NavInputs[NAV_NO] = vn; }
    const int thumb_dead_zone = 8000;           // SDL_gamecontroller.h suggests using this value.
    MAP_BUTTON(ImGuiNavInput_Activate,      SDL_CONTROLLER_BUTTON_A);               // Cross / A
    MAP_BUTTON(ImGuiNavInput_Cancel,        SDL_CONTROLLER_BUTTON_B);               // Circle / B
    MAP_BUTTON(ImGuiNavInput_Menu,          SDL_CONTROLLER_BUTTON_X);               // Square / X
    MAP_BUTTON(ImGuiNavInput_Input,         SDL_CONTROLLER_BUTTON_Y);               // Triangle / Y
    MAP_BUTTON(ImGuiNavInput_DpadLeft,      SDL_CONTROLLER_BUTTON_DPAD_LEFT);       // D-Pad Left
    MAP_BUTTON(ImGuiNavInput_DpadRight,     SDL_CONTROLLER_BUTTON_DPAD_RIGHT);      // D-Pad Right
    MAP_BUTTON(ImGuiNavInput_DpadUp,        SDL_CONTROLLER_BUTTON_DPAD_UP);         // D-Pad Up
    MAP_BUTTON(ImGuiNavInput_DpadDown,      SDL_CONTROLLER_BUTTON_DPAD_DOWN);       // D-Pad Down
    MAP_BUTTON(ImGuiNavInput_FocusPrev,     SDL_CONTROLLER_BUTTON_LEFTSHOULDER);    // L1 / LB
    MAP_BUTTON(ImGuiNavInput_FocusNext,     SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);   // R1 / RB
    MAP_BUTTON(ImGuiNavInput_TweakSlow,     SDL_CONTROLLER_BUTTON_LEFTSHOULDER);    // L1 / LB
    MAP_BUTTON(ImGuiNavInput_TweakFast,     SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);   // R1 / RB
    MAP_ANALOG(ImGuiNavInput_LStickLeft,    SDL_CONTROLLER_AXIS_LEFTX, -thumb_dead_zone, -32768);
    MAP_ANALOG(ImGuiNavInput_LStickRight,   SDL_CONTROLLER_AXIS_LEFTX, +thumb_dead_zone, +32767);
    MAP_ANALOG(ImGuiNavInput_LStickUp,      SDL_CONTROLLER_AXIS_LEFTY, -thumb_dead_zone, -32767);
    MAP_ANALOG(ImGuiNavInput_LStickDown,    SDL_CONTROLLER_AXIS_LEFTY, +thumb_dead_zone, +32767);

    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    #undef MAP_BUTTON
    #undef MAP_ANALOG
}

static void UpdateMousePosAndButtons()
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantSetMousePos)
        SDL_WarpMouseInWindow(g_Window, (int)io.MousePos.x, (int)io.MousePos.y);
    else
        io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);

    int mx, my;
    Uint32 mouse_buttons = SDL_GetMouseState(&mx, &my);
    io.MouseDown[0] = g_MousePressed[0] || (mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;  // If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
    io.MouseDown[1] = g_MousePressed[1] || (mouse_buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
    io.MouseDown[2] = g_MousePressed[2] || (mouse_buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0;
    g_MousePressed[0] = g_MousePressed[1] = g_MousePressed[2] = false;

#if SDL_HAS_CAPTURE_AND_GLOBAL_MOUSE && !defined(__EMSCRIPTEN__) && !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IOS)
    SDL_Window* focused_window = SDL_GetKeyboardFocus();
    if (g_Window == focused_window)
    {
        if (g_MouseCanUseGlobalState)
        {

            int wx, wy;
            SDL_GetWindowPosition(focused_window, &wx, &wy);
            SDL_GetGlobalMouseState(&mx, &my);
            mx -= wx;
            my -= wy;
        }
        io.MousePos = ImVec2((float)mx, (float)my);
    }

    // SDL_CaptureMouse() let the OS know e.g. that our imgui drag outside the SDL window boundaries shouldn't e.g. trigger the OS window resize cursor.
    // The function is only supported from SDL 2.0.4 (released Jan 2016)
    bool any_mouse_button_down = ImGui::IsAnyMouseDown();
    SDL_CaptureMouse(any_mouse_button_down ? SDL_TRUE : SDL_FALSE);
#else
    if (SDL_GetWindowFlags(g_Window) & SDL_WINDOW_INPUT_FOCUS)
        io.MousePos = ImVec2((float)mx, (float)my);
#endif
}

namespace
{
	struct Device* CurrentDevice = nullptr;

	namespace TupleHash
	{
		template <typename T> struct Hash
		{
			std::size_t operator()(const T& value) const
			{
				return std::hash<T>()(value);
			}
		};

		template <typename T> void CombineHash(std::size_t& seed, const T& value)
		{
			seed ^= TupleHash::Hash<T>()(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		}

		template <typename Tuple, std::size_t Index = std::tuple_size<Tuple>::value - 1> struct Hasher
		{
			static void Hash(std::size_t& seed, const Tuple& tuple)
			{
				Hasher<Tuple, Index - 1>::Hash(seed, tuple);
				CombineHash(seed, std::get<Index>(tuple));
			}
		};

		template <typename Tuple> struct Hasher<Tuple, 0>
		{
			static void Hash(std::size_t& seed, const Tuple& tuple)
			{
				CombineHash(seed, std::get<0>(tuple));
			}
		};

		template <typename... T> struct Hash<std::tuple<T...>>
		{
			std::size_t operator()(const std::tuple<T...>& value) const
			{
				std::size_t seed = 0;
				Hasher<std::tuple<T...>>::Hash(seed, value);
				return seed;
			}
		};
	}

	template <typename Key, typename Value, std::size_t Size> class LRUCache
	{
	public:
		bool Contains(const Key& key) const
		{
			return Container.find(key) != Container.end();
		}

		const Value& At(const Key& key)
		{
			assert(Contains(key));

			const auto location = Container.find(key);
			Order.splice(Order.begin(), Order, location->second);
			return location->second->second;
		}

		void Insert(const Key& key, Value value)
		{
			const auto existingLocation = Container.find(key);
			if (existingLocation != Container.end())
			{
				Order.erase(existingLocation->second);
				Container.erase(existingLocation);
			}

			Order.push_front(std::make_pair(key, std::move(value)));
			Container.insert(std::make_pair(key, Order.begin()));

			Clean();
		}
	private:
		void Clean()
		{
			while (Container.size() > Size)
			{
				auto last = Order.end();
				last--;
				Container.erase(last->first);
				Order.pop_back();
			}
		}

		std::list<std::pair<Key, Value>> Order;
		std::unordered_map<Key, decltype(Order.begin()), TupleHash::Hash<Key>> Container;
	};

	struct Color
	{
		const float R, G, B, A;

		explicit Color(uint32_t color)
			: R(((color >> 0) & 0xff) / 255.0f), G(((color >> 8) & 0xff) / 255.0f), B(((color >> 16) & 0xff) / 255.0f), A(((color >> 24) & 0xff) / 255.0f) { }
		Color(float r, float g, float b, float a) : R(r), G(g), B(b), A(a) { }

		Color operator*(const Color& c) const { return Color(R * c.R, G * c.G, B * c.B, A * c.A); }
		Color operator*(float v) const { return Color(R * v, G * v, B * v, A * v); }
		Color operator+(const Color& c) const { return Color(R + c.R, G + c.G, B + c.B, A + c.A); }

		uint32_t ToInt() const
		{
			return	((static_cast<int>(R * 255) & 0xff) << 0)
				  | ((static_cast<int>(G * 255) & 0xff) << 8)
				  | ((static_cast<int>(B * 255) & 0xff) << 16)
				  | ((static_cast<int>(A * 255) & 0xff) << 24);
		}

		void UseAsDrawColor(SDL_Renderer* renderer) const
		{
			SDL_SetRenderDrawColor(renderer,
				static_cast<uint8_t>(R * 255),
				static_cast<uint8_t>(G * 255),
				static_cast<uint8_t>(B * 255),
				static_cast<uint8_t>(A * 255));
		}
	};

	struct Device
	{
		SDL_Renderer* Renderer;

		struct ClipRect
		{
			int X, Y, Width, Height;
		} Clip;

		struct TriangleCacheItem
		{
			SDL_Texture* Texture = nullptr;
			int Width = 0, Height = 0;

			~TriangleCacheItem() { if (Texture) SDL_DestroyTexture(Texture); }
		};

		// You can tweak these to values that you find that work the best.
		static constexpr std::size_t UniformColorTriangleCacheSize = 512;
		static constexpr std::size_t GenericTriangleCacheSize = 64;

		// Uniform color is identified by its color and the coordinates of the edges.
		using UniformColorTriangleKey = std::tuple<uint32_t, int, int, int, int, int, int>;
		// The generic triangle cache unfortunately has to be basically a full representation of the triangle.
		// This includes the (offset) vertex positions, texture coordinates and vertex colors.
		using GenericTriangleVertexKey = std::tuple<int, int, double, double, uint32_t>;
		using GenericTriangleKey = std::tuple<GenericTriangleVertexKey, GenericTriangleVertexKey, GenericTriangleVertexKey>;

		LRUCache<UniformColorTriangleKey, std::unique_ptr<TriangleCacheItem>, UniformColorTriangleCacheSize> UniformColorTriangleCache;
		LRUCache<GenericTriangleKey, std::unique_ptr<TriangleCacheItem>, GenericTriangleCacheSize> GenericTriangleCache;

		Device(SDL_Renderer* renderer) : Renderer(renderer) { }

		void SetClipRect(const ClipRect& rect)
		{
			Clip = rect;
			const SDL_Rect clip = { rect.X, rect.Y, rect.Width, rect.Height };
			SDL_RenderSetClipRect(Renderer, &clip);
		}

		void EnableClip() { SetClipRect(Clip); }
		void DisableClip() { SDL_RenderSetClipRect(Renderer, nullptr); }

		void SetAt(int x, int y, const Color& color)
		{
			color.UseAsDrawColor(Renderer);
			SDL_RenderDrawPoint(Renderer, x, y);
		}

		SDL_Texture* MakeTexture(int width, int height)
		{
			SDL_Texture* texture = SDL_CreateTexture(Renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, width, height);
			SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
			return texture;
		}

		void UseAsRenderTarget(SDL_Texture* texture)
		{
			SDL_SetRenderTarget(Renderer, texture);
			if (texture)
			{
				SDL_SetRenderDrawColor(Renderer, 0, 0, 0, 0);
				SDL_RenderClear(Renderer);
			}
		}
	};

	struct Texture
	{
		SDL_Surface* Surface;
		SDL_Texture* Source;

		~Texture()
		{
			SDL_FreeSurface(Surface);
			SDL_DestroyTexture(Source);
		}

		Color Sample(float u, float v) const
		{
			const int x = static_cast<int>(std::round(u * (Surface->w - 1) + 0.5f));
			const int y = static_cast<int>(std::round(v * (Surface->h - 1) + 0.5f));

			const int location = y * Surface->w + x;
			assert(location < Surface->w * Surface->h);

			return Color(static_cast<uint32_t*>(Surface->pixels)[location]);
		}
	};

	template <typename T> class InterpolatedFactorEquation
	{
	public:
		InterpolatedFactorEquation(const T& value0, const T& value1, const T& value2, const ImVec2& v0, const ImVec2& v1, const ImVec2& v2)
			: Value0(value0), Value1(value1), Value2(value2), V0(v0), V1(v1), V2(v2),
			Divisor((V1.y - V2.y) * (V0.x - V2.x) + (V2.x - V1.x) * (V0.y - V2.y)) { }

		T Evaluate(float x, float y) const
		{
			const float w1 = ((V1.y - V2.y) * (x - V2.x) + (V2.x - V1.x) * (y - V2.y)) / Divisor;
			const float w2 = ((V2.y - V0.y) * (x - V2.x) + (V0.x - V2.x) * (y - V2.y)) / Divisor;
			const float w3 = 1.0f - w1 - w2;

			return static_cast<T>((Value0 * w1) + (Value1 * w2) + (Value2 * w3));
		}
	private:
		const T Value0;
		const T Value1;
		const T Value2;

		const ImVec2& V0;
		const ImVec2& V1;
		const ImVec2& V2;

		const float Divisor;
	};

	struct Rect
	{
		float MinX, MinY, MaxX, MaxY;
		float MinU, MinV, MaxU, MaxV;

		bool IsOnExtreme(const ImVec2& point) const
		{
			return (point.x == MinX || point.x == MaxX) && (point.y == MinY || point.y == MaxY);
		}

		bool UsesOnlyColor() const
		{
			const ImVec2& whitePixel = ImGui::GetIO().Fonts->TexUvWhitePixel;

			return MinU == MaxU && MinU == whitePixel.x && MinV == MaxV && MaxV == whitePixel.y;
		}

		static Rect CalculateBoundingBox(const ImDrawVert& v0, const ImDrawVert& v1, const ImDrawVert& v2)
		{
			return Rect{
				std::min({ v0.pos.x, v1.pos.x, v2.pos.x }),
				std::min({ v0.pos.y, v1.pos.y, v2.pos.y }),
				std::max({ v0.pos.x, v1.pos.x, v2.pos.x }),
				std::max({ v0.pos.y, v1.pos.y, v2.pos.y }),
				std::min({ v0.uv.x, v1.uv.x, v2.uv.x }),
				std::min({ v0.uv.y, v1.uv.y, v2.uv.y }),
				std::max({ v0.uv.x, v1.uv.x, v2.uv.x }),
				std::max({ v0.uv.y, v1.uv.y, v2.uv.y })
			};
		}
	};

	struct FixedPointTriangleRenderInfo
	{
		int X1, X2, X3, Y1, Y2, Y3;
		int MinX, MaxX, MinY, MaxY;

		static FixedPointTriangleRenderInfo CalculateFixedPointTriangleInfo(const ImVec2& v1, const ImVec2& v2, const ImVec2& v3)
		{
			static constexpr float scale = 16.0f;

			const int x1 = static_cast<int>(std::round(v1.x * scale));
			const int x2 = static_cast<int>(std::round(v2.x * scale));
			const int x3 = static_cast<int>(std::round(v3.x * scale));

			const int y1 = static_cast<int>(std::round(v1.y * scale));
			const int y2 = static_cast<int>(std::round(v2.y * scale));
			const int y3 = static_cast<int>(std::round(v3.y * scale));

			int minX = (std::min({ x1, x2, x3 }) + 0xF) >> 4;
			int maxX = (std::max({ x1, x2, x3 }) + 0xF) >> 4;
			int minY = (std::min({ y1, y2, y3 }) + 0xF) >> 4;
			int maxY = (std::max({ y1, y2, y3 }) + 0xF) >> 4;

			return FixedPointTriangleRenderInfo{ x1, x2, x3, y1, y2, y3, minX, maxX, minY, maxY };
		}
	};

	void DrawTriangleWithColorFunction(const FixedPointTriangleRenderInfo& renderInfo, const std::function<Color(float x, float y)>& colorFunction, Device::TriangleCacheItem* cacheItem)
	{
		// Implementation source: https://web.archive.org/web/20171128164608/http://forum.devmaster.net/t/advanced-rasterization/6145.
		// This is a fixed point implementation that rounds to top-left.

		const int deltaX12 = renderInfo.X1 - renderInfo.X2;
		const int deltaX23 = renderInfo.X2 - renderInfo.X3;
		const int deltaX31 = renderInfo.X3 - renderInfo.X1;

		const int deltaY12 = renderInfo.Y1 - renderInfo.Y2;
		const int deltaY23 = renderInfo.Y2 - renderInfo.Y3;
		const int deltaY31 = renderInfo.Y3 - renderInfo.Y1;

		const int fixedDeltaX12 = deltaX12 << 4;
		const int fixedDeltaX23 = deltaX23 << 4;
		const int fixedDeltaX31 = deltaX31 << 4;

		const int fixedDeltaY12 = deltaY12 << 4;
		const int fixedDeltaY23 = deltaY23 << 4;
		const int fixedDeltaY31 = deltaY31 << 4;

		const int width = renderInfo.MaxX - renderInfo.MinX;
		const int height = renderInfo.MaxY - renderInfo.MinY;
		if (width == 0 || height == 0) return;

		int c1 = deltaY12 * renderInfo.X1 - deltaX12 * renderInfo.Y1;
		int c2 = deltaY23 * renderInfo.X2 - deltaX23 * renderInfo.Y2;
		int c3 = deltaY31 * renderInfo.X3 - deltaX31 * renderInfo.Y3;

		if (deltaY12 < 0 || (deltaY12 == 0 && deltaX12 > 0)) c1++;
		if (deltaY23 < 0 || (deltaY23 == 0 && deltaX23 > 0)) c2++;
		if (deltaY31 < 0 || (deltaY31 == 0 && deltaX31 > 0)) c3++;

		int edgeStart1 = c1 + deltaX12 * (renderInfo.MinY << 4) - deltaY12 * (renderInfo.MinX << 4);
		int edgeStart2 = c2 + deltaX23 * (renderInfo.MinY << 4) - deltaY23 * (renderInfo.MinX << 4);
		int edgeStart3 = c3 + deltaX31 * (renderInfo.MinY << 4) - deltaY31 * (renderInfo.MinX << 4);

		SDL_Texture* cache = CurrentDevice->MakeTexture(width, height);
		CurrentDevice->DisableClip();
		CurrentDevice->UseAsRenderTarget(cache);

		for (int y = renderInfo.MinY; y < renderInfo.MaxY; y++)
		{
			int edge1 = edgeStart1;
			int edge2 = edgeStart2;
			int edge3 = edgeStart3;

			for (int x = renderInfo.MinX; x < renderInfo.MaxX; x++)
			{
				if (edge1 > 0 && edge2 > 0 && edge3 > 0)
				{
					CurrentDevice->SetAt(x - renderInfo.MinX, y - renderInfo.MinY, colorFunction(x + 0.5f, y + 0.5f));
				}

				edge1 -= fixedDeltaY12;
				edge2 -= fixedDeltaY23;
				edge3 -= fixedDeltaY31;
			}

			edgeStart1 += fixedDeltaX12;
			edgeStart2 += fixedDeltaX23;
			edgeStart3 += fixedDeltaX31;
		}

		CurrentDevice->UseAsRenderTarget(nullptr);
		CurrentDevice->EnableClip();

		cacheItem->Texture = cache;
		cacheItem->Width = width;
		cacheItem->Height = height;
	}

	void DrawCachedTriangle(const Device::TriangleCacheItem& triangle, const FixedPointTriangleRenderInfo& renderInfo)
	{
		const SDL_Rect destination = { renderInfo.MinX, renderInfo.MinY, triangle.Width, triangle.Height };
		SDL_RenderCopy(CurrentDevice->Renderer, triangle.Texture, nullptr, &destination);
	}

	void DrawTriangle(const ImDrawVert& v1, const ImDrawVert& v2, const ImDrawVert& v3, const Texture* texture)
	{
		// The naming inconsistency in the parameters is intentional. The fixed point algorithm wants the vertices in a counter clockwise order.
		const auto& renderInfo = FixedPointTriangleRenderInfo::CalculateFixedPointTriangleInfo(v3.pos, v2.pos, v1.pos);

		// First we check if there is a cached version of this triangle already waiting for us. If so, we can just do a super fast texture copy.

		const auto key = std::make_tuple(
			std::make_tuple(static_cast<int>(std::round(v1.pos.x)) - renderInfo.MinX, static_cast<int>(std::round(v1.pos.y)) - renderInfo.MinY, v1.uv.x, v1.uv.y, v1.col),
			std::make_tuple(static_cast<int>(std::round(v2.pos.x)) - renderInfo.MinX, static_cast<int>(std::round(v2.pos.y)) - renderInfo.MinY, v2.uv.x, v2.uv.y, v2.col),
			std::make_tuple(static_cast<int>(std::round(v3.pos.x)) - renderInfo.MinX, static_cast<int>(std::round(v3.pos.y)) - renderInfo.MinY, v3.uv.x, v3.uv.y, v3.col));

		if (CurrentDevice->GenericTriangleCache.Contains(key))
		{
			const auto& cached = CurrentDevice->GenericTriangleCache.At(key);
			DrawCachedTriangle(*cached, renderInfo);

			return;
		}

		const InterpolatedFactorEquation<float> textureU(v1.uv.x, v2.uv.x, v3.uv.x, v1.pos, v2.pos, v3.pos);
		const InterpolatedFactorEquation<float> textureV(v1.uv.y, v2.uv.y, v3.uv.y, v1.pos, v2.pos, v3.pos);

		const InterpolatedFactorEquation<Color> shadeColor(Color(v1.col), Color(v2.col), Color(v3.col), v1.pos, v2.pos, v3.pos);

		auto cached = std::make_unique<Device::TriangleCacheItem>();
		DrawTriangleWithColorFunction(renderInfo, [&](float x, float y) {
			const float u = textureU.Evaluate(x, y);
			const float v = textureV.Evaluate(x, y);
			const Color sampled = texture->Sample(u, v);
			const Color shade = shadeColor.Evaluate(x, y);

			return sampled * shade;
		}, cached.get());

		if (!cached->Texture) return;

		const SDL_Rect destination = { renderInfo.MinX, renderInfo.MinY, cached->Width, cached->Height };
		SDL_RenderCopy(CurrentDevice->Renderer, cached->Texture, nullptr, &destination);

		CurrentDevice->GenericTriangleCache.Insert(key, std::move(cached));
	}

	void DrawUniformColorTriangle(const ImDrawVert& v1, const ImDrawVert& v2, const ImDrawVert& v3)
	{
		const Color color(v1.col);

		// The naming inconsistency in the parameters is intentional. The fixed point algorithm wants the vertices in a counter clockwise order.
		const auto& renderInfo = FixedPointTriangleRenderInfo::CalculateFixedPointTriangleInfo(v3.pos, v2.pos, v1.pos);

		const auto key =std::make_tuple(v1.col,
			static_cast<int>(std::round(v1.pos.x)) - renderInfo.MinX, static_cast<int>(std::round(v1.pos.y)) - renderInfo.MinY,
			static_cast<int>(std::round(v2.pos.x)) - renderInfo.MinX, static_cast<int>(std::round(v2.pos.y)) - renderInfo.MinY,
			static_cast<int>(std::round(v3.pos.x)) - renderInfo.MinX, static_cast<int>(std::round(v3.pos.y)) - renderInfo.MinY);
		if (CurrentDevice->UniformColorTriangleCache.Contains(key))
		{
			const auto& cached = CurrentDevice->UniformColorTriangleCache.At(key);
			DrawCachedTriangle(*cached, renderInfo);

			return;
		}

		auto cached = std::make_unique<Device::TriangleCacheItem>();
		DrawTriangleWithColorFunction(renderInfo, [&color](float, float) { return color; }, cached.get());

		if (!cached->Texture) return;

		const SDL_Rect destination = { renderInfo.MinX, renderInfo.MinY, cached->Width, cached->Height };
		SDL_RenderCopy(CurrentDevice->Renderer, cached->Texture, nullptr, &destination);

		CurrentDevice->UniformColorTriangleCache.Insert(key, std::move(cached));
	}

	void DrawRectangle(const Rect& bounding, SDL_Texture* texture, int textureWidth, int textureHeight, const Color& color, bool doHorizontalFlip, bool doVerticalFlip)
	{
		// We are safe to assume uniform color here, because the caller checks it and and uses the triangle renderer to render those.

		const SDL_Rect destination = {
			static_cast<int>(bounding.MinX),
			static_cast<int>(bounding.MinY),
			static_cast<int>(bounding.MaxX - bounding.MinX),
			static_cast<int>(bounding.MaxY - bounding.MinY)
		};

		// If the area isn't textured, we can just draw a rectangle with the correct color.
		if (bounding.UsesOnlyColor())
		{
			color.UseAsDrawColor(CurrentDevice->Renderer);
			SDL_RenderFillRect(CurrentDevice->Renderer, &destination);
		}
		else
		{
			// We can now just calculate the correct source rectangle and draw it.

			const SDL_Rect source = {
				static_cast<int>(bounding.MinU * textureWidth),
				static_cast<int>(bounding.MinV * textureHeight),
				static_cast<int>((bounding.MaxU - bounding.MinU) * textureWidth),
				static_cast<int>((bounding.MaxV - bounding.MinV) * textureHeight)
			};

			const SDL_RendererFlip flip = static_cast<SDL_RendererFlip>((doHorizontalFlip ? SDL_FLIP_HORIZONTAL : 0) | (doVerticalFlip ? SDL_FLIP_VERTICAL : 0));

			SDL_SetTextureColorMod(texture, static_cast<uint8_t>(color.R * 255), static_cast<uint8_t>(color.G * 255), static_cast<uint8_t>(color.B * 255));
			SDL_RenderCopyEx(CurrentDevice->Renderer, texture, &source, &destination, 0.0, nullptr, flip);
		}
	}

	void DrawRectangle(const Rect& bounding, const Texture* texture, const Color& color, bool doHorizontalFlip, bool doVerticalFlip)
	{
		DrawRectangle(bounding, texture->Source, texture->Surface->w, texture->Surface->h, color, doHorizontalFlip, doVerticalFlip);
	}

	void DrawRectangle(const Rect& bounding, SDL_Texture* texture, const Color& color, bool doHorizontalFlip, bool doVerticalFlip)
	{
		int width, height;
		SDL_QueryTexture(texture, nullptr, nullptr, &width, &height);
		DrawRectangle(bounding, texture, width, height, color, doHorizontalFlip, doVerticalFlip);
	}
}

namespace ImGuiSDL
{
	void Initialize(SDL_Renderer* renderer, SDL_Window* window, int windowWidth, int windowHeight)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize.x = static_cast<float>(windowWidth);
		io.DisplaySize.y = static_cast<float>(windowHeight);

		ImGui::GetStyle().WindowRounding = 0.0f;
		ImGui::GetStyle().AntiAliasedFill = false;
		ImGui::GetStyle().AntiAliasedLines = false;

		// Loads the font texture.
		unsigned char* pixels;
		int width, height;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
		static constexpr uint32_t rmask = 0x000000ff, gmask = 0x0000ff00, bmask = 0x00ff0000, amask = 0xff000000;
		SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(pixels, width, height, 32, 4 * width, rmask, gmask, bmask, amask);

		Texture* texture = new Texture();
		texture->Surface = surface;
		texture->Source = SDL_CreateTextureFromSurface(renderer, surface);
		io.Fonts->TexID = (void*)texture;

		CurrentDevice = new Device(renderer);

		g_Window = window;

		// Setup backend capabilities flags
		io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;       // We can honor GetMouseCursor() values (optional)
		io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;        // We can honor io.WantSetMousePos requests (optional, rarely used)
		io.BackendPlatformName = "imgui_impl_sdl";

		// Keyboard mapping. ImGui will use those indices to peek into the io.KeysDown[] array.
		io.KeyMap[ImGuiKey_Tab] = SDL_SCANCODE_TAB;
		io.KeyMap[ImGuiKey_LeftArrow] = SDL_SCANCODE_LEFT;
		io.KeyMap[ImGuiKey_RightArrow] = SDL_SCANCODE_RIGHT;
		io.KeyMap[ImGuiKey_UpArrow] = SDL_SCANCODE_UP;
		io.KeyMap[ImGuiKey_DownArrow] = SDL_SCANCODE_DOWN;
		io.KeyMap[ImGuiKey_PageUp] = SDL_SCANCODE_PAGEUP;
		io.KeyMap[ImGuiKey_PageDown] = SDL_SCANCODE_PAGEDOWN;
		io.KeyMap[ImGuiKey_Home] = SDL_SCANCODE_HOME;
		io.KeyMap[ImGuiKey_End] = SDL_SCANCODE_END;
		io.KeyMap[ImGuiKey_Insert] = SDL_SCANCODE_INSERT;
		io.KeyMap[ImGuiKey_Delete] = SDL_SCANCODE_DELETE;
		io.KeyMap[ImGuiKey_Backspace] = SDL_SCANCODE_BACKSPACE;
		io.KeyMap[ImGuiKey_Space] = SDL_SCANCODE_SPACE;
		io.KeyMap[ImGuiKey_Enter] = SDL_SCANCODE_RETURN;
		io.KeyMap[ImGuiKey_Escape] = SDL_SCANCODE_ESCAPE;
		io.KeyMap[ImGuiKey_KeyPadEnter] = SDL_SCANCODE_KP_ENTER;
		io.KeyMap[ImGuiKey_A] = SDL_SCANCODE_A;
		io.KeyMap[ImGuiKey_C] = SDL_SCANCODE_C;
		io.KeyMap[ImGuiKey_V] = SDL_SCANCODE_V;
		io.KeyMap[ImGuiKey_X] = SDL_SCANCODE_X;
		io.KeyMap[ImGuiKey_Y] = SDL_SCANCODE_Y;
		io.KeyMap[ImGuiKey_Z] = SDL_SCANCODE_Z;

		io.SetClipboardTextFn = ImGui_SetClipboardText;
		io.GetClipboardTextFn = ImGui_GetClipboardText;
		io.ClipboardUserData = NULL;

		// Load mouse cursors
		g_MouseCursors[ImGuiMouseCursor_Arrow] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
		g_MouseCursors[ImGuiMouseCursor_TextInput] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);
		g_MouseCursors[ImGuiMouseCursor_ResizeAll] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
		g_MouseCursors[ImGuiMouseCursor_ResizeNS] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
		g_MouseCursors[ImGuiMouseCursor_ResizeEW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
		g_MouseCursors[ImGuiMouseCursor_ResizeNESW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW);
		g_MouseCursors[ImGuiMouseCursor_ResizeNWSE] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);
		g_MouseCursors[ImGuiMouseCursor_Hand] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
		g_MouseCursors[ImGuiMouseCursor_NotAllowed] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NO);

		// Check and store if we are on Wayland
		g_MouseCanUseGlobalState = strncmp(SDL_GetCurrentVideoDriver(), "wayland", 7) != 0;

		#ifdef _WIN32
			SDL_SysWMinfo wmInfo;
			SDL_VERSION(&wmInfo.version);
			SDL_GetWindowWMInfo(window, &wmInfo);
			io.ImeWindowHandle = wmInfo.info.win.window;
		#else
			(void)window;
		#endif

	}

	void Deinitialize()
	{
		// Frees up the memory of the font texture.
		ImGuiIO& io = ImGui::GetIO();
		Texture* texture = static_cast<Texture*>(io.Fonts->TexID);
		delete texture;

		delete CurrentDevice;
		g_Window = NULL;

		// Destroy last known clipboard data
		if (g_ClipboardTextData)
			SDL_free(g_ClipboardTextData);
		g_ClipboardTextData = NULL;

		// Destroy SDL mouse cursors
		for (ImGuiMouseCursor cursor_n = 0; cursor_n < ImGuiMouseCursor_COUNT; cursor_n++)
			SDL_FreeCursor(g_MouseCursors[cursor_n]);
		memset(g_MouseCursors, 0, sizeof(g_MouseCursors));

	}

	void Render(ImDrawData* drawData)
	
	{
		SDL_BlendMode blendMode;
		SDL_GetRenderDrawBlendMode(CurrentDevice->Renderer, &blendMode);
		SDL_SetRenderDrawBlendMode(CurrentDevice->Renderer, SDL_BLENDMODE_BLEND);

		Uint8 initialR, initialG, initialB, initialA;
		SDL_GetRenderDrawColor(CurrentDevice->Renderer, &initialR, &initialG, &initialB, &initialA);

		SDL_bool initialClipEnabled = SDL_RenderIsClipEnabled(CurrentDevice->Renderer);
		SDL_Rect initialClipRect;
		SDL_RenderGetClipRect(CurrentDevice->Renderer, &initialClipRect);

		SDL_Texture* initialRenderTarget = SDL_GetRenderTarget(CurrentDevice->Renderer);

		ImGuiIO& io = ImGui::GetIO();

		for (int n = 0; n < drawData->CmdListsCount; n++)
		{
			auto commandList = drawData->CmdLists[n];
			auto vertexBuffer = commandList->VtxBuffer;
			auto indexBuffer = commandList->IdxBuffer.Data;

			for (int cmd_i = 0; cmd_i < commandList->CmdBuffer.Size; cmd_i++)
			{
				const ImDrawCmd* drawCommand = &commandList->CmdBuffer[cmd_i];

				const Device::ClipRect clipRect = {
					static_cast<int>(drawCommand->ClipRect.x),
					static_cast<int>(drawCommand->ClipRect.y),
					static_cast<int>(drawCommand->ClipRect.z - drawCommand->ClipRect.x),
					static_cast<int>(drawCommand->ClipRect.w - drawCommand->ClipRect.y)
				};
				CurrentDevice->SetClipRect(clipRect);

				if (drawCommand->UserCallback)
				{
					drawCommand->UserCallback(commandList, drawCommand);
				}
				else
				{
					const bool isWrappedTexture = drawCommand->TextureId == io.Fonts->TexID;

					// Loops over triangles.
					for (unsigned int i = 0; i + 3 <= drawCommand->ElemCount; i += 3)
					{
						const ImDrawVert& v0 = vertexBuffer[indexBuffer[i + 0]];
						const ImDrawVert& v1 = vertexBuffer[indexBuffer[i + 1]];
						const ImDrawVert& v2 = vertexBuffer[indexBuffer[i + 2]];

						const Rect& bounding = Rect::CalculateBoundingBox(v0, v1, v2);

						const bool isTriangleUniformColor = v0.col == v1.col && v1.col == v2.col;
						const bool doesTriangleUseOnlyColor = bounding.UsesOnlyColor();

						// Actually, since we render a whole bunch of rectangles, we try to first detect those, and render them more efficiently.
						// How are rectangles detected? It's actually pretty simple: If all 6 vertices lie on the extremes of the bounding box,
						// it's a rectangle.
						if (i + 6 <= drawCommand->ElemCount)
						{
							const ImDrawVert& v3 = vertexBuffer[indexBuffer[i + 3]];
							const ImDrawVert& v4 = vertexBuffer[indexBuffer[i + 4]];
							const ImDrawVert& v5 = vertexBuffer[indexBuffer[i + 5]];

							const bool isUniformColor = isTriangleUniformColor && v2.col == v3.col && v3.col == v4.col && v4.col == v5.col;

							if (isUniformColor
							&& bounding.IsOnExtreme(v0.pos)
							&& bounding.IsOnExtreme(v1.pos)
							&& bounding.IsOnExtreme(v2.pos)
							&& bounding.IsOnExtreme(v3.pos)
							&& bounding.IsOnExtreme(v4.pos)
							&& bounding.IsOnExtreme(v5.pos))
							{
								// ImGui gives the triangles in a nice order: the first vertex happens to be the topleft corner of our rectangle.
								// We need to check for the orientation of the texture, as I believe in theory ImGui could feed us a flipped texture,
								// so that the larger texture coordinates are at topleft instead of bottomright.
								// We don't consider equal texture coordinates to require a flip, as then the rectangle is mostlikely simply a colored rectangle.
								const bool doHorizontalFlip = v2.uv.x < v0.uv.x;
								const bool doVerticalFlip = v2.uv.x < v0.uv.x;

								if (isWrappedTexture)
								{
									DrawRectangle(bounding, static_cast<const Texture*>(drawCommand->TextureId), Color(v0.col), doHorizontalFlip, doVerticalFlip);
								}
								else
								{
									DrawRectangle(bounding, static_cast<SDL_Texture*>(drawCommand->TextureId), Color(v0.col), doHorizontalFlip, doVerticalFlip);
								}

								i += 3;  // Additional increment to account for the extra 3 vertices we consumed.
								continue;
							}
						}

						if (isTriangleUniformColor && doesTriangleUseOnlyColor)
						{
							DrawUniformColorTriangle(v0, v1, v2);
						}
						else
						{
							// Currently we assume that any non rectangular texture samples the font texture. Dunno if that's what actually happens, but it seems to work.
							assert(isWrappedTexture);
							DrawTriangle(v0, v1, v2, static_cast<const Texture*>(drawCommand->TextureId));
						}
					}
				}

				indexBuffer += drawCommand->ElemCount;
			}
		}

		CurrentDevice->DisableClip();

		SDL_SetRenderTarget(CurrentDevice->Renderer, initialRenderTarget);

		SDL_RenderSetClipRect(CurrentDevice->Renderer, initialClipEnabled ? &initialClipRect : nullptr);

		SDL_SetRenderDrawColor(CurrentDevice->Renderer,
			initialR, initialG, initialB, initialA);

		SDL_SetRenderDrawBlendMode(CurrentDevice->Renderer, blendMode);
	}

	void NewFrame(SDL_Window* window)
	{
		ImGuiIO& io = ImGui::GetIO();
		IM_ASSERT(io.Fonts->IsBuilt() && "Font atlas not built! It is generally built by the renderer backend. Missing call to renderer _NewFrame() function? e.g. ImGui_ImplOpenGL3_NewFrame().");

		// Setup display size (every frame to accommodate for window resizing)
		int w, h;
		int display_w, display_h;
		SDL_GetWindowSize(window, &w, &h);
		if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
			w = h = 0;
		SDL_GL_GetDrawableSize(window, &display_w, &display_h);
		io.DisplaySize = ImVec2((float)w, (float)h);
		if (w > 0 && h > 0)
			io.DisplayFramebufferScale = ImVec2((float)display_w / w, (float)display_h / h);

		// Setup time step (we don't use SDL_GetTicks() because it is using millisecond resolution)
		static Uint64 frequency = SDL_GetPerformanceFrequency();
		Uint64 current_time = SDL_GetPerformanceCounter();
		io.DeltaTime = g_Time > 0 ? (float)((double)(current_time - g_Time) / frequency) : (float)(1.0f / 60.0f);
		g_Time = current_time;

		UpdateMousePosAndButtons();
		UpdateMouseCursor();

		// Update game controllers & call ImGUI
		UpdateGamepads();
		ImGui::NewFrame();
	}

	bool ProcessEvent(const SDL_Event* event)
	{
		ImGuiIO& io = ImGui::GetIO();
		switch (event->type)
		{
		case SDL_MOUSEWHEEL:
			{
				if (event->wheel.x > 0) io.MouseWheelH += 1;
				if (event->wheel.x < 0) io.MouseWheelH -= 1;
				if (event->wheel.y > 0) io.MouseWheel += 1;
				if (event->wheel.y < 0) io.MouseWheel -= 1;
				return true;
			}
		case SDL_MOUSEBUTTONDOWN:
			{
				if (event->button.button == SDL_BUTTON_LEFT) g_MousePressed[0] = true;
				if (event->button.button == SDL_BUTTON_RIGHT) g_MousePressed[1] = true;
				if (event->button.button == SDL_BUTTON_MIDDLE) g_MousePressed[2] = true;
				return true;
			}
		case SDL_TEXTINPUT:
			{
				io.AddInputCharactersUTF8(event->text.text);
				return true;
			}
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			{
				int key = event->key.keysym.scancode;
				IM_ASSERT(key >= 0 && key < IM_ARRAYSIZE(io.KeysDown));
				io.KeysDown[key] = (event->type == SDL_KEYDOWN);
				io.KeyShift = ((SDL_GetModState() & KMOD_SHIFT) != 0);
				io.KeyCtrl = ((SDL_GetModState() & KMOD_CTRL) != 0);
				io.KeyAlt = ((SDL_GetModState() & KMOD_ALT) != 0);
				io.KeySuper = ((SDL_GetModState() & KMOD_GUI) != 0);
				return true;
			}
		}
		return false;
	}

}
