#pragma once

#include "resource.h"

#include <functional>
#include <iostream>
#include <linalg.h>
#include <limits>
#include <memory>


using namespace linalg::aliases;

static constexpr float DEFAULT_DEPTH = std::numeric_limits<float>::max();

namespace cg::renderer
{
	template<typename VB, typename RT>
	class rasterizer
	{
	public:
		rasterizer(){};
		~rasterizer(){};
		void set_render_target(
				std::shared_ptr<resource<RT>> in_render_target,
				std::shared_ptr<resource<float>> in_depth_buffer = nullptr);
		void clear_render_target(
				const RT& in_clear_value, const float in_depth = DEFAULT_DEPTH);

		void set_vertex_buffer(std::shared_ptr<resource<VB>> in_vertex_buffer);
		void set_index_buffer(std::shared_ptr<resource<unsigned int>> in_index_buffer);

		void set_viewport(size_t in_width, size_t in_height);

		void draw(size_t num_vertexes, size_t vertex_offset);

		std::function<std::pair<float4, VB>(float4 vertex, VB vertex_data)> vertex_shader;
		std::function<cg::color(const VB& vertex_data, const float z)> pixel_shader;

	protected:
		std::shared_ptr<cg::resource<VB>> vertex_buffer;
		std::shared_ptr<cg::resource<unsigned int>> index_buffer;
		std::shared_ptr<cg::resource<RT>> render_target;
		std::shared_ptr<cg::resource<float>> depth_buffer;

		size_t width = 1920;
		size_t height = 1080;

		int edge_function(int2 a, int2 b, int2 c);
		bool depth_test(float z, size_t x, size_t y);
	};

	template<typename VB, typename RT>
	inline void rasterizer<VB, RT>::set_render_target(
			std::shared_ptr<resource<RT>> in_render_target,
			std::shared_ptr<resource<float>> in_depth_buffer)
	{
		render_target = in_render_target;
		depth_buffer = in_depth_buffer;
	}

	template<typename VB, typename RT>
	inline void rasterizer<VB, RT>::set_viewport(size_t in_width, size_t in_height)
	{
		// TODO Lab: 1.02 Implement `set_render_target`, `set_viewport`, `clear_render_target` methods of `cg::renderer::rasterizer` class
		width = in_width;
    	height = in_height;
	}

	template<typename VB, typename RT>
	inline void rasterizer<VB, RT>::clear_render_target(
			const RT& in_clear_value, const float in_depth)
	{
		for (size_t i = 0; i < render_target->count(); ++i)
		{
			render_target->item(i) = in_clear_value;
		}

		if (depth_buffer)
		{
			for (size_t i = 0; i < depth_buffer->count(); ++i)
			{
				depth_buffer->item(i) = in_depth;
			}
		}
	}

	template<typename VB, typename RT>
	inline void rasterizer<VB, RT>::set_vertex_buffer(
			std::shared_ptr<resource<VB>> in_vertex_buffer)
	{
		vertex_buffer = in_vertex_buffer;
	}

	template<typename VB, typename RT>
	inline void rasterizer<VB, RT>::set_index_buffer(
			std::shared_ptr<resource<unsigned int>> in_index_buffer)
	{
		index_buffer = in_index_buffer;
	}

	template<typename VB, typename RT>
	inline void rasterizer<VB, RT>::draw(size_t num_vertexes, size_t vertex_offset)
	{
		for (size_t i = vertex_offset; i < vertex_offset + num_vertexes; i += 3)
		{
			vertex vertex0 = vertex_buffer[i];
			vertex vertex1 = vertex_buffer[i + 1];
			vertex vertex2 = vertex_buffer[i + 2];

			float4 projected_vertex0 = camera->get_projection_matrix() * camera->get_view_matrix() * float4{vertex0.position, 1.f};
			float4 projected_vertex1 = camera->get_projection_matrix() * camera->get_view_matrix() * float4{vertex1.position, 1.f};
			float4 projected_vertex2 = camera->get_projection_matrix() * camera->get_view_matrix() * float4{vertex2.position, 1.f};

			projected_vertex0 /= projected_vertex0.w;
			projected_vertex1 /= projected_vertex1.w;
			projected_vertex2 /= projected_vertex2.w;

			int2 screen_vertex0 = {
				static_cast<int>((projected_vertex0.x + 1.f) * 0.5f * width),
				static_cast<int>((1.f - projected_vertex0.y) * 0.5f * height)
			};
			int2 screen_vertex1 = {
				static_cast<int>((projected_vertex1.x + 1.f) * 0.5f * width),
				static_cast<int>((1.f - projected_vertex1.y) * 0.5f * height)
			};
			int2 screen_vertex2 = {
				static_cast<int>((projected_vertex2.x + 1.f) * 0.5f * width),
				static_cast<int>((1.f - projected_vertex2.y) * 0.5f * height)
			};

			int min_x = std::min({screen_vertex0.x, screen_vertex1.x, screen_vertex2.x});
			int max_x = std::max({screen_vertex0.x, screen_vertex1.x, screen_vertex2.x});
			int min_y = std::min({screen_vertex0.y, screen_vertex1.y, screen_vertex2.y});
			int max_y = std::max({screen_vertex0.y, screen_vertex1.y, screen_vertex2.y});

			for (int x = min_x; x <= max_x; ++x)
			{
				for (int y = min_y; y <= max_y; ++y)
				{
					int2 pixel{x, y};
					float area = edge_function(screen_vertex0, screen_vertex1, screen_vertex2);
					float w0 = edge_function(screen_vertex1, screen_vertex2, pixel) / area;
					float w1 = edge_function(screen_vertex2, screen_vertex0, pixel) / area;
					float w2 = edge_function(screen_vertex0, screen_vertex1, pixel) / area;

					if (w0 >= 0 && w1 >= 0 && w2 >= 0)
					{
						float depth = w0 * projected_vertex0.z + w1 * projected_vertex1.z + w2 * projected_vertex2.z;

						if (depth < depth_buffer->item(x, y))
						{
							depth_buffer->item(x, y) = depth;

							float3 color = w0 * vertex0.color + w1 * vertex1.color + w2 * vertex2.color;
							render_target->item(x, y) = RT::from_float3(color);
						}
					}
				}
			}
		}
	}

	template<typename VB, typename RT>
	inline int
	rasterizer<VB, RT>::edge_function(int2 a, int2 b, int2 c)
	{
		// TODO Lab: 1.05 Implement `cg::renderer::rasterizer::edge_function` method
		return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
	}

	template<typename VB, typename RT>
	inline bool rasterizer<VB, RT>::depth_test(float z, size_t x, size_t y)
	{
		if (!depth_buffer)
		{
			return true;
		}
		return depth_buffer->item(x, y) > z;
	}

}// namespace cg::renderer