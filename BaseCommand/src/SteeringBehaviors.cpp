#include "StdAfx.h"
#include "SteeringBehaviors.h"

#include "Vehicle.h"

SteeringBehaviors::SteeringBehaviors(Vehicle *vehicle)
{
	mVehicle		 = vehicle;		
	mCurrentWayPoint = 0;
	mFlags			 = none;
}

Vector3 SteeringBehaviors::seek(Vector3 destination)
{
   Vector3 tempVector = Vector3(0.0,0.0,0.0);
   tempVector = ( destination - mVehicle->position() ); 
   Vector3 desiredVelocity = tempVector.normalisedCopy() * mVehicle->maxSpeed();
   
   return ( desiredVelocity - mVehicle->velocity() ); 
}

Vector3 SteeringBehaviors::arrive(Vector3 TargetPos, int deceleration)
{
     Vector3 ToTarget = (TargetPos - mVehicle->position() );

  //setLeader the distance to the target
  double dist = (double)ToTarget.length();

  if (dist > 0)
  {
    //because Deceleration is enumerated as an int, this value is required
    //to provide fine tweaking of the deceleration..
    const double decelerationTweaker = 0.1 ; //was 0.3

    //setLeader the speed required to reach the target given the desired
    //deceleration
    double speed =  dist / ((double)deceleration * decelerationTweaker);     

    //make sure the velocity does not exceed the max

	if (speed > mVehicle->maxSpeed())
	{
		speed = mVehicle->maxSpeed();
	}

    //from here proceed just like seek except we don't need to normalize 
    //the ToTarget vector because we have already gone to the trouble
    //of calculating its length: dist. 
    Vector3 desiredVelocity =  (ToTarget * speed / dist);

    return (desiredVelocity - mVehicle->velocity());
  }

  return Vector3::ZERO;
}

Vector3 SteeringBehaviors::pursuit()
{
  Vehicle *evader = gVehicles.front();
	
  //if the evader is ahead and facing the agent then we can just seek
  //for the evader's current position.
  Vector3 ToEvader = evader->position() - mVehicle->position();

  double RelativeHeading = mVehicle->forward().dotProduct( evader->forward() );

   //dot product tells us if the headings are similar or not
  if ( (ToEvader.dotProduct(mVehicle->forward()) > 0) && (RelativeHeading < -0.95))  //acos(0.95)=18 degs
  {
    return seek(evader->position());
  }

  //Not considered ahead so we predict where the evader will be.
 
  //the lookahead time is propotional to the distance between the evader
  //and the pursuer; and is inversely proportional to the sum of the
  //agent's velocities
  double LookAheadTime = (double)ToEvader.length() / ( mVehicle->maxSpeed() + evader->speed() );
  
  //now seek to the predicted future position of the evader
  return seek(evader->position() + evader->velocity() * LookAheadTime);  
}

Vector3 SteeringBehaviors::offsetPursuit()
{
	Vehicle *leader = mLeader;

	// Point to world space
	Vector3 localPos = mVehicle->offset();	
	Matrix4 mat;	
	leader->sceneNode()->getWorldTransforms(&mat);
	Vector3 worldOffsetPos = mat * localPos;  

	Vector3 ToOffset = worldOffsetPos - mVehicle->position();

	double LookAheadTime = (double)ToOffset.length() / ( mVehicle->maxSpeed() + leader->speed() ); 
	return arrive(worldOffsetPos + leader->velocity() * LookAheadTime, 1);      
}


Vector3 SteeringBehaviors::calculate()
{
	Vector3 steeringForce = Vector3::ZERO;			

	if(on(follow_path))
	{
		steeringForce += followPath();		
	}
	else if(on(offset_pursuit))
	{
		steeringForce += offsetPursuit();
	}	

	return steeringForce;
}

Vector3 SteeringBehaviors::followPath()
{		
	if(mWayPoints[mCurrentWayPoint].distance(mVehicle->position()) < 100.0f)
	{
		mCurrentWayPoint++;
		if( mCurrentWayPoint == mWayPoints.size())
		{
			mCurrentWayPoint = 0;
		}
	}

	return seek(mWayPoints[mCurrentWayPoint]);	
}