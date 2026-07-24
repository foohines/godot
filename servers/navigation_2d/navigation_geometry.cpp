#include "navigation_geometry.h"

#include "core/math/geometry_2d.h"
#include "core/object/class_db.h"
#include "core/variant/typed_array.h"

void NavigationGeometry::_initialize(const PackedVector2Array &p_vertices, const PackedInt32Array &p_indices, const PackedInt32Array &p_ranges, const Transform2D &p_xform) {
	if (!p_vertices.size() || !p_indices.size() || !p_ranges.size()) {
		return;
	}

	int vertex_count = p_vertices.size();
	vertices.resize(vertex_count);
	const Vector2 *vertices_ptr = p_vertices.ptr();
	for (int i = 0; i < vertex_count; i++) {
		vertices[i] = p_xform.xform(vertices_ptr[i]);
	}

	// COW
	indices = p_indices;
	ranges = p_ranges;

	int num_polygons = ranges.size() - 1;
	aabbs.resize(num_polygons);
	const int32_t *indices_ptr = indices.ptr();
	const int32_t *ranges_ptr = ranges.ptr();
	for (int i = 0; i < num_polygons; i++) {
		Rect2 aabb;
		for (int j = ranges_ptr[i]; j < ranges_ptr[i + 1]; j++) {
			const Vector2 &vertex = vertices[indices_ptr[j]];
			aabb = (j == ranges_ptr[i]) ? Rect2(vertex, Vector2()) : aabb.expand(vertex);
		}
		aabbs[i] = aabb;
	}
}

Ref<NavigationGeometry> NavigationGeometry::create(const PackedVector2Array &p_vertices,
		const PackedInt32Array &p_indices,
		const PackedInt32Array &p_ranges,
		const Transform2D &p_xform) {
	Ref<NavigationGeometry> navigation_geometry;
	navigation_geometry.instantiate();
	navigation_geometry->_initialize(p_vertices, p_indices, p_ranges, p_xform);
	return navigation_geometry;
}

TypedArray<PackedVector2Array> NavigationGeometry::intersect(const PackedVector2Array &p_global_polygon, real_t p_min_area) const {
	const Rect2 global_polygon_aabb = _aabb_of(p_global_polygon);

	TypedArray<PackedVector2Array> intersections;
	const int nav_polygon_count = aabbs.size();
	for (int i = 0; i < nav_polygon_count; i++) {
		if (!global_polygon_aabb.intersects(aabbs[i])) {
			continue; // broadphase
		}
		const PackedVector2Array poly = _get_polygon(i);
		if (!_sat_overlap(p_global_polygon, poly)) {
			continue; // convex reject
		}
		Vector<Vector<Point2>> pieces = Geometry2D::intersect_polygons(p_global_polygon, poly);
		for (int j = 0; j < pieces.size(); j++) {
			const PackedVector2Array &piece = pieces[j];
			if (_area(piece) >= p_min_area) {
				intersections.push_back(piece);
			}
		}
	}
	return intersections;
}

Rect2 NavigationGeometry::_aabb_of(const PackedVector2Array &p) const {
	Rect2 r(p[0], Vector2());
	for (int i = 1; i < p.size(); i++) {
		r = r.expand(p[i]);
	}
	return r;
}

PackedVector2Array NavigationGeometry::_get_polygon(int polygon_index) const {
	const int32_t *indices_ptr = indices.ptr();
	const int32_t *ranges_ptr = ranges.ptr();
	const int start = ranges_ptr[polygon_index], end = ranges_ptr[polygon_index + 1];
	PackedVector2Array result;
	result.resize(end - start);
	Vector2 *result_ptrw = result.ptrw();
	for (int i = start; i < end; i++) {
		result_ptrw[i - start] = vertices[indices_ptr[i]];
	}
	return result;
}

bool NavigationGeometry::_sat_overlap(const PackedVector2Array &a, const PackedVector2Array &b) {
	const PackedVector2Array *polys[2] = { &a, &b };
	for (int pass = 0; pass < 2; pass++) {
		const PackedVector2Array &poly = *polys[pass];
		const int n = poly.size();
		for (int i = 0; i < n; i++) {
			const Vector2 p1 = poly[i];
			const Vector2 p2 = poly[(i + 1) % n];
			const Vector2 axis(p2.y - p1.y, p1.x - p2.x); // edge normal
			real_t amin = axis.dot(a[0]), amax = amin;
			for (int k = 1; k < a.size(); k++) {
				real_t d = axis.dot(a[k]);
				amin = MIN(amin, d);
				amax = MAX(amax, d);
			}
			real_t bmin = axis.dot(b[0]), bmax = bmin;
			for (int k = 1; k < b.size(); k++) {
				real_t d = axis.dot(b[k]);
				bmin = MIN(bmin, d);
				bmax = MAX(bmax, d);
			}
			if (amax < bmin || bmax < amin) {
				return false; // separating axis
			}
		}
	}
	return true;
}

