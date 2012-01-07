#pragma once

#include "ExampleApplication.h"
#include "Vehicle.h"
#include "Fader.h"

#define ROTATESPEED 0.07f
#define FADEIN_TIME 1.0f
#define FADEOUT_TIME 1.0f
#define PLAYERHOUSES 8
#define ENABLE_DB

class BaseCommandFrameListener : public ExampleFrameListener, public OIS::MouseListener, 
	public OIS::KeyListener, public RenderTargetListener, public FaderCallback
{
protected:
	SceneManager	*mSceneMgr;
	RaySceneQuery	*mRaySceneQuery;     
	bool mLMouseDown, mRMouseDown;     // True if the mouse buttons are down

	// CEGUI variables and widgets
	CEGUI::Renderer *mGUIRenderer; 
	CEGUI::System	*mGUISystem;
	
	CEGUI::Window* mMainMenuSheet;
	CEGUI::Window* mGameOverSheet;		
	CEGUI::PushButton*  mStartButton;
	CEGUI::WindowManager *mGUIWindowMgr;

	ET::Brush mEditBrush;              // Brush for deforming the terrain

	// Bullet uses the billboard set
	BillboardSet	*mBulletBillboardSet;	

	struct Bullet 
	{
		Vector3		position;
		Vector3		velocity;
	};

	struct House
	{
		SceneNode	*sceneNode;
		SceneNode	*destroyedSceneNode;
		Vector3		position;
		bool		dead;
	};

	std::list< Bullet > mBullets;
	std::list< House >  mHouses;

	Ray		mMouseRay;
	bool	mShakeCamera;
	float	mCameraShakeTime;
	bool	mQuitting;		
	int		mKillCount;
	bool	mGameStart;		// See if game has been started

	Fader	*mFader;	
	bool	 mDoFade;
	float	mCurrentGameTime; // As it increases, so does the difficulty and the rate of spawning new planes.
	int		mBulletCount;		// Number of bullets fired	

	// For connecting to internet
	mysqlpp::Connection mMySQLConnection;


public:
	BaseCommandFrameListener(SceneManager *sceneMgr, RenderWindow* win, Camera* cam);		
	void createHole( float elapsedTime, Vector3 point );
	
protected:	

	BaseCommandFrameListener::~BaseCommandFrameListener(void);

	void initBase();

	// Derived from ExampleFrameListener
	bool frameStarted(const FrameEvent& evt);
	bool frameRenderingQueued(const FrameEvent& evt);

	// Update functions broken into smaller parts
	void updateMenuScene(float elapsedTime);
	void updateGameScene(float elapsedTime);
	void updateDifficulty(float elapsedTime);
	void updateBullets(float elapsedTime);
	void updateEnemies(float elapsedTime);	
	void updateText();

	// Helper functions
	void createEditBrush();	
	void createExplosion( Vector3 point );	

	void clearVehicles();
	void igniteTrees(Vector3 &point);	
	void gameOver();
	float getPlayerAccuracyRating();
	void populateLeaderboard();
	void ResetGame();		
	void shoot( Ray &ray );		
	void updateLightmap();

	void spawnBomber();
	void spawnPlane();
	void spawnPursuitOn();

	// CEGUI events
	bool handleQuit(const CEGUI::EventArgs& e);
	bool handleStart(const CEGUI::EventArgs& e);
	bool handleReturn(const CEGUI::EventArgs& e);	
	bool handleSend(const CEGUI::EventArgs& e);

	// Input events
	CEGUI::MouseButton convertOISMouseButtonToCegui(int buttonID);
	bool mouseMoved(const OIS::MouseEvent &arg);
	bool mousePressed(const OIS::MouseEvent &arg, OIS::MouseButtonID id);
	bool mouseReleased(const OIS::MouseEvent &arg, OIS::MouseButtonID id);
	bool keyPressed( const OIS::KeyEvent &arg );
	bool keyReleased( const OIS::KeyEvent &arg );

};