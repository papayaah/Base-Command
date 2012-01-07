#include "stdafx.h"
#include "BaseCommandFrameListener.h"

BaseCommandFrameListener::BaseCommandFrameListener(SceneManager *sceneMgr, RenderWindow* win, Camera* cam)
: ExampleFrameListener(win, cam, true, true), mSceneMgr(sceneMgr), mMySQLConnection(false), mFader(NULL)
{
	// Initialize CEGUI		
	mGUIRenderer = new CEGUI::OgreCEGUIRenderer(win, Ogre::RENDER_QUEUE_OVERLAY, false, 3000, mSceneMgr);
	mGUISystem = new CEGUI::System(mGUIRenderer);		
	CEGUI::SchemeManager::getSingleton().loadScheme((CEGUI::utf8*)"TaharezLookSkin.scheme");
	mGUISystem->setDefaultMouseCursor((CEGUI::utf8*)"TaharezLook", (CEGUI::utf8*)"MouseArrow");
	mGUISystem->setDefaultFont((CEGUI::utf8*)"BlueHighway-12");

	mGUIWindowMgr = CEGUI::WindowManager::getSingletonPtr();

	// Load the 2 layouts created for this game
	mMainMenuSheet = CEGUI::WindowManager::getSingleton().loadWindowLayout((CEGUI::utf8*)"mainmenu.layout"); 
	mGameOverSheet = CEGUI::WindowManager::getSingleton().loadWindowLayout((CEGUI::utf8*)"gameover.layout"); 
	mGUISystem->setGUISheet(mMainMenuSheet);	   												

#ifdef ENABLE_DB
	// Try to connect to DB
	gLogMgr->logMessage( "Connecting to database..." );		
	if (mMySQLConnection.connect("pmind_basecommand", "www.programmingmind.com", "pmind_basecomman", "letmein")) 
	{
		gLogMgr->logMessage( "Successfully connected to remote database." ); 
		populateLeaderboard();			
	}   
	else
	{
		gLogMgr->logMessage( "Failed to connect to remote database." ); 
		CEGUI::Listbox *leaderboardListbox = static_cast<CEGUI::Listbox*>(mGUIWindowMgr->getWindow((CEGUI::utf8*)"MainMenu/LeaderboardsListbox"));	
		CEGUI::ListboxTextItem *item = new CEGUI::ListboxTextItem((CEGUI::utf8*)"Failed to connect to database to retrieve high score. Scores will not be save for this session.");				
		leaderboardListbox->addItem(item);
	}
#endif
	// Main menu widgets	   
	
	mGUIWindowMgr->getWindow((CEGUI::utf8*)"MainMenu/StartButton")->subscribeEvent(CEGUI::PushButton::EventClicked, 
		CEGUI::Event::Subscriber(&BaseCommandFrameListener::handleStart, this));	   
	mGUIWindowMgr->getWindow((CEGUI::utf8*)"MainMenu/QuitButton")->subscribeEvent(CEGUI::PushButton::EventClicked, 
		CEGUI::Event::Subscriber(&BaseCommandFrameListener::handleQuit, this));	   

	// Game over layout widgets
	mGUIWindowMgr->getWindow((CEGUI::utf8*)"GameOver/SendButton")->subscribeEvent(CEGUI::PushButton::EventClicked, 
		CEGUI::Event::Subscriber(&BaseCommandFrameListener::handleSend, this));	   
	mGUIWindowMgr->getWindow((CEGUI::utf8*)"GameOver/ReturnButton")->subscribeEvent(CEGUI::PushButton::EventClicked, 
		CEGUI::Event::Subscriber(&BaseCommandFrameListener::handleReturn, this));	   
		
	mMouse->setEventCallback( this );	 		
	mKeyboard->setEventCallback(this);

	mRaySceneQuery = mSceneMgr->createRayQuery(Ray());			

	// Init bullet billboard set
	mBulletBillboardSet = mSceneMgr->createBillboardSet("bullets" + StringConverter::toString(mSceneMgr->getRootSceneNode()->numChildren()), 1);		
	mBulletBillboardSet->setMaterialName("Examples/Flare");		
	mSceneMgr->getRootSceneNode()->attachObject( mBulletBillboardSet );		

	initBase();
	createEditBrush();	

	// Create a fader instance so we can have a simple fade in/out transition between scenes
	mFader = new Fader("Overlays/FadeInOut", "Materials/OverlayMaterial", this);		

	// Initialize starting game properties		
	mShakeCamera	= false;		
	mGameStart		= false;
	mDoFade			= false;		
	mQuitting		= false;	

	// Don't show the default ogre debug overlay
	showDebugOverlay(false);

}	  

