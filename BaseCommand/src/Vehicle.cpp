#include "StdAfx.h"
#include "Vehicle.h"
#include "basecommandframelistener.h"

int Vehicle::mCount = 0;

Vector3 RandomVector3(float minx, float maxx, float miny, float maxy, float minz, float maxz) 
{
	Vector3 r;
	r.x = Math::RangeRandom(minx, maxx);
	r.y = Math::RangeRandom(miny, maxy);
	r.z = Math::RangeRandom(minz, maxz );
	return r;
}

Vehicle::Vehicle(BaseCommandFrameListener *frameListener, SceneManager *sceneMgr, VEHICLE_TYPE type, Vector3 offset, Vehicle *leader) : mSteeringBehavior(this)
{
	mType = type;
	mFrameListener = frameListener;
	mSceneMgr = sceneMgr;	

	mSceneNode = mSceneMgr->getRootSceneNode()->createChildSceneNode( "planeNode" + StringConverter::toString(mCount) );
	if(mType == FIGHTER)
	{
		mEntity = mSceneMgr->createEntity( "plane"+ StringConverter::toString(mCount), "razor.mesh" );			
		mSceneNode->scale(0.5f, 0.5f, 0.5f);
	}
	else if(mType == BOMBER)
	{
		mEntity = mSceneMgr->createEntity( "plane"+ StringConverter::toString(mCount), "RZR-002.mesh" );	
		mSceneNode->scale(5.f, 5.f, 5.f);
	}	

	mSceneNode->attachObject(mEntity);
	mSceneNode->setPosition(mPosition);			
	mSceneNode->setFixedYawAxis(true);

	// Set initial properties
	mVelocity = Vector3::ZERO;	
	mMass = 1.0f;		
	mDead = false;

	if(offset != Vector3::ZERO)
	{
		mSteeringBehavior.offsetPursuitOn();
		mSteeringBehavior.setLeader(leader);			
		// Apply the same speed as the leader's
		mMaxSpeed = leader->maxSpeed();		
		mSteeringBehavior.addWaypoint(leader->position());
	}
	else
	{
		// Apply a random speed
		mMaxSpeed = Math::RangeRandom(600.0f, 1200.0f);
		mSteeringBehavior.followPathOn();		
		// Add waypoints		
		if(mType == FIGHTER)
		{
			mSteeringBehavior.addWaypoint(RandomVector3(-4000.f, 4000.f, 2500.f, 3000.f, -11000.f, -10500.f));
			mSteeringBehavior.addWaypoint(RandomVector3(-4000.f, 4000.f, 2500.f, 3000.f, -11000.f, -10500.f));
			mSteeringBehavior.addWaypoint(RandomVector3(-100.f, 1500.f, 250.f, 600.f, 400.f, 500.f));
			mSteeringBehavior.addWaypoint(RandomVector3(-100.f, 1500.f, 250.f, 600.f, 400.f, 500.f));
			mSteeringBehavior.addWaypoint(RandomVector3(-1500.f, 1000.f, 600.f, 1000.f, 1000.f, 1200.f));
			mSteeringBehavior.addWaypoint(RandomVector3(-1500.f, 1000.f, 600.f, 1000.f, 1000.f, 1200.f));
		}
		else if(mType == BOMBER)
		{
			// Bombers fly low
			mSteeringBehavior.addWaypoint(RandomVector3(-4000.f, 4000.f, 2500.f, 3000.f, -11000.f, -10500.f));
			mSteeringBehavior.addWaypoint(RandomVector3(-4000.f, 4000.f, 2500.f, 3000.f, -11000.f, -10500.f));
			mSteeringBehavior.addWaypoint(RandomVector3(-1500.f, 1500.f, 250.f, 400.f, 400.f, 500.f));
			mSteeringBehavior.addWaypoint(RandomVector3(-1500.f, 1500.f, 250.f, 400.f, 400.f, 500.f));
		}
	}
	// set current position to the first waypoint
	mPosition = mSteeringBehavior.currentDestionation();
	mOffset = offset;

	//LogManager::getSingleton().logMessage( "New plane created: " + StringConverter::toString(mPosition));
	mCount++;
	mFireTime = 0.f;
}


Vehicle::~Vehicle(void)
{
}

void Vehicle::update(float elapsedTime)
{
	updatePosition(elapsedTime);

	// update firing rate
	mFireTime += elapsedTime;

	Vector3 up = mSceneNode->_getDerivedOrientation() * Vector3::UNIT_Y;
	if(up.directionEquals(Vector3::UNIT_Y, Radian(Degree(45))))
	{
		if(mType == FIGHTER)
		{
			if(mFireTime > 1.0f)
			{
				// Only fire when facing player and close to the targets
				if(mForward.z >= 0.5f && mPosition.distance(Vector3(900.f, 0.0f, 1200.f)) < 2000.0f)
					fireMissile();			
				mFireTime = 0.0f;
			}
		}
		if(mType == BOMBER)
		{
			if(mFireTime > 0.4f)
			{
				// Bombers drop bomb even if not facing player
				if(mPosition.distance(Vector3(900.f, 0.0f, 1200.f)) < 2000.0f)
					dropBomb();
				mFireTime = 0.0f;
			}
		}
	}

	// update missiles
	std::list<Missile>::iterator list_iter;	
	for( list_iter = mMissiles.begin(); list_iter != mMissiles.end(); )
	{
		std::list<Missile>::iterator iter = list_iter;
		Missile *p = &*list_iter;
		p->update(elapsedTime);		
		*list_iter++;

		// See if missile hits the terrain
		if( p->mPosition.y <= gTerrainMgr->getTerrainInfo().getHeightAt( p->mPosition.x, p->mPosition.z ) )
		{	
			mFrameListener->createHole( elapsedTime, p->mPosition );				
			mSceneMgr->getRootSceneNode()->removeChild( p->mSceneNode ) ;												
			mMissiles.erase( iter );

		}	
	} 
}

