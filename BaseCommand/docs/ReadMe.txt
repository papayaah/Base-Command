========================================================================
    About the "BaseCommand" Project (c) January 2009
========================================================================

Thank you for downloading this game. 

This game was created using Visual Studio 2008(vc9) and uses different open source libraries and tools:

3D Graphics Engine – OGRE 3D v1.6.1 (http://www.ogre3d.org)
Terrain Editor -  ArtifaxTerra 3D (http://www.artifexterra3d.com/)
Particle Effects – Particle Universe v1.0, OGRE plugin (http://www.fxpression.com/)
Clouds rendering – Caelum v0.4, OGRE plugin
Foliage – Paged Geometry v1.05, OGRE plugin
Terrain engine – ETM v2.3.1, OGRE plugin
Sounds – FMOD and SoundManager class from the OGRE wiki (http://www.ogre3d.org/wiki/index.php/FMOD_SoundManager)
MySQL connection – MySQL++ v3.0.8 (http://tangentsoft.net/mysql++/)

Unforuntately, the Particle Universe library version(1.0) I used is a license copy, which means I cannot include the library here.

But it does not mean you wouldn't be able to compile the game anymore, simply open the precompiled header
"stdafx.h", then search for "USE_PARTICLEUNIVERSE" and comment out that directive and you should be able to compile the game.

I tried my best to include all the library headers, source and libs that I used into this whole visual studio solution so you can generate your own
library using your own version of Visual Studio.

But some like the MySQL connection is too big to just include it here. If you are using an older Visual Studio (2003/2005), then you may encounter 
compilation errors. If this happens, simply download MySQL++ and compile their library using your version of Visual Studio. Linking 
different versions of libraries into one Visual Studio solution simply won't do. Do note that to compile MySQL++ on your own,
you need the MySQL C development headers, which you can get from the MySQL site.

Instructions on how to play are in the game. Don't forget to submit your scores! 

Thanks and enjoy playing!

/////////////////////////////////////////////////////////////////////////////

If you have any questions or suggestions regarding this game, don't hesitate to email me at davidang@pldtdsl.net. I would love to hear from you.

Please don't forget to visit my website as well, where I have other game development projects - http://www.programmingmind.com/

/////////////////////////////////////////////////////////////////////////////
