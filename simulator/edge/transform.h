
#pragma once
#include "vector.h"

namespace edge
{
	template<typename from_traits, typename to_traits>
	struct transform
	{
		using from_type = typename from_traits::type;
		using to_type   = typename to_traits::type;

		float _11; // unit is to_type/from_type
		float _12; // unit is to_type/from_type
		float _21; // unit is to_type/from_type
		float _22; // unit is to_type/from_type
		to_type _31;
		to_type _32;

		transform() = default;

		transform (float m11, float m12, float m21, float m22, to_type m31, to_type m32)
			: _11(m11), _12(m12), _21(m21), _22(m22), _31(m31), _32(m32)
		{ }

		static transform empty() { return { 0, 0, 0, 0, 0, 0 }; }
		static transform identity()	{ return transform { 1, 0, 0, 1, 0, 0 }; }
		static transform translation (to_type x, to_type y)	{ return transform { 1, 0, 0, 1, x, y }; }
		static transform scale (float sx, float sy)	{ return transform { sx, 0, 0, sy, 0, 0 }; }
		static transform rotation (float degrees, to_type cx, to_type cy)
		{
			float radians = degrees * 3.14159265f / 180.0f;
			//float cx = rotationCenterX;
			//float cy = rotationCenterY;
			float c = cos(radians);
			float s = sin(radians);
			return { c, s, -s, c, cx - c * cx + s * cy, cy - s * cx - c * cy };
		}

		template<typename other_to_traits>
		transform<from_traits, other_to_traits> operator* (const transform<to_traits, other_to_traits>& other) const
		{
			float x11 = _11 * other._11 + _12 * other._21;
			float x12 = _11 * other._12 + _12 * other._22;
			float x21 = _21 * other._11 + _22 * other._21;
			float x22 = _21 * other._12 + _22 * other._22;
			float x31 = _31 * other._11 + _32 * other._21 + other._31;
			float x32 = _31 * other._12 + _32 * other._22 + other._32;

			return transform<from_traits, other_to_traits> { x11, x12, x21, x22, (typename other_to_traits::type) x31, (typename other_to_traits::type) x32 };
		}

		bool is_identity() const    { return (_11 == 1) && (_12 == 0) && (_21 == 0) && (_22 == 1) && (_31 == 0) && (_32 == 0); }
		bool is_empty() const       { return (_11 == 0) && (_12 == 0) && (_21 == 0) && (_22 == 0) && (_31 == 0) && (_32 == 0); }
		bool is_translation() const { return (_11 == 1) && (_12 == 0) && (_21 == 0) && (_22 == 1); }

	private:
		point<to_traits> transform_location (from_type x, from_type y) const
		{
			const auto xx = x * _11 + y * _21 + _31;
			const auto yy = x * _12 + y * _22 + _32;
			return { (to_type)xx, (to_type)yy };
		}

		size<to_traits> transform_size (from_type width, from_type height) const
		{
			const auto ww = width * _11 + height * _21;
			const auto hh = width * _12 + height * _22;
			return { (to_type)ww, (to_type)hh };
		}

	public:
		point<to_traits> transform_location (point<from_traits> location) const { return transform_location(location.x, location.y); }

		size<to_traits> transform_size (size<from_traits> size) const { return _transform_size(size.x, size.y); }

		to_type transform_length (from_type len) const { return transform_size(len, 0).length(); }
		
		std::vector<point<to_traits>> transform_locations (gsl::span<const point<from_traits>> locations) const
		{
			std::vector<point<to_traits>> res;
			for (auto& l : locations)
				res.push_back (transform_location(l));
			return res;
		}
		
		transform<to_traits, from_traits> invert() const
		{
			if (is_translation())
				return { 1, 0, 0, 1, -(from_type) _31, -(from_type) _32 };

			float det = _11 * _22 - _12 * _21;
			float invdet = 1 / det;
			auto m11 =  _22 * invdet;
			auto m12 = -_12 * invdet;
			auto m21 = -_21 * invdet;
			auto m22 =  _11 * invdet;
			auto m31 = (_21 * _32 - _31 * _22) * invdet;
			auto m32 = (_31 * _12 - _11 * _32) * invdet;
			return { m11, m12, m21, m22, (from_type) m31, (from_type) m32 };
		}
		/*
		template<typename = std::enable_if_t<std::is_same_v<from_traits, wsp> && std::is_same_v<to_traits, dip>>>
		operator D2D1_MATRIX_3X2_F() const
		{
			return { _11, _12, _21, _22, _31, _32 };
		}
		*/
	};

	using transform_wtod = transform<wsp, dip>;
	using transform_dtow = transform<dip, wsp>;
	using transform_ptod = transform<pix, dip>;
	using transform_dtop = transform<dip, pix>;
	using transform_ptow = transform<pix, wsp>;
}