BaseCommandFrameListener::~BaseCommandFrameListener(void)
{
	SAFE_DELETE(mGUISystem);
	SAFE_DELETE(mGUIRenderer);		
	SAFE_DELETE(mFader);
	SAFE_DELETE(mRaySceneQuery);		

}

void BaseCommandFrameListener::initBase()
{
	// position 8 houses
	int numHouses = 0;
	while(numHouses < PLAYERHOUSES)
	{
		// Specific area where to place the house
		float posx = Math::RangeRandom( 500.f, 900.f );
		float posz = Math::RangeRandom( 400.f, 800.f );			
		float posy = gTerrainMgr->getTerrainInfo().getHeightAt( posx, posz );
		Vector3 position(posx, posy, posz);

		int placeok = true;
		// Find a place to put the house
		for( std::list<House>::iterator list_iter = mHouses.begin(); list_iter != mHouses.end(); )
		{				
			House *p = &*list_iter;
			if(position.distance( p->position ) < 120 )						
			{
				placeok = false;
				break;
			}			
			list_iter++;								
		}


		if(placeok)
		{
			Entity *houseEntity = mSceneMgr->createEntity( "house" + StringConverter::toString(numHouses), "DestructibleHouse.mesh");
			Entity *houseruins = mSceneMgr->createEntity( "houseruin" + StringConverter::toString(numHouses), "DestructibleHouseRuins.mesh");

			House house;				
			house.sceneNode = mSceneMgr->getRootSceneNode()->createChildSceneNode("housenode" + StringConverter::toString(numHouses));
			house.sceneNode->attachObject( houseEntity );			
			house.sceneNode->setPosition( position );
			house.sceneNode->pitch( -Radian(Math::HALF_PI) );
			house.sceneNode->scale(5.f, 5.f, 5.f);
			house.sceneNode->roll( Radian(Math::RangeRandom( 0.0f, Math::TWO_PI )) );

			house.destroyedSceneNode = mSceneMgr->getRootSceneNode()->createChildSceneNode("housenoderuin"+ StringConverter::toString(numHouses));
			house.destroyedSceneNode->attachObject( houseruins );	
			house.destroyedSceneNode->setPosition( house.sceneNode->getPosition() );
			house.destroyedSceneNode->rotate( house.sceneNode->getOrientation() );
			house.destroyedSceneNode->scale( house.sceneNode->getScale() );
			house.destroyedSceneNode->setVisible(false);

			house.position = position;
			house.dead = false;		

			mHouses.push_back(house);			
			numHouses++;
		}
	}
}

bool BaseCommandFrameListener::frameRenderingQueued(const FrameEvent& evt)
{
	if( ExampleFrameListener::frameRenderingQueued(evt) == false )
		return false;

	if( mKeyboard->isKeyDown(OIS::KC_ESCAPE) || mQuitting )
	{
		// Dump fps before quitting			
		const RenderTarget::FrameStats& stats = mWindow->getStatistics();
		static String currFps = "Current FPS: ";
		static String avgFps = "Average FPS: ";
		static String bestFps = "Best FPS: ";
		static String worstFps = "Worst FPS: ";			
		gLogMgr->logMessage( "------Game FPS statistics------" );
		gLogMgr->logMessage( avgFps + StringConverter::toString(stats.avgFPS) );
		gLogMgr->logMessage( currFps + StringConverter::toString(stats.lastFPS) );
		gLogMgr->logMessage( bestFps + StringConverter::toString(stats.bestFPS) + " " + StringConverter::toString(stats.bestFrameTime) + " ms" );
		gLogMgr->logMessage( worstFps + StringConverter::toString(stats.worstFPS) );
		return false;
	}

	return true;
}

