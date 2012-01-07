#pragma once

#include "SteeringBehaviors.h"

class BaseCommandFrameListener;

enum VEHICLE_TYPE { FIGHTER, BOMBER };

struct Missile
{
	void update(float elapsedTime)
	{
		Vector3 tempVector = Vector3(0.0,0.0,0.0);

		tempVector = ( mTarget - mPosition );

		Vector3 desiredVelocity = tempVector.normalisedCopy() * mMaxSpeed;

		Vector3 acc = desiredVelocity - mVelocity; 

		mVelocity += acc * elapsedTime;	   

		mPosition += mVelocity * elapsedTime;

		mSceneNode->setPosition(mPosition);	

		if(mVelocity.length() > 0.00000001)
		{
			mForward = mVelocity;
			mForward.normalise();
		}

		Vector3 srcVec = mSceneNode->getOrientation( ) * Vector3::UNIT_Z;
		Quaternion quat = Quaternion::ZERO ;
		Vector3 direction = mTarget - mPosition;		
		quat = srcVec.getRotationTo( direction);

		Radian angle = Math::ACos( srcVec.dotProduct(mForward) );
		if((Degree(angle) > Degree(179)) )
		{
			mSceneNode->yaw(Degree(180));			
		}
		else
		{		
			mSceneNode->rotate(quat, Node::TS_PARENT);
		}
	}

	Vector3			mVelocity;	
	Vector3			mForward;	
	Vector3			mPosition;	
	Vector3			mTarget;
	float			mMaxSpeed;

	Entity			*mEntity; 
	SceneNode		*mSceneNode;
	SceneManager	*mSceneMgr;	
};


class Vehicle
{
public:
	Vehicle(BaseCommandFrameListener *frameListener, SceneManager *sceneMgr, VEHICLE_TYPE type, Vector3 offset = Vector3::ZERO, Vehicle *leader = NULL);
	~Vehicle(void);

	void update(float elapsedTime);
	void updatePosition(float elapsedTime);
	void clearMissiles();
	void fireMissile();
	void dropBomb();
	Entity *entity()	{ return mEntity; }
	SceneNode *sceneNode()	{ return mSceneNode; }

	void kill();

	// Accesors
	bool isDead()		{ return mDead; }
	Vector3 position()	{ return mPosition; }
	Vector3 velocity()	{ return mVelocity; }
	Vector3 offset()	{ return mOffset; }
	float maxSpeed()	{ return mMaxSpeed; }
	Vector3 forward()	{ return mForward; }
	float speed()		{ return mVelocity.length(); }	

	size_t missileCount() { return mMissiles.size(); }
	float boundingRadius()	{ return mEntity->getBoundingRadius(); }

private:
	Vector3 mVelocity;
	Vector3 mAcceleration;
	Vector3 mForward;
	Vector3 mRight;
	Vector3 mPosition;
	Vector3 mScale;	
	float mMass;
	float mMaxSpeed;
	float mRadius;
	Vector3 mOffset;

	bool mTag;

	Entity					*mEntity; 
	SceneNode				*mSceneNode;
	SceneManager			*mSceneMgr;
	BaseCommandFrameListener *mFrameListener; 

	SteeringBehaviors mSteeringBehavior;

	std::list<Missile> mMissiles;

	VEHICLE_TYPE mType;
	bool mDead;

	float mFireTime;

	static int mCount;
};
