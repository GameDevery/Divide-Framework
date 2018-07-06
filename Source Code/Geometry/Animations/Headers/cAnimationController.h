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

#ifndef AV_SCENEANIMATOR_H
#define AV_SCENEANIMATOR_H

#include "resource.h"

class cBone {
public:

	std::string Name;
	mat4 Offset, LocalTransform, GlobalTransform, OriginalLocalTransform;
	cBone* Parent;
	std::vector<cBone*> Children;

	cBone() :Parent(0){}
	~cBone(){ for(size_t i(0); i< Children.size(); i++) delete Children[i]; }
};

struct aiVectorKey;
struct aiQuatKey;
class cAnimationChannel{
public:
	std::string Name;
	std::vector<aiVectorKey> mPositionKeys;
	std::vector<aiQuatKey> mRotationKeys;
	std::vector<aiVectorKey> mScalingKeys;
};

struct aiAnimation;
class cAnimEvaluator{
public:

	cAnimEvaluator(): mLastTime(0.0f), TicksPerSecond(0.0f), Duration(0.0f), PlayAnimationForward(true) {}
	cAnimEvaluator( const aiAnimation* pAnim);
	void Evaluate( float pTime, std::map<std::string, cBone*>& bones);
	void Save(std::ofstream& file);
	void Load(std::ifstream& file);
	std::vector<mat4>& GetTransforms(float dt){ return Transforms[GetFrameIndexAt(dt)]; }
	unsigned int GetFrameIndexAt(float time);

	std::string Name;
	std::vector<cAnimationChannel> Channels;
	bool PlayAnimationForward;///< play forward == true, play backward == false
	float mLastTime, TicksPerSecond, Duration;	
	std::vector<vec3> mLastPositions;
	std::vector<std::vector<mat4>> Transforms;//, QuatTransforms;/**< Array to return transformations results inside. */
};

struct aiScene;
struct aiNode;
class SceneAnimator{
public:

	SceneAnimator(): Skeleton(0), CurrentAnimIndex(-1) {}
	~SceneAnimator(){ Release(); }

	void Init(const aiScene* pScene);///< this must be called to fill the SceneAnimator with valid data
	void Release();///< frees all memory and initializes everything to a default state
	void Save(std::ofstream& file);
	void Load(std::ifstream& file);
	bool HasSkeleton() const { return !Bones.empty(); }///< lets the caller know if there is a skeleton present
	/// the set animation returns whether the animation changed or is still the same. 
	bool SetAnimIndex( I32 pAnimIndex);///< this takes an index to set the current animation to
	bool SetAnimation(const std::string& name);///< this takes a string to set the animation to, i.e. SetAnimation("Idle");
	/// the next two functions are good if you want to change the direction of the current animation. You could use a forward walking animation and reverse it to get a walking backwards
	void PlayAnimationForward() { Animations[CurrentAnimIndex].PlayAnimationForward = true; }
	void PlayAnimationBackward() { Animations[CurrentAnimIndex].PlayAnimationForward = false; }
	///this function will adjust the current animations speed by a percentage. So, passing 100, would do nothing, passing 50, would decrease the speed by half, and 150 increase it by 50%
	void AdjustAnimationSpeedBy(float percent) { Animations[CurrentAnimIndex].TicksPerSecond*=percent/100.0f; }
	///This will set the animation speed
	void AdjustAnimationSpeedTo(float tickspersecond) { Animations[CurrentAnimIndex].TicksPerSecond=tickspersecond; }
	/// get the animationspeed...
	float GetAnimationSpeed() const { return Animations[CurrentAnimIndex].TicksPerSecond; }
	/// get the transforms needed to pass to the vertex shader. This will wrap the dt value passed, so it is safe to pass 50000000 as a valid number
	std::vector<mat4>& GetTransforms(float dt){ return Animations[CurrentAnimIndex].GetTransforms(dt); }

	I32 GetAnimationIndex() const { return CurrentAnimIndex; }
	std::string GetAnimationName() const { return Animations[CurrentAnimIndex].Name;  }

	std::vector<cAnimEvaluator> Animations;///< a std::vector that holds each animation 
	I32 CurrentAnimIndex;/**< Current animation index */

protected:		

	
	cBone* Skeleton;/**< Root node of the internal scene structure */
	std::map<std::string, cBone*> BonesByName;/**< Name to node map to quickly find nodes by their name */
	std::map<std::string, U32> AnimationNameToId;// find animations quickly
	std::vector<cBone*> Bones;///< DO NOT DELETE THESE when the destructor runs... THEY ARE JUST COPIES!!
	std::vector<mat4> Transforms;///< temp array of transfrorms

	void SaveSkeleton(std::ofstream& file, cBone* pNode);
	cBone* LoadSkeleton(std::ifstream& file, cBone* pNode);

	void UpdateTransforms(cBone* pNode);
	void CalculateBoneToWorldTransform(cBone* pInternalNode);/**< Calculates the global transformation matrix for the given internal node */

	void Calculate( float pTime);
	void CalcBoneMatrices();

	void ExtractAnimations(const aiScene* pScene);
	cBone* CreateBoneTree( aiNode* pNode, cBone* pParent);///< Recursively creates an internal node structure matching the current scene and animation. 
	
};


#endif