bool BaseCommandFrameListener::frameStarted(const FrameEvent& evt)
{
	bool ret = ExampleFrameListener::frameStarted(evt);

	
	if( gCaelumSystem )
	{
		ColourValue orange(0.8f, 0.5f, 0.2f);
		ColourValue orange2(0.9f, 0.6f, 0.3f);
		gCaelumSystem->getCloudSystem()->getLayer(0)->update(  evt.timeSinceLastFrame * 140.f, Vector3(0.f, -1.f, 0.f), orange, orange, orange);
		//gCaelumSystem->getCloudSystem()->getLayer(1)->update(  evt.timeSinceLastFrame * 80.f, Vector3(0.f, -1.f, 0.f), orange2, orange2, orange2);
	}		


	// Updated paged geometry
	if(gTrees)  gTrees->update();
	if(gBushes) gBushes->update();
	if(gGrass)  gGrass->update();		

	// update terrain lightmap every 5 seconds		
	static float lightmapsec = 0.0f;
	lightmapsec += evt.timeSinceLastFrame;
	if(lightmapsec > 5.0f )
	{
		updateLightmap();
		lightmapsec = 0.f;
	}

	// Update game scene or menu scene
	if(mGameStart)
	{
		updateGameScene(evt.timeSinceLastFrame);
	}
	else
	{
		updateMenuScene(evt.timeSinceLastFrame);
	}

	// Fade in / out
	if( mDoFade )
	{
		static float fadeInTime = 0.f;
		fadeInTime += evt.timeSinceLastFrame;
		// Check if we need to start the game
		if(fadeInTime > FADEOUT_TIME)
		{
			// Start game, fade out
			mFader->startFadeIn(FADEIN_TIME);					
			ResetGame();
			mDoFade = false;								
		}			
	}
	mFader->fade(evt.timeSinceLastFrame);

	// Do camera shake if needed. Triggered when a house is destroyed
	mCameraShakeTime += evt.timeSinceLastFrame;
	if(mShakeCamera && mCameraShakeTime < 0.3f)
	{
		mCamera->pitch( Radian(Math::RangeRandom(-.01f, 0.01f)));			
	}	

	// Set camera height to terrain height
	//Vector3 camPos = mCamera->getPosition();
	//float terrainHeight = gTerrainMgr->getTerrainInfo().getHeightAt( camPos.x, camPos.z );
	//mCamera->setPosition( camPos.x, terrainHeight + 10.f, camPos.z );			

	return ret;
}


void BaseCommandFrameListener::clearVehicles()
{
	for( std::list<Vehicle*>::iterator list_iter = gVehicles.begin(); list_iter != gVehicles.end(); )
	{				
		std::list<Vehicle*>::iterator iter = list_iter;
		Vehicle *p = &**list_iter;
		list_iter++;
		mSceneMgr->getRootSceneNode()->removeChild( p->sceneNode() ) ;												
		gVehicles.erase( iter );
	}
	gVehicles.clear();
}

void BaseCommandFrameListener::spawnBomber()
{
	Vehicle *vehicle = new Vehicle(this, mSceneMgr, BOMBER);		
	gVehicles.push_back(vehicle);
}

void BaseCommandFrameListener::spawnPlane()
{
	Vehicle *vehicle = new Vehicle(this, mSceneMgr, FIGHTER);		
	gVehicles.push_back(vehicle);
}

void BaseCommandFrameListener::spawnPursuitOn()
{
	Vehicle *leader = new Vehicle(this, mSceneMgr, FIGHTER);		
	gVehicles.push_back(leader);
	gVehicles.push_back(new Vehicle(this, mSceneMgr, FIGHTER, Vector3(100.0f, 0.f, 0.f), leader));
	gVehicles.push_back(new Vehicle(this, mSceneMgr, FIGHTER, Vector3(-100.0f, 0.f, 0.f), leader));		

	//gVehicles.push_back(new Vehicle(this, mSceneMgr, FIGHTER, Vector3(-150.0f, -550.0f, 500.f), leader));
	//gVehicles.push_back(new Vehicle(this, mSceneMgr, FIGHTER, Vector3(150.0f, -550.0f, 500.f), leader));
	//gVehicles.push_back(new Vehicle(this, mSceneMgr, FIGHTER, Vector3(0.0f, -550.0f, 500.f), leader));
}

