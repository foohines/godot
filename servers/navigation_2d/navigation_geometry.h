#pragma once

#include "core/object/ref_counted.h"
#include "core/templates/local_vector.h"
#include "core/variant/typed_array.h"


class NavigationGeometry : public RefCounted {
	GDCLASS(NavigationGeometry, RefCounted);

	LocalVector<Vector2> vertices;
	PackedInt32Array indices; // shared CoW handle — no copy
	PackedInt32Array ranges; // shared CoW handle — no copy
	LocalVector<Rect2> aabbs;

	Rect2 _aabb_of(const PackedVector2Array &p) const;
	PackedVector2Array _get_polygon(int polygon_index) const;
	static bool _sat_overlap(const PackedVector2Array &a, const PackedVector2Array &b);
	static real_t _area(const PackedVector2Array &p);
	static Vector2 _centroid(const PackedVector2Array &p);

	void _initialize(const PackedVector2Array &p_vertices,
			const PackedInt32Array &p_indices,
			const PackedInt32Array &p_ranges,
			const Transform2D &p_xform);

protected:
	static void _bind_methods();

public:
	TypedArray<PackedVector2Array> intersect(const PackedVector2Array &p_global_polygon, real_t p_min_area = 1.0f) const;

	static Ref<NavigationGeometry> create(const PackedVector2Array &p_vertices,
			const PackedInt32Array &p_indices,
			const PackedInt32Array &p_ranges,
			const Transform2D &p_xform);
};
