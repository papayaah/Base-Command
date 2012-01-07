#pragma once

class Vehicle;

enum BEHAVIOR_TYPE
{
	none = 0x00000,
	seek = 0x00002,
	follow_path = 0x00004,
	arrive = 0x00008,
	pursuit = 0x00010,
	offset_pursuit = 0x00020,
};	

class SteeringBehaviors
{
public:
	SteeringBehaviors(Vehicle *vehicle);	

	Vector3 calculate();

	void followPathOn()		{mFlags |= follow_path;}
	void offsetPursuitOn()	{mFlags |= offset_pursuit; }  

	Vector3 currentDestionation() { return mWayPoints[mCurrentWayPoint]; }
	void addWaypoint(Vector3 pos) { mWayPoints.push_back(pos); }

	void setLeader( Vehicle* leader ) { mLeader = leader; }


private:
	Vector3 seek(Vector3 destination);	
	Vector3 followPath();
	Vector3 pursuit();
	Vector3 offsetPursuit();

	Vector3 arrive(Vector3 TargetPos, int deceleration);

	// This function tests if a specific bit of mFlags is set
	bool on(BEHAVIOR_TYPE bt){return (mFlags & bt) == bt;}	

	int           mFlags;
	Vehicle *mVehicle;
	Vehicle *mLeader;
	std::vector<Vector3> mWayPoints;
	int mCurrentWayPoint;
};