//----------------------------------------------------------------//
CEGUI::MouseButton BaseCommandFrameListener::convertOISMouseButtonToCegui(int buttonID)
{
	switch (buttonID)
	{
	case 0: return CEGUI::LeftButton;
	case 1: return CEGUI::RightButton;
	case 2:	return CEGUI::MiddleButton;
	case 3: return CEGUI::X1Button;
	default: return CEGUI::LeftButton;
	}
}

bool BaseCommandFrameListener::mouseMoved(const OIS::MouseEvent &arg)
{
	if(mGameStart)
	{			
		// Fix the mouse cursor in the center of the screen
		CEGUI::System::getSingleton().injectMousePosition(mWindow->getWidth() / 2.f, mWindow->getHeight() / 2.f);

		// Move the camera as the mouse moves
		mCamera->yaw(Degree(-arg.state.X.rel * ROTATESPEED));
		mCamera->pitch(Degree(-arg.state.Y.rel * ROTATESPEED));

		// Rotate the turret with the camera
		mSceneMgr->getSceneNode( "turretnode" )->roll( Degree(-arg.state.X.rel * ROTATESPEED) );			
		mSceneMgr->getSceneNode( "gunnode" )->yaw( Degree(arg.state.Y.rel * ROTATESPEED) );

		// If we are dragging the left mouse button.
		if (mLMouseDown)
		{
		} 
		// If we are dragging the right mouse button.
		else if (mRMouseDown)
		{				
		} 			

		CEGUI::Point mousePos = CEGUI::MouseCursor::getSingleton().getPosition();
		mMouseRay = mCamera->getCameraToViewportRay(mousePos.d_x/float(arg.state.width), mousePos.d_y/float(arg.state.height));
	}		
	else
	{
		// The game has not been started yet, so player can move the mouse cursor freely around the screen
		CEGUI::System::getSingleton().injectMouseMove((float)arg.state.X.rel * 0.5f, (float)arg.state.Y.rel * 0.5f);		
	}
	return true;
}

bool BaseCommandFrameListener::mousePressed(const OIS::MouseEvent &arg, OIS::MouseButtonID id)
{
	CEGUI::System::getSingleton().injectMouseButtonDown(convertOISMouseButtonToCegui(id));

	if(mGameStart)
	{
		// Fire weapon
		if (id == OIS::MB_Left)
		{
			// Setup the ray scene query, use CEGUI's mouse position
			CEGUI::Point mousePos = CEGUI::MouseCursor::getSingleton().getPosition();
			Ray mouseRay = mCamera->getCameraToViewportRay(mousePos.d_x/float(arg.state.width), mousePos.d_y/float(arg.state.height));

			mLMouseDown = true;	
			shoot( mouseRay );								
		}			
	}

	return true;
}

bool BaseCommandFrameListener::mouseReleased(const OIS::MouseEvent &arg, OIS::MouseButtonID id)
{
	CEGUI::System::getSingleton().injectMouseButtonUp(convertOISMouseButtonToCegui(id));

	// Left mouse button up
	if (id == OIS::MB_Left)
	{
		mLMouseDown = false;
	} 

	// Right mouse button up
	else if (id == OIS::MB_Right)
	{			
		mRMouseDown = false;
	} // else if


	return true;
}


//----------------------------------------------------------------//
bool BaseCommandFrameListener::keyPressed( const OIS::KeyEvent &arg )
{
	CEGUI::System::getSingleton().injectKeyDown( arg.key );
	CEGUI::System::getSingleton().injectChar( arg.text );
	return true;
}

//----------------------------------------------------------------//
bool BaseCommandFrameListener::keyReleased( const OIS::KeyEvent &arg )
{
	CEGUI::System::getSingleton().injectKeyUp( arg.key );
	return true;
}

void BaseCommandFrameListener::updateMenuScene(float elapsedTime)
{
	static float nextSpawn = 0.0f;
	nextSpawn += elapsedTime;
	if(nextSpawn > 1.0f && gVehicles.size() < 12)		
	{
		nextSpawn = 0.f;
		spawnPlane();
		spawnBomber();		
		spawnPursuitOn();
	}

	std::list<Vehicle*>::iterator list_iter;	
	for( list_iter = gVehicles.begin(); list_iter != gVehicles.end(); )
	{				
		Vehicle *p = &**list_iter;
		list_iter++;	
		p->updatePosition(elapsedTime);
	}

	if( mKeyboard->isKeyDown(OIS::KC_F5) && mTimeUntilNextToggle <= 0 )
	{
		gameOver();
	}	
}

