/*�Copyright 2009-2012 DIVIDE-Studio�*/
/* This file is part of DIVIDE Framework.

   DIVIDE Framework is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   DIVIDE Framework is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with DIVIDE Framework.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BOUNDINGBOX_H_
#define BOUNDINGBOX_H_

#include "Core/Math/Headers/Ray.h"
#include "Utility/Headers/CRC.h"

///ToDo: -Add BoundingSphere -Ionut
class BoundingBox {
public:
	BoundingBox() : _computed(false),
				    _visibility(false),
				    _dirty(true)
	{
		_min.reset();
		_max.reset();
		_points.resize(8, vec3<F32>());
		_guid = Util::CRC32(this,sizeof(BoundingBox));
	}

	BoundingBox(const vec3<F32>& min, const vec3<F32>& max) : _computed(false),
														      _visibility(false),
															  _dirty(true),
															  _min(min),
															  _max(max)
	{
		_points.resize(8, vec3<F32>());
		_guid = Util::CRC32(this,sizeof(BoundingBox));
	}

	BoundingBox(const BoundingBox& b){
		WriteLock w_lock(_lock);
		this->_computed = b._computed;
		this->_visibility = b._visibility;
		this->_dirty = b._dirty;
		this->_min = b._min;
		this->_max = b._max;
		this->_center = b._center;
		this->_extent = b._extent;
		this->_oldMatrix = b._oldMatrix;
		this->_guid = b._guid;
		this->_points.clear();
		for_each(vec3<F32> p, b._points){
			this->_points.push_back(p);
		}
	}

	void operator=(const BoundingBox& b){
		WriteLock w_lock(_lock);
		this->_computed = b._computed;
		this->_visibility = b._visibility;
		this->_dirty = b._dirty;
		this->_min = b._min;
		this->_max = b._max;
		this->_center = b._center;
		this->_extent = b._extent;
		this->_oldMatrix = b._oldMatrix;
		this->_guid = b._guid;
		this->_points.clear();
		for_each(vec3<F32> p, b._points){
			this->_points.push_back(p);
		}
	}

	inline bool ContainsPoint(const vec3<F32>& point) const	{
		const ReadLock r_lock(_lock);
		return (point.x>=_min.x && point.y>=_min.y && point.z>=_min.z && point.x<=_max.x && point.y<=_max.y && point.z<=_max.z);
	};

	inline bool  Collision(const BoundingBox& AABB2) {

		ReadLock r_lock(_lock);

		if(_max.x < AABB2._min.x) return false;
		if(_max.y < AABB2._min.y) return false;
		if(_max.z < AABB2._min.z) return false;

		if(_min.x > AABB2._max.x) return false;
   		if(_min.y > AABB2._max.y) return false;
		if(_min.z > AABB2._max.z) return false;

		PRINT_FN("COLLISON: [ %2.2f %2.2f %2.2f - %2.2f %2.2f %2.2f] with [ %2.2f %2.2f %2.2f - %2.2f %2.2f %2.2f]",
				 _min.x,_min.y, _min.z, _max.x, _max.y, _max.z, AABB2._min.x, AABB2._min.y ,AABB2._min.z, AABB2._max.x , AABB2._max.y ,AABB2._max.z);
        return true;
	}

	inline bool Compare(const BoundingBox& bb) {return _guid == bb._guid;}

	/// Optimized method
	inline bool intersect(const Ray &r, F32 t0, F32 t1) const {
		ReadLock r_lock(_lock);

		F32 t_min, t_max, ty_min, ty_max, tz_min, tz_max;
		vec3<F32> bounds[2];
		bounds[0] = _min; bounds[1] = _max;

		t_min = (bounds[r.sign[0]].x - r.origin.x) * r.inv_direction.x;
		t_max = (bounds[1-r.sign[0]].x - r.origin.x) * r.inv_direction.x;
		ty_min = (bounds[r.sign[1]].y - r.origin.y) * r.inv_direction.y;
		ty_max = (bounds[1-r.sign[1]].y - r.origin.y) * r.inv_direction.y;

		if ( (t_min > ty_max) || (ty_min > t_max) ) 
			return false;

		if (ty_min > t_min)
			t_min = ty_min;
		if (ty_max < t_max)
			t_max = ty_max;
		tz_min = (bounds[r.sign[2]].z - r.origin.z) * r.inv_direction.z;
		tz_max = (bounds[1-r.sign[2]].z - r.origin.z) * r.inv_direction.z;
		
		if ( (t_min > tz_max) || (tz_min > t_max) )
			return false;

		if (tz_min > t_min)
			t_min = tz_min;
		if (tz_max < t_max)
			t_max = tz_max;

		return ( (t_min < t1) && (t_max > t0) );
	}
	
	inline void Add(const vec3<F32>& v)	{
		WriteLock w_lock(_lock); 
		if(v.x > _max.x)	_max.x = v.x;
		if(v.x < _min.x)	_min.x = v.x;
		if(v.y > _max.y)	_max.y = v.y;
		if(v.y < _min.y)	_min.y = v.y;
		if(v.z > _max.z)	_max.z = v.z;
		if(v.z < _min.z)	_min.z = v.z;
		_dirty = true;
	};

	inline void Add(const BoundingBox& bb)	{
		WriteLock w_lock(_lock); 
		if(bb._max.x > _max.x)	_max.x = bb._max.x;
		if(bb._min.x < _min.x)	_min.x = bb._min.x;
		if(bb._max.y > _max.y)	_max.y = bb._max.y;
		if(bb._min.y < _min.y)	_min.y = bb._min.y;
		if(bb._max.z > _max.z)	_max.z = bb._max.z;
		if(bb._min.z < _min.z)	_min.z = bb._min.z;
		_dirty = true;
	}

	inline void Translate(const vec3<F32>& v) {
		WriteLock w_lock(_lock); 
		_min += v;
		_max += v;
		_dirty = true;
	}

	inline void Multiply(F32 factor){
		WriteLock w_lock(_lock); 
		_min *= factor;
		_max *= factor;
		_dirty = true;
	}

	inline void Multiply(const vec3<F32>& v){
		WriteLock w_lock(_lock); 
		_min.x *= v.x;
		_min.y *= v.y;
		_min.z *= v.z;
		_max.x *= v.x;
		_max.y *= v.y;
		_max.z *= v.z;
		_dirty = true;
	}

	inline void MultiplyMax(const vec3<F32>& v){
		WriteLock w_lock(_lock); 
		_max.x *= v.x;
		_max.y *= v.y;
		_max.z *= v.z;
		_dirty = true;
	}

	inline void MultiplyMin(const vec3<F32>& v){
		WriteLock w_lock(_lock); 
		_min.x *= v.x;
		_min.y *= v.y;
		_min.z *= v.z;
		_dirty = true;
	}

	void ComputePoints()  {
		_points[0].set(_min.x, _min.y, _min.z);
		_points[1].set(_max.x, _min.y, _min.z);
		_points[2].set(_max.x, _max.y, _min.z);
		_points[3].set(_min.x, _max.y, _min.z);
		_points[4].set(_min.x, _min.y, _max.z);
		_points[5].set(_max.x, _min.y, _max.z);
		_points[6].set(_max.x, _max.y, _max.z);
		_points[7].set(_min.x, _max.y, _max.z);
	}

	void Transform(const BoundingBox& initialBoundingBox, const mat4<F32>& mat){
		UpgradableReadLock ur_lock(_lock); 
		if(_oldMatrix == mat){
			return;
		}else{
			_oldMatrix = mat;
			UpgradeToWriteLock uw_lock(ur_lock);

			F32 a, b;
			vec3<F32> old_min = initialBoundingBox.getMin();
			vec3<F32> old_max = initialBoundingBox.getMax();

		
			_min = _max =  vec3<F32>(mat[12],mat[13],mat[14]);

			for (U8 i = 0; i < 3; ++i)		{
				for (U8 j = 0; j < 3; ++j)			{
					a = mat.element(i,j,false) * old_min[j];
					b = mat.element(i,j,false) * old_max[j]; /// Transforms are usually row major

					if (a < b) {
						_min[i] += a;
						_max[i] += b;
					} else {
						_min[i] += b;
						_max[i] += a;
					}
				}
			}
			_dirty = true;
		}
	}


	inline bool& isComputed()		    {ReadLock r_lock(_lock); return _computed;}
	inline bool  getVisibility()		{ReadLock r_lock(_lock); return _visibility;}

	inline vec3<F32>  getMin()	     const	{ReadLock r_lock(_lock); return _min;}
	inline vec3<F32>  getMax()		 const	{ReadLock r_lock(_lock); return _max;}
	
	void clean(){
		UpgradableReadLock ur_lock(_lock);
		if(!_dirty) return;
		UpgradeToWriteLock uw_lock(ur_lock);
		_extent = (_max-_min);
		_center = (_max+_min)*0.5f;
		ComputePoints();
		_dirty = false;
	}

	inline vec3<F32>  getCenter()  {
		clean();
		ReadLock r_lock(_lock);
		return _center;
	}

	inline vec3<F32>  getExtent()  {
		clean();
		ReadLock r_lock(_lock);
		return _extent;
	}

	inline vec3<F32>  getHalfExtent()   {return getExtent() * 0.5f;}

	inline F32   getWidth()  const {ReadLock r_lock(_lock); return _max.x - _min.x;}
	inline F32   getHeight() const {ReadLock r_lock(_lock); return _max.y - _min.y;}
	inline F32   getDepth()  const {ReadLock r_lock(_lock); return _max.z - _min.z;}
	

	inline void setVisibility(bool visibility) {_visibility = visibility;}
	inline void setMin(const vec3<F32>& min)   {WriteLock w_lock(_lock); _min = min; _dirty = true;}
	inline void setMax(const vec3<F32>& max)   {WriteLock w_lock(_lock); _max = max; _dirty = true;}
	inline void set(const vec3<F32>& min, const vec3<F32>& max)  {WriteLock w_lock(_lock); _min = min; _max = max; _dirty = true;}

	inline F32 nearestDistanceFromPoint( const vec3<F32> &pos){
		const vec3<F32>& center = getCenter() ;
		const vec3<F32>& hextent = getHalfExtent();
		
		vec3<F32> nearestVec;
		nearestVec.x = Util::max( 0, fabsf( pos.x - center.x ) - hextent.x );
		nearestVec.y = Util::max( 0, fabsf( pos.y - center.y ) - hextent.y );
		nearestVec.z = Util::max( 0, fabsf( pos.z - center.z ) - hextent.z );
		
		return nearestVec.length();
	}

private:
	bool _computed, _visibility,_dirty;
	vec3<F32> _min, _max;
	vec3<F32> _center, _extent;
	mat4<F32> _oldMatrix;
	std::vector<vec3<F32> > _points;
	U32 _guid;
	mutable Lock _lock;
};

#endif


