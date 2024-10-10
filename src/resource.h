#pragma once

#include "utils/error_handler.h"

#include <algorithm>
#include <linalg.h>
#include <vector>


using namespace linalg::aliases;

namespace cg
{
	template<typename T>
	class resource
	{
	public:
		resource(size_t size);
		resource(size_t x_size, size_t y_size);
		~resource();

		const T* get_data();
		T& item(size_t item);
		T& item(size_t x, size_t y);

		size_t size_bytes() const;
		size_t count() const;
		size_t get_stride() const;

	private:
		std::vector<T> data;
		size_t item_size = sizeof(T);
		size_t stride;
	};

	template<typename T>
	inline resource<T>::resource(size_t size)
		: data(size), stride(0) {}
	
	template<typename T>
	inline resource<T>::resource(size_t x_size, size_t y_size)
		: data(x_size * y_size), stride(x_size) {}
		
	template<typename T>
	inline resource<T>::~resource()
	{
	}
	template<typename T>
	inline const T* resource<T>::get_data()
	{
		// TODO Lab: 1.02 Implement `cg::resource` class
		return data.data();
	}
	template<typename T>
	inline T& resource<T>::item(size_t item)
	{
		// TODO Lab: 1.02 Implement `cg::resource` class
		return data[item];
	}
	template<typename T>
	inline T& resource<T>::item(size_t x, size_t y)
	{
		// TODO Lab: 1.02 Implement `cg::resource` class
		return data[y * stride + x];
	}
	template<typename T>
	inline size_t resource<T>::size_bytes() const
	{
		// TODO Lab: 1.02 Implement `cg::resource` class
		return data.size() * item_size;
	}
	template<typename T>
	inline size_t resource<T>::count() const
	{
		// TODO Lab: 1.02 Implement `cg::resource` class
		return data.size();
	}

	template<typename T>
	inline size_t resource<T>::get_stride() const
	{
		// TODO Lab: 1.02 Implement `cg::resource` class
		return stride;
	}

	struct color
	{
		static color from_float3(const float3& in)
		{
			// TODO Lab: 1.02 Implement `cg::color` and `cg::unsigned_color` structs
			return color{in.x, in.y, in.z};
		};
		float3 to_float3() const
		{
			// TODO Lab: 1.02 Implement `cg::color` and `cg::unsigned_color` structs
			return float3{r, g, b};
		}
		float r;
		float g;
		float b;
	};

	struct unsigned_color
	{
		static unsigned_color from_color(const color& color)
		{
			return unsigned_color{
				static_cast<uint8_t>(std::clamp(color.r * 255.0f, 0.0f, 255.0f)),
				static_cast<uint8_t>(std::clamp(color.g * 255.0f, 0.0f, 255.0f)),
				static_cast<uint8_t>(std::clamp(color.b * 255.0f, 0.0f, 255.0f))};
		}
		static unsigned_color from_float3(const float3& color)
		{
			return unsigned_color{
				static_cast<uint8_t>(std::clamp(color.x * 255.0f, 0.0f, 255.0f)),
				static_cast<uint8_t>(std::clamp(color.y * 255.0f, 0.0f, 255.0f)),
				static_cast<uint8_t>(std::clamp(color.z * 255.0f, 0.0f, 255.0f))};
		}
		float3 to_float3() const
		{
			return float3{
				r / 255.0f,
				g / 255.0f,
				b / 255.0f};
		}
		uint8_t r;
		uint8_t g;
		uint8_t b;
	};


	struct vertex
	{
        float3 position;
        float3 normal;
        float3 color;
    };

    template<typename T>
    class resource
    {
    public:
        resource(size_t size);
        resource(size_t x_size, size_t y_size);
        ~resource();

        const T* get_data();
        T& item(size_t item);
        T& item(size_t x, size_t y);
        size_t size_bytes() const;
        size_t count() const;
        size_t get_stride() const;

    protected:
        std::vector<T> data;
        size_t stride;
        static constexpr size_t item_size = sizeof(T);
    };

}// namespace cg