void BaseCommandFrameListener::updateGameScene(float elapsedTime)
{
	// Increase current game time
	mCurrentGameTime += elapsedTime;

	updateDifficulty(elapsedTime);
	updateEnemies(elapsedTime);
	updateBullets(elapsedTime);
	updateText();
}

void BaseCommandFrameListener::updateBullets(float elapsedTime)
{
	std::list<Bullet>::iterator list_iter;	
	int i = 0;
	for( list_iter = mBullets.begin(); list_iter != mBullets.end(); )
	{				
		std::list<Bullet>::iterator iter = list_iter;
		list_iter++;

		Bullet *bullet = &*iter;		
		bullet->position += bullet->velocity * elapsedTime;

		mBulletBillboardSet->getBillboard( i )->setPosition( bullet->position );															

		// Check if a plane has been hit
		bool hit = false;
		std::list<Vehicle*>::iterator list_iter;	
		for( list_iter = gVehicles.begin(); list_iter != gVehicles.end(); )
		{				
			Vehicle *p = &**list_iter;
			list_iter++;
			if( p->entity()->getWorldBoundingBox().contains( bullet->position ))
			{
#ifdef USE_PARTICLEUNIVERSE
				createExplosion( p->position() );												
#endif
				// Set for removal
				p->kill();																							
				hit = true;	
				mBullets.erase( iter );					
				mBulletBillboardSet->removeBillboard( i );							
				mKillCount++;
				break;
			}				
		}

		if(hit)
			continue;

		// Remove out of bounds bullet
		if( bullet->position.x > 10000.f || bullet->position.x < -10000.f ||
			bullet->position.y > 10000.f || bullet->position.y < -10000.f ||
			bullet->position.z > 10000.f || bullet->position.z < -10000.f
			)
		{					
			mBullets.erase( iter );					
			mBulletBillboardSet->removeBillboard( i );					
			continue;
		}												

		// Check if hit terrain						
		if( bullet->position.y <= gTerrainMgr->getTerrainInfo().getHeightAt( bullet->position.x, bullet->position.z ) )
		{					
			mBullets.erase( iter );	
			mBulletBillboardSet->removeBillboard( i );										
			continue;
		}								
		i++;
	}

}

void BaseCommandFrameListener::updateDifficulty(float elapsedTime)
{
	static float nextSpawn = 0.0f;
	nextSpawn += elapsedTime;

	// Spawn enemies depending on how long the player have been playing
	if(mCurrentGameTime > 180.f) 
	{
		// After 3 minutes
		if(nextSpawn > 2.f && gVehicles.size() < 50)		
		{
			nextSpawn = 0.f;
			spawnPlane();
			spawnBomber();	
			spawnPursuitOn();
		}
	}
	else if(mCurrentGameTime > 120.f) 
	{
		if(nextSpawn > 2.f )		
		{
			nextSpawn = 0.f;
			spawnPlane();
			spawnBomber();			
		}
	}
	else if(mCurrentGameTime > 60.f) 
	{
		if(nextSpawn > 3.f)		
		{
			nextSpawn = 0.f;
			spawnPlane();
			spawnBomber();			
		}
	}
	else
	{
		if(nextSpawn > 4.f)		
		{
			nextSpawn = 0.f;
			spawnPlane();
			spawnBomber();			
		}
	}	
}

void BaseCommandFrameListener::updateEnemies(float elapsedTime)
{


	std::list<Vehicle*>::iterator list_iter;	
	for( list_iter = gVehicles.begin(); list_iter != gVehicles.end(); )
	{				
		std::list<Vehicle*>::iterator iter = list_iter;
		Vehicle *p = &**list_iter;
		list_iter++;
		// If registered to die, remove the scene sceneNode
		if(p->isDead() && p->missileCount() == 0)
		{
			mSceneMgr->getRootSceneNode()->removeChild( p->sceneNode() ) ;												
			gVehicles.erase( iter );
		}
		else
		{			
			p->update(elapsedTime);							
		}
	}
}