real_t NavigationGeometry::_area(const PackedVector2Array &p) {
	real_t a = 0.0;
	const int n = p.size();
	for (int i = 0; i < n; i++) {
		const Vector2 &q = p[(i + 1) % n];
		a += p[i].x * q.y - q.x * p[i].y;
	}
	return Math::abs(a) * 0.5;
}

Vector2 NavigationGeometry::_centroid(const PackedVector2Array &p) {
	Vector2 c;
	real_t a = 0.0;
	const int n = p.size();
	for (int i = 0; i < n; i++) {
		const Vector2 &p0 = p[i];
		const Vector2 &p1 = p[(i + 1) % n];
		real_t cross = p0.x * p1.y - p1.x * p0.y;
		a += cross;
		c += (p0 + p1) * cross;
	}
	a *= 0.5;
	if (Math::abs(a) < 1e-5) {
		return p[0]; // degenerate fallback
	}
	return c / (6.0 * a);
}

// Sutherland-Hodgman against one axis-aligned half-plane.
// p_axis: 0 = x, 1 = y. p_keep_greater: keep points with p[axis] >= p_bound.
static PackedVector2Array _clip_axis(const PackedVector2Array &p_poly, int p_axis,
		real_t p_bound, bool p_keep_greater) {
	PackedVector2Array out;
	const int n = p_poly.size();
	for (int i = 0; i < n; i++) {
		const Vector2 &a = p_poly[i];
		const Vector2 &b = p_poly[(i + 1) % n];
		const real_t da = p_keep_greater ? a[p_axis] - p_bound : p_bound - a[p_axis];
		const real_t db = p_keep_greater ? b[p_axis] - p_bound : p_bound - b[p_axis];
		if (da >= 0.0) {
			out.push_back(a);
		}
		if (da * db < 0.0) { // strictly straddles — emit the crossing
			out.push_back(a + (b - a) * (da / (da - db)));
		}
	}
	return out;
}

// Convex ∩ rect is convex. Empty result => fewer than 3 vertices.
static PackedVector2Array _clip_to_rect(const PackedVector2Array &p_poly, const Rect2 &p_rect) {
	const Vector2 end = p_rect.position + p_rect.size;
	PackedVector2Array r = _clip_axis(p_poly, 0, p_rect.position.x, true);
	if (r.size() < 3) {
		return PackedVector2Array();
	}
	r = _clip_axis(r, 0, end.x, false);
	if (r.size() < 3) {
		return PackedVector2Array();
	}
	r = _clip_axis(r, 1, p_rect.position.y, true);
	if (r.size() < 3) {
		return PackedVector2Array();
	}
	r = _clip_axis(r, 1, end.y, false);
	return r.size() < 3 ? PackedVector2Array() : r;
}

static Vector2 _closest_on_segment(const Vector2 &p, const Vector2 &a, const Vector2 &b) {
	const Vector2 ab = b - a;
	const real_t len_sq = ab.length_squared();
	if (len_sq < 1e-12) {
		return a;
	}
	const real_t t = CLAMP((p - a).dot(ab) / len_sq, (real_t)0.0, (real_t)1.0);
	return a + ab * t;
}

// Closest navmesh point to p_origin that lies within p_rect.
// Returns (INF, INF) if the rect contains no navmesh.
Vector2 NavigationGeometry::closest_point_in_rect(const Vector2 &p_origin, const Rect2 &p_rect) const {
	real_t best_sq = INFINITY;
	Vector2 best(INFINITY, INFINITY);

	for (uint32_t i = 0; i < aabbs.size(); i++) {
		if (!p_rect.intersects(aabbs[i], true)) {
			continue; // broadphase — the rect is small, so this rejects nearly everything
		}
		const PackedVector2Array clipped = _clip_to_rect(_get_polygon(i), p_rect);
		if (clipped.is_empty()) {
			continue;
		}
		const int m = clipped.size();
		for (int j = 0; j < m; j++) {
			const Vector2 c = _closest_on_segment(p_origin, clipped[j], clipped[(j + 1) % m]);
			const real_t d = c.distance_squared_to(p_origin);
			if (d < best_sq) {
				best_sq = d;
				best = c;
			}
		}
	}
	return best;
}

void NavigationGeometry::_bind_methods() {
	ClassDB::bind_method(D_METHOD("intersect", "global_polygon", "min_area"), &NavigationGeometry::intersect, DEFVAL(1.0f));
	ClassDB::bind_method(D_METHOD("closest_point_in_rect", "origin", "rect"), &NavigationGeometry::closest_point_in_rect);
	ClassDB::bind_static_method("NavigationGeometry", D_METHOD("create", "vertices", "indices", "ranges", "xform"), &NavigationGeometry::create);
}