void Vehicle::updatePosition(float elapsedTime)
{
	Vector3 steeringForce = mSteeringBehavior.calculate();

	mAcceleration = steeringForce / mMass;   

	mVelocity += mAcceleration * elapsedTime;

	// Clamp velocity
	if (mVelocity.length() > mMaxSpeed)
	{
		mVelocity.normalise();
		mVelocity *= mMaxSpeed;
	}	  

	// calculate forward and right vectors
	if(mVelocity.length() > 0.00000001)
	{
		mForward = mVelocity;
		mForward.normalise();
		mRight = mForward.crossProduct(Vector3::UNIT_Y);
	}

	mPosition += mVelocity * elapsedTime;
	mSceneNode->setPosition(mPosition);	

	// Set orientation
	Vector3 currentLookAt = mSceneNode->getOrientation( ) * Vector3::UNIT_Z;	

	Quaternion quat = Quaternion::ZERO ;
	quat = currentLookAt.getRotationTo( mForward) ;		

	Radian angle = Math::ACos((float)currentLookAt.dotProduct(mForward) );

	if((Degree(angle) > Degree(179)) )
	{
		mSceneNode->yaw(Degree(180));			
	}
	else
	{		
		mSceneNode->rotate(quat, Node::TS_PARENT);
	}
}


void Vehicle::clearMissiles()
{
	gLogMgr->logMessage( "clearing missiles....");
	std::list<Missile>::iterator list_iter;	
	for( list_iter = mMissiles.begin(); list_iter != mMissiles.end(); )
	{
		std::list<Missile>::iterator iter = list_iter;
		Missile *p = &*list_iter;
		*list_iter++;		
		mSceneMgr->getRootSceneNode()->removeChild( p->mSceneNode ) ;												
		mMissiles.erase( iter );			
		gLogMgr->logMessage( "missile removed");
	} 
}


void Vehicle::fireMissile()
{
	mCount++;

	Missile missile;

	missile.mPosition = mPosition;
	missile.mPosition.y -= 20.0f;

	missile.mEntity = mSceneMgr->createEntity( "missile" + StringConverter::toString(mCount), "Capsule.mesh" );			
	missile.mSceneNode = mSceneMgr->getRootSceneNode()->createChildSceneNode( "missileNode" + StringConverter::toString(mCount) );
	missile.mSceneNode->attachObject( missile.mEntity );
	missile.mSceneNode->setPosition( missile.mPosition);		
	missile.mSceneNode->scale(4, 4, 4);

	missile.mVelocity = Vector3::ZERO;		
	missile.mMaxSpeed = 2500.0f;

	int spot = (int)Math::RangeRandom(0.0f, 1000.0f);

	// Random target, this really needs to be improved. Basically the desired behavior is the missile should fire
	// aligned a little bit below the plane and with the same velocity as of the plane.
	missile.mTarget = Vector3(Math::RangeRandom(0.0f, 1500.0f), 0.0f, Math::RangeRandom(0.0f, 1500.0f));
	Vector3 direction = mVelocity;
	direction.y = 0.f;
	direction.normalise();
	//missile->mTarget = mPosition + (direction * 1000.f);

	mMissiles.push_back(missile);
}

void Vehicle::dropBomb()
{
	mCount++;

	Missile missile;

	missile.mPosition = mPosition;
	missile.mPosition.y -= 20.0f;

	missile.mEntity = mSceneMgr->createEntity( "bomb" + StringConverter::toString(mCount), "Capsule.mesh" );			
	missile.mSceneNode = mSceneMgr->getRootSceneNode()->createChildSceneNode( "bombNode" + StringConverter::toString(mCount) );	
	missile.mSceneNode->attachObject( missile.mEntity );
	missile.mSceneNode->setPosition( missile.mPosition);		
	missile.mSceneNode->scale(4, 4, 4);

	missile.mVelocity = Vector3::ZERO;		
	missile.mMaxSpeed = 600.0f;	

	missile.mTarget = Vector3(mPosition.x, mPosition.y - 10000.0f, mPosition.z);
	mMissiles.push_back(missile);
}

void Vehicle::kill()
{
	mPosition = Vector3(0.f, -10000.f, 0.f);
	mSceneNode->setVisible(false);
	mDead = true; 
}