void BaseCommandFrameListener::updateText()
{
	try 
	{
		// Simply using OGRE's default debug overlay to debug in game variables		

		OverlayElement* guiAvg = OverlayManager::getSingleton().getOverlayElement("Core/AverageFps");
		OverlayElement* guiCurr = OverlayManager::getSingleton().getOverlayElement("Core/CurrFps");
		OverlayElement* guiBest = OverlayManager::getSingleton().getOverlayElement("Core/BestFps");
		OverlayElement* guiWorst = OverlayManager::getSingleton().getOverlayElement("Core/WorstFps");

		OverlayElement* guiTris = OverlayManager::getSingleton().getOverlayElement("Core/NumTris");
		OverlayElement* guiBatches = OverlayManager::getSingleton().getOverlayElement("Core/NumBatches");
		OverlayElement* guiDbg = OverlayManager::getSingleton().getOverlayElement("Core/DebugText");

		guiAvg->setCaption(StringConverter::toString(mCurrentGameTime));					
		guiCurr->setCaption("kill count: " + StringConverter::toString(mKillCount));
		guiBest->setCaption("Bullet count: " + StringConverter::toString(mBulletCount));				
	}
	catch(...) { /* ignore */ }
}

void BaseCommandFrameListener::populateLeaderboard()
{
	CEGUI::Listbox *leaderboardListbox = static_cast<CEGUI::Listbox*>(mGUIWindowMgr->getWindow((CEGUI::utf8*)"MainMenu/LeaderboardsListbox"));	

	while(leaderboardListbox->getItemCount())
	{
		// There are too many items within the history Listbox, purging them one at a time
		CEGUI::ListboxItem* item = leaderboardListbox->getListboxItemFromIndex(0);
		leaderboardListbox->removeItem(item);
	}

	if(mMySQLConnection.connected())
	{
		mysqlpp::Query query = mMySQLConnection.query("SELECT name, kills, FORMAT(duration / 60, 2) AS duration, FORMAT(accuracy, 1) as accuracy, DATE_FORMAT(createdon, '%h:%i %p %d %b %Y') AS createdon FROM scores ORDER BY kills DESC, accuracy DESC, duration DESC, createdon DESC");	
		if(mysqlpp::UseQueryResult res = query.use())
		{
			while (mysqlpp::Row row = res.fetch_row()) 
			{
				CEGUI::String str;
				str.append(row["name"]);
				/*
				for(int i = str.size(); i < 20; i++)
				{
				str.append(" ");
				}
				*/
				str.append(" - ");
				str.append(row["kills"]);
				str.append(" kills in ");
				str.append(row["duration"]);			  
				str.append(" minutes on ");			  
				str.append(row["createdon"]);			  
				str.append(" with a ");			  
				str.append(row["accuracy"]);			  			  
				str.append(" % accuracy.");		
				CEGUI::ListboxTextItem *item = new CEGUI::ListboxTextItem(str, 5);				
				leaderboardListbox->addItem(item);
			}
		} 
	}
	else
	{
		CEGUI::ListboxTextItem *item = new CEGUI::ListboxTextItem((CEGUI::utf8*)"Currently not connected to the database.");				
		leaderboardListbox->addItem(item);
	}
}

void BaseCommandFrameListener::shoot( Ray &ray )
{
	// Create the graphic representation of the plasma
	Vector3 camdir = ray.getDirection();
	camdir.normalise();
	camdir *= 4000.f;		

	Bullet bullet;		
	bullet.position =  mSceneMgr->getSceneNode("turret")->getPosition() + mSceneMgr->getSceneNode("gunnode")->getPosition();;
	bullet.velocity = camdir;		
	mBullets.push_back( bullet );				

	mBulletBillboardSet->createBillboard( bullet.position, ColourValue::Green );												
	mBulletBillboardSet->_updateBounds();
	mBulletCount++;
}

void BaseCommandFrameListener::createEditBrush()
{
	// load the edit brush for terrain editing
	Image image;
	image.load("brush.png", ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME );
	image.resize(16, 16);
	mEditBrush = ET::loadBrushFromImage(image);		
}

void BaseCommandFrameListener::createHole( float elapsedTime, Vector3 point )
{		
	float x = point.x;
	float z = point.z;
	float height = gTerrainMgr->getTerrainInfo().getHeightAt( x, z );

	if( (x > 100 && x < 1400.0f) && (z > 100 && z < 1400.0f) )
	{
		// choose a brush intensity, this determines
		// how extreme our brush works on the terrain
		float brushIntensity = elapsedTime * -10.0f;		  
		// translate our cursor position to vertex indexes          
		int xx = gTerrainMgr->getTerrainInfo().posToVertexX(x);
		int zz = gTerrainMgr->getTerrainInfo().posToVertexZ(z);
		// now tell the ETM to deform the terrain
		gTerrainMgr->deform(xx, zz, mEditBrush, brushIntensity);

		if(height < 50 )
		{
			float brushIntensity = elapsedTime * 15.0f;
			//gSplatMgr->paint(3, xx, zz, mEditBrush, brushIntensity);			 
		}
		else
		{
			float brushIntensity = elapsedTime * 25.0f;
			//gSplatMgr->paint(5, xx, zz, mEditBrush, brushIntensity);			 
		}
	}	

	if( (x > 100 && x < 1400.0f) && (z > 100 && z < 1400.0f) )
	{
		// Push the point of explosion a little higher for better visual of the effect
#ifdef USE_PARTICLEUNIVERSE
		createExplosion( Vector3(point.x, point.y + 2.f, point.z) );
		igniteTrees(point);					
#endif

		// See if a house has been hit and if game over
		int numDead = 0;
		for( std::list<House>::iterator list_iter = mHouses.begin(); list_iter != mHouses.end(); )
		{
			House *p = &*list_iter;
			if( p->position.distance(point) < 100 && !p->dead )						
			{
				p->sceneNode->setVisible(false);
				p->destroyedSceneNode->setVisible(true);
				p->dead = true;

				// Shake cam
				mShakeCamera = true;
				mCameraShakeTime = 0.0f;					
			}			
			if(p->dead)
				numDead++;
			list_iter++;					
		}

		if( numDead == mHouses.size() )
		{
			gameOver();
		}
	}
}

void BaseCommandFrameListener::updateLightmap()
{
	Image lightmap;
	ET::createTerrainLightmap(gTerrainMgr->getTerrainInfo(), lightmap, 128, 128, Vector3(1, -1, 1), ColourValue(1,1,1),
		ColourValue(0.3, 0.3,  0.3));

	// get our dynamic texture and update its contents
	TexturePtr tex = TextureManager::getSingleton().getByName("ETLightmap");
	tex->getBuffer(0, 0)->blitFromMemory(lightmap.getPixelBox(0, 0));
}

#ifdef USE_PARTICLEUNIVERSE
void BaseCommandFrameListener::igniteTrees(Vector3 &point)
{		
	// Add surrounding fires from the point of explosion				
	TreeIterator3D iter = gTreeLoader->getTrees();
	while( iter.hasMoreElements() )
	{
		Forests::TreeRef treeRef = iter.getNext();
		if( treeRef.getPosition().distance( point ) < 100 )
		{
			float randomDuration = Math::RangeRandom( 5.0f, 10.0f );
			for( int i = 0; i < 2; i ++)
			{
				ParticleUniverse::ParticleTechnique* technique = gFireParticleSystem->getTechnique( i );			
				ParticleUniverse::ParticleEmitter* clonedEmitter = gParticleSystemMgr->cloneEmitter( technique->getEmitter( 0 ) );
				static_cast<ParticleUniverse::DynamicAttributeFixed*>(clonedEmitter->getDynDuration())->setValue( randomDuration );				
				// daf->setValue( randomDuration );			
				clonedEmitter->position = treeRef.getPosition();
				clonedEmitter->_notifyStart(); 
				technique->addEmitter( clonedEmitter );		
			}												
		}			
	}	
}

void BaseCommandFrameListener::createExplosion( Vector3 point )
{
	for( int i = 0; i < 4; i ++)
	{
		ParticleUniverse::ParticleTechnique* technique = gExplosionParticleSystem->getTechnique( i );									
		ParticleUniverse::ParticleEmitter* clonedEmitter = gParticleSystemMgr->cloneEmitter( technique->getEmitter( 0 ) );
		clonedEmitter->position = point;
		clonedEmitter->_notifyStart();
		technique->addEmitter( clonedEmitter );
	}		

	int channelFireGun =  INVALID_SOUND_CHANNEL;		
	gSoundMgr->PlaySound( gExplosionSoundId, mSceneMgr->getSceneNode("exnode"), &channelFireGun );	
	gSoundMgr->Set3DMinMaxDistance(channelFireGun, 100.0, 1000.0);	
}
#endif

float BaseCommandFrameListener::getPlayerAccuracyRating()
{
	float accuracy;
	if( mBulletCount == 0 || mKillCount == 0 )
		accuracy = 0.f;
	else
		accuracy = ((float)mKillCount / (float)mBulletCount) * 100.f;
	return accuracy;
}

void BaseCommandFrameListener::gameOver()
{
	float accuracy = getPlayerAccuracyRating();

	mGUISystem->setGUISheet(mGameOverSheet);	   				
	gLogMgr->logMessage("==========Setting gui sheet gameOver============");
	CEGUI::Window *textBox = static_cast<CEGUI::Window*>(mGUIWindowMgr->getWindow((CEGUI::utf8*)"GameOver/Info"));
	char str[512];
	sprintf( str, "Well Done. You have survived for %.2f minutes and have %d kills with an accuracy rating of %.1f%%. Please enter your OGRE community nick name to submit your score.",
		mCurrentGameTime / 60.f, mKillCount, accuracy );
	textBox->setText( str );	

	mGameStart = false;			
}

void BaseCommandFrameListener::ResetGame()
{
	// Reset some vairables
	mCurrentGameTime = 0.f;
	mKillCount = 0;
	mBulletCount = 0;
	mGameStart = true;

	// Hide main menu
	mMainMenuSheet->setVisible(false);							

	// Reset houses
	for( std::list<House>::iterator list_iter = mHouses.begin(); list_iter != mHouses.end(); )
	{
		House *p = &*list_iter;				
		p->sceneNode->setVisible(true);
		p->destroyedSceneNode->setVisible(false);
		p->dead = false;				
		list_iter++;					
	}

	// Clear any missiles
	std::list<Vehicle*>::iterator list_iter;	
	for( list_iter = gVehicles.begin(); list_iter != gVehicles.end(); )
	{
		Vehicle *p = &**list_iter;
		p->clearMissiles();
		list_iter++;	
	}

	clearVehicles();	
}


bool BaseCommandFrameListener::handleQuit(const CEGUI::EventArgs& e)
{
	mQuitting = true;
	return true;
}

bool BaseCommandFrameListener::handleStart(const CEGUI::EventArgs& e)
{
	// Start game
	if( !mDoFade )
	{
		mDoFade = true;
		mFader->startFadeOut(FADEOUT_TIME);
	}
	int temp = INVALID_SOUND_CHANNEL;
	gSoundMgr->PlaySound( gAirRaidSoundId, mSceneMgr->getRootSceneNode(), &temp);				
	return true;
}

bool BaseCommandFrameListener::handleReturn(const CEGUI::EventArgs& e)
{
	mGUISystem->setGUISheet(mMainMenuSheet);	 
	mMainMenuSheet->setVisible(true);		
	populateLeaderboard();
	gLogMgr->logMessage("==========Setting gui sheet handleReturn============");

	return true;
}

bool BaseCommandFrameListener::handleSend(const CEGUI::EventArgs& e)
{
	float accuracy = getPlayerAccuracyRating();		

	CEGUI::Editbox* nameEditbox = static_cast<CEGUI::Editbox*>(mGUIWindowMgr->getWindow((CEGUI::utf8*)"GameOver/NameEditbox"));	   	           

	CEGUI::String name = nameEditbox->getText();
	if( name.size() == 0 )
		name.append( "Unnamed" );

	if(mMySQLConnection.connected())
	{
		mysqlpp::Query query = mMySQLConnection.query();			
		query << "INSERT INTO scores(name,kills,duration, fires, accuracy, createdon) VALUES(\"" 
			<< name << "\", " 
			<< mKillCount << ", " << mCurrentGameTime << ", "
			<< mBulletCount << "," << accuracy << ", Now())";
		query.execute();
	}

	mGUISystem->setGUISheet(mMainMenuSheet);	 
	gLogMgr->logMessage("==========Setting gui sheet handleSend============");
	mMainMenuSheet->setVisible(true);
	populateLeaderboard();
	return true;
}