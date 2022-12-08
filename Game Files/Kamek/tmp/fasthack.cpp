//
// processed\../src/chestnut.cpp
//

#include <game.h>
#include <sfx.h>
const char *ChestnutFileList[] = {"chestnut", 0};

class daEnChestnut_c : public dEn_c {
	public:
		static daEnChestnut_c *build();

		mHeapAllocator_c allocator;
		nw4r::g3d::ResFile resFile;
		m3d::mdl_c model;
		m3d::anmChr_c animation;

		void playAnimation(const char *name, bool loop = false);
		void playLoopedAnimation(const char *name) {
			playAnimation(name, true);
		}

		int objNumber;
		int starCoinNumber;
		bool ignorePlayers;
		bool breaksBlocks;
		float shakeWindow, fallWindow;

		int timeSpentExploding;

		lineSensor_s belowSensor;

		float nearestPlayerDistance();

		int onCreate();
		int onDelete();
		int onExecute();
		int onDraw();

		void spawnObject();

		bool collisionCat1_Fireball_E_Explosion(ActivePhysics *apThis, ActivePhysics *apOther);
		bool collisionCat5_Mario(ActivePhysics *apThis, ActivePhysics *apOther);

		bool CreateIceActors();

		u32 canBePowed();
		void powBlockActivated(bool isNotMPGP);

		USING_STATES(daEnChestnut_c);
		DECLARE_STATE(Idle);
		DECLARE_STATE(Shake);
		DECLARE_STATE(Fall);
		DECLARE_STATE(Explode);
};

CREATE_STATE(daEnChestnut_c, Idle);
CREATE_STATE(daEnChestnut_c, Shake);
CREATE_STATE(daEnChestnut_c, Fall);
CREATE_STATE(daEnChestnut_c, Explode);

daEnChestnut_c *daEnChestnut_c::build() {
	void *buf = AllocFromGameHeap1(sizeof(daEnChestnut_c));
	return new(buf) daEnChestnut_c;
}


int daEnChestnut_c::onCreate() {
	// Get settings
	int texNum = settings & 0xF;
	int rawScale = (settings & 0xF0) >> 4;
	starCoinNumber = (settings & 0xF00) >> 8;
	ignorePlayers = ((settings & 0x1000) != 0);
	breaksBlocks = ((settings & 0x2000) != 0);
	objNumber = (settings & 0xF0000) >> 16;

	if ((settings & 0x4000) != 0) {
		shakeWindow = 96.0f;
		fallWindow = 64.0f;
	} else {
		shakeWindow = 64.0f;
		fallWindow = 32.0f;
	}

	// Set up models
	allocator.link(-1, GameHeaps[0], 0, 0x20);

	char rfName[64];
	sprintf(rfName, "g3d/t%02d.brres", texNum);

	resFile.data = getResource("chestnut", rfName);

	nw4r::g3d::ResMdl resMdl = resFile.GetResMdl("kuribo_iga");
	nw4r::g3d::ResAnmChr resAnm = resFile.GetResAnmChr("wait");

	model.setup(resMdl, &allocator, 0x224, 1, 0);
	SetupTextures_Enemy(&model, 0);

	animation.setup(resMdl, resAnm, &allocator, 0);

	allocator.unlink();


	// Scale us
	scale.x = scale.y = scale.z = (1.0f + (float(rawScale) * 0.5f));

	// Physics and crap
	ActivePhysics::Info ccInfo;
	ccInfo.xDistToCenter = 0.0f;
	ccInfo.xDistToEdge = 12.0f * scale.x;
	ccInfo.yDistToCenter = 1.0f + (12.0f * scale.y);
	ccInfo.yDistToEdge = 12.0f * scale.y;

	ccInfo.category1 = 3;
	ccInfo.category2 = 0;
	ccInfo.bitfield1 = 0x6F;
	ccInfo.bitfield2 = 0xFFBAFFFE;

	ccInfo.unkShort1C = 0;
	ccInfo.callback = &dEn_c::collisionCallback;

	aPhysics.initWithStruct(this, &ccInfo);
	aPhysics.addToList();

	// WE'RE READY
	doStateChange(&StateID_Idle);

	return true;
}

void daEnChestnut_c::playAnimation(const char *name, bool loop) {
	animation.bind(&model, resFile.GetResAnmChr(name), !loop);
	model.bindAnim(&animation, 0.0f);
	animation.setUpdateRate(1.0f);
}

int daEnChestnut_c::onDelete() {
	aPhysics.removeFromList();
	return true;
}

int daEnChestnut_c::onExecute() {
	acState.execute();

	matrix.translation(pos.x, pos.y, pos.z);

	model.setDrawMatrix(matrix);
	model.setScale(&scale);
	model.calcWorld(false);

	model._vf1C();

	return true;
}

int daEnChestnut_c::onDraw() {
	model.scheduleForDrawing();

	return true;
}

float daEnChestnut_c::nearestPlayerDistance() {
	float bestSoFar = 10000.0f;

	for (int i = 0; i < 4; i++) {
		if (dAcPy_c *player = dAcPy_c::findByID(i)) {
			if (strcmp(player->states2.getCurrentState()->getName(), "dAcPy_c::StateID_Balloon")) {
				float thisDist = abs(player->pos.x - pos.x);
				if (thisDist < bestSoFar)
					bestSoFar = thisDist;
			}
		}
	}

	return bestSoFar;
}



void daEnChestnut_c::beginState_Idle() {
	playLoopedAnimation("wait");
}
void daEnChestnut_c::endState_Idle() { }


void daEnChestnut_c::executeState_Idle() {
	if (ignorePlayers)
		return;

	float dist = nearestPlayerDistance();

	if (dist < fallWindow)
		doStateChange(&StateID_Fall);
	else if (dist < shakeWindow)
		doStateChange(&StateID_Shake);
}



void daEnChestnut_c::beginState_Shake() {
	playLoopedAnimation("shake");
	animation.setUpdateRate(2.0f);

	nw4r::snd::SoundHandle handle;
	PlaySoundWithFunctionB4(SoundRelatedClass, &handle, SE_PLY_CLIMB_KUSARI, 1);
}
void daEnChestnut_c::endState_Shake() { }

void daEnChestnut_c::executeState_Shake() {
	float dist = nearestPlayerDistance();

	if (dist >= shakeWindow)
		doStateChange(&StateID_Idle);
	else if (dist < fallWindow)
		doStateChange(&StateID_Fall);
}



void daEnChestnut_c::beginState_Fall() {
	animation.setUpdateRate(0.0f); // stop animation

	int size = 12*scale.x;

	belowSensor.flags = SENSOR_LINE;
	if (breaksBlocks)
		belowSensor.flags |= SENSOR_10000000 | SENSOR_BREAK_BLOCK | SENSOR_BREAK_BRICK;
	// 10000000 makes it pass through bricks

	belowSensor.lineA = -size << 12;
	belowSensor.lineB = size << 12;
	belowSensor.distanceFromCenter = 0;

	collMgr.init(this, &belowSensor, 0, 0);

	speed.y = 0.0f;
	y_speed_inc = -0.1875f;
	max_speed.y = -4.0f;

	nw4r::snd::SoundHandle handle;
	PlaySoundWithFunctionB4(SoundRelatedClass, &handle, SE_DEMO_OP_PRESENT_THROW_2308f, 1);
}

void daEnChestnut_c::endState_Fall() { }

void daEnChestnut_c::executeState_Fall() {
	HandleYSpeed();
	doSpriteMovement();
	UpdateObjectPosBasedOnSpeedValuesReal();

	if (collMgr.calculateBelowCollision() & (~0x400000)) {
		doStateChange(&StateID_Explode);
	}
}



void daEnChestnut_c::beginState_Explode() {
	OSReport("Entering Explode\n");
	playAnimation("break");
	animation.setUpdateRate(2.0f);

	nw4r::snd::SoundHandle handle;
	PlaySoundWithFunctionB4(SoundRelatedClass, &handle, SE_DEMO_OP_LAND_JR_0983f, 1);

	timeSpentExploding = 0;
}
void daEnChestnut_c::endState_Explode() { }

void daEnChestnut_c::executeState_Explode() {
	timeSpentExploding++;

	if (timeSpentExploding == 10) {
		S16Vec efRot = {0,0,0};
		SpawnEffect("Wm_en_burst_ss", 0, &pos, &efRot, &scale);
		spawnObject();
	}

	if (animation.isAnimationDone()) {
		Delete(1);
	}
}



bool daEnChestnut_c::CreateIceActors() {
	animation.setUpdateRate(0.0f);
	
	IceActorSpawnInfo info;
	info.flags = 0;
	info.pos = pos;
	info.pos.y -= (6.0f * info.scale.y);
	info.scale.x = info.scale.y = info.scale.z = scale.x * 1.35f;
	for (int i = 0; i < 8; i++)
		info.what[0] = 0.0f;

	return frzMgr.Create_ICEACTORs(&info, 1);
}

u32 daEnChestnut_c::canBePowed() {
	return true;
}
void daEnChestnut_c::powBlockActivated(bool isNotMPGP) {
	if (!isNotMPGP)
		return;

	dStateBase_c *state = acState.getCurrentState();
	if (state == &StateID_Idle || state == &StateID_Shake)
		doStateChange(&StateID_Fall);
}

bool daEnChestnut_c::collisionCat1_Fireball_E_Explosion(ActivePhysics *apThis, ActivePhysics *apOther) {
	SpawnEffect("Wm_en_igafirehit", 0, &pos, &rot, &scale);

	if (acState.getCurrentState() != &StateID_Explode)
		doStateChange(&StateID_Explode);

	return true;
}

bool daEnChestnut_c::collisionCat5_Mario(ActivePhysics *apThis, ActivePhysics *apOther) {
	this->_vf220(apOther->owner);
	return true;
}


void daEnChestnut_c::spawnObject() {
	VEC3 acPos = pos;

	static const u32 things[] = {
		EN_KURIBO, 0,
		EN_TOGEZO, 0,
		EN_COIN_JUMP, 0,
		EN_ITEM, 0x05000009,
		EN_STAR_COIN, 0x10000000,
	};

	u32 acSettings = things[objNumber*2+1];

	if (objNumber == 4) {
		acSettings |= (starCoinNumber << 8);
		acPos.x -= 12.0f;
		acPos.y += 32.0f;
	}

	aPhysics.removeFromList();

	OSReport("Crap %d, %d, %08x\n", objNumber, things[objNumber*2], acSettings);
	dStageActor_c *ac =
		dStageActor_c::create((Actors)things[objNumber*2], acSettings, &acPos, 0, currentLayerID);

	S16Vec efRot = {0,0,0};
	SpawnEffect("Wm_ob_itemsndlandsmk", 0, &pos, &efRot, &scale);

	if (objNumber == 0) {
		dEn_c *en = (dEn_c*)ac;
		en->direction = 1;
		en->rot.y = -8192;
	}
}

//
// processed\../src/flipblock.cpp
//

#include <common.h>
#include <game.h>

const char *FlipBlockFileList[] = {"block_rotate", 0};

class daEnFlipBlock_c : public daEnBlockMain_c {
public:
	Physics::Info physicsInfo;

	int onCreate();
	int onDelete();
	int onExecute();
	int onDraw();

	void calledWhenUpMoveExecutes();
	void calledWhenDownMoveExecutes();

	void blockWasHit(bool isDown);

	bool playerOverlaps();

	mHeapAllocator_c allocator;
	nw4r::g3d::ResFile resFile;
	m3d::mdl_c model;

	int flipsRemaining;

	USING_STATES(daEnFlipBlock_c);
	DECLARE_STATE(Wait);
	DECLARE_STATE(Flipping);

	static daEnFlipBlock_c *build();
};


CREATE_STATE(daEnFlipBlock_c, Wait);
CREATE_STATE(daEnFlipBlock_c, Flipping);


int daEnFlipBlock_c::onCreate() {
	allocator.link(-1, GameHeaps[0], 0, 0x20);

	resFile.data = getResource("block_rotate", "g3d/block_rotate.brres");
	model.setup(resFile.GetResMdl("block_rotate"), &allocator, 0, 1, 0);
	SetupTextures_MapObj(&model, 0);

	allocator.unlink();



	blockInit(pos.y);

	physicsInfo.x1 = -8;
	physicsInfo.y1 = 8;
	physicsInfo.x2 = 8;
	physicsInfo.y2 = -8;

	physicsInfo.otherCallback1 = &daEnBlockMain_c::OPhysicsCallback1;
	physicsInfo.otherCallback2 = &daEnBlockMain_c::OPhysicsCallback2;
	physicsInfo.otherCallback3 = &daEnBlockMain_c::OPhysicsCallback3;

	physics.setup(this, &physicsInfo, 3, currentLayerID);
	physics.flagsMaybe = 0x260;
	physics.callback1 = &daEnBlockMain_c::PhysicsCallback1;
	physics.callback2 = &daEnBlockMain_c::PhysicsCallback2;
	physics.callback3 = &daEnBlockMain_c::PhysicsCallback3;
	physics.addToList();

	doStateChange(&daEnFlipBlock_c::StateID_Wait);

	return true;
}


int daEnFlipBlock_c::onDelete() {
	physics.removeFromList();

	return true;
}


int daEnFlipBlock_c::onExecute() {
	acState.execute();
	physics.update();
	blockUpdate();

	// now check zone bounds based on state
	if (acState.getCurrentState()->isEqual(&StateID_Wait)) {
		checkZoneBoundaries(0);
	}

	return true;
}


int daEnFlipBlock_c::onDraw() {
	matrix.translation(pos.x, pos.y, pos.z);
	matrix.applyRotationYXZ(&rot.x, &rot.y, &rot.z);

	model.setDrawMatrix(matrix);
	model.setScale(&scale);
	model.calcWorld(false);
	model.scheduleForDrawing();

	return true;
}


daEnFlipBlock_c *daEnFlipBlock_c::build() {

	void *buffer = AllocFromGameHeap1(sizeof(daEnFlipBlock_c));
	daEnFlipBlock_c *c = new(buffer) daEnFlipBlock_c;


	return c;
}


void daEnFlipBlock_c::blockWasHit(bool isDown) {
	pos.y = initialY;

	doStateChange(&StateID_Flipping);
}



void daEnFlipBlock_c::calledWhenUpMoveExecutes() {
	if (initialY >= pos.y)
		blockWasHit(false);
}

void daEnFlipBlock_c::calledWhenDownMoveExecutes() {
	if (initialY <= pos.y)
		blockWasHit(true);
}



void daEnFlipBlock_c::beginState_Wait() {
}

void daEnFlipBlock_c::endState_Wait() {
}

void daEnFlipBlock_c::executeState_Wait() {
	int result = blockResult();

	if (result == 0)
		return;

	if (result == 1) {
		doStateChange(&daEnBlockMain_c::StateID_UpMove);
		anotherFlag = 2;
		isGroundPound = false;
	} else {
		doStateChange(&daEnBlockMain_c::StateID_DownMove);
		anotherFlag = 1;
		isGroundPound = true;
	}
}


void daEnFlipBlock_c::beginState_Flipping() {
	flipsRemaining = 7;
	physics.removeFromList();
}
void daEnFlipBlock_c::executeState_Flipping() {
	if (isGroundPound)
		rot.x += 0x800;
	else
		rot.x -= 0x800;

	if (rot.x == 0) {
		flipsRemaining--;
		if (flipsRemaining <= 0) {
			if (!playerOverlaps())
				doStateChange(&StateID_Wait);
		}
	}
}
void daEnFlipBlock_c::endState_Flipping() {
	physics.setup(this, &physicsInfo, 3, currentLayerID);
	physics.addToList();
}



bool daEnFlipBlock_c::playerOverlaps() {
	dStageActor_c *player = 0;

	Vec myBL = {pos.x - 8.0f, pos.y - 8.0f, 0.0f};
	Vec myTR = {pos.x + 8.0f, pos.y + 8.0f, 0.0f};

	while ((player = (dStageActor_c*)fBase_c::search(PLAYER, player)) != 0) {
		float centerX = player->pos.x + player->aPhysics.info.xDistToCenter;
		float centerY = player->pos.y + player->aPhysics.info.yDistToCenter;

		float left = centerX - player->aPhysics.info.xDistToEdge;
		float right = centerX + player->aPhysics.info.xDistToEdge;

		float top = centerY + player->aPhysics.info.yDistToEdge;
		float bottom = centerY - player->aPhysics.info.yDistToEdge;

		Vec playerBL = {left, bottom + 0.1f, 0.0f};
		Vec playerTR = {right, top - 0.1f, 0.0f};

		if (RectanglesOverlap(&playerBL, &playerTR, &myBL, &myTR))
			return true;
	}

	return false;
}


//
// processed\../src/magicplatform.cpp
//

#include <game.h>
#include <dCourse.h>

class daEnMagicPlatform_c : public dEn_c {
	public:
		static daEnMagicPlatform_c *build();

		int onCreate();
		int onExecute();
		int onDelete();

		enum CollisionType {
			Solid = 0,
			SolidOnTop = 1,
			None = 2,
			ThinLineRight = 3,
			ThinLineLeft = 4,
			ThinLineTop = 5,
			ThinLineBottom = 6,
			NoneWithZ500 = 7
		};

		// Settings
		CollisionType collisionType;
		u8 rectID, moveSpeed, moveDirection, moveLength;

		u8 moveDelay, currentMoveDelay;

		bool doesMoveInfinitely;

		float moveMin, moveMax, moveDelta, moveBaseDelta;
		float *moveTarget;

		bool isMoving;
		void setupMovement();
		void handleMovement();

		Physics physics;
		StandOnTopCollider sotCollider;

		TileRenderer *renderers;
		int rendererCount;

		void findSourceArea();
		void createTiles();
		void deleteTiles();
		void updateTilePositions();

		void checkVisibility();
		void setVisible(bool shown);

		bool isVisible;

		int srcX, srcY;
		int width, height;
};

/*****************************************************************************/
// Glue Code
daEnMagicPlatform_c *daEnMagicPlatform_c::build() {
	void *buffer = AllocFromGameHeap1(sizeof(daEnMagicPlatform_c));
	daEnMagicPlatform_c *c = new(buffer) daEnMagicPlatform_c;
	return c;
}

extern "C" void HurtMarioBecauseOfBeingSquashed(void *mario, dStageActor_c *squasher, int type);

static void PhysCB1(daEnMagicPlatform_c *one, dStageActor_c *two) {
	if (two->stageActorType != 1)
		return;

	// if left/right
	if (one->moveDirection <= 1)
		return;

	if (one->pos_delta.y > 0.0f)
		HurtMarioBecauseOfBeingSquashed(two, one, 1);
	else
		HurtMarioBecauseOfBeingSquashed(two, one, 9);
}

static void PhysCB2(daEnMagicPlatform_c *one, dStageActor_c *two) {
	if (two->stageActorType != 1)
		return;

	// if left/right
	if (one->moveDirection <= 1)
		return;

	if (one->pos_delta.y < 0.0f)
		HurtMarioBecauseOfBeingSquashed(two, one, 2);
	else
		HurtMarioBecauseOfBeingSquashed(two, one, 10);
}

static void PhysCB3(daEnMagicPlatform_c *one, dStageActor_c *two, bool unkMaybeNotBool) {
	if (two->stageActorType != 1)
		return;

	// if up/down
	if (one->moveDirection > 1)
		return;

	if (unkMaybeNotBool) {
		if (one->pos_delta.x > 0.0f)
			HurtMarioBecauseOfBeingSquashed(two, one, 6);
		else
			HurtMarioBecauseOfBeingSquashed(two, one, 12);
	} else {
		if (one->pos_delta.x < 0.0f)
			HurtMarioBecauseOfBeingSquashed(two, one, 5);
		else
			HurtMarioBecauseOfBeingSquashed(two, one, 11);
	}
}

static bool PhysCB4(daEnMagicPlatform_c *one, dStageActor_c *two) {
	return (one->pos_delta.y > 0.0f);
}

static bool PhysCB5(daEnMagicPlatform_c *one, dStageActor_c *two) {
	return (one->pos_delta.y < 0.0f);
}

static bool PhysCB6(daEnMagicPlatform_c *one, dStageActor_c *two, bool unkMaybeNotBool) {
	if (unkMaybeNotBool) {
		if (one->pos_delta.x > 0.0f)
			return true;
	} else {
		if (one->pos_delta.x < 0.0f)
			return true;
	}
	return false;
}

int daEnMagicPlatform_c::onCreate() {
	rectID = settings & 0xFF;

	moveSpeed = (settings & 0xF00) >> 8;
	moveDirection = (settings & 0x3000) >> 12;
	moveLength = ((settings & 0xF0000) >> 16) + 1;

	moveDelay = ((settings & 0xF00000) >> 20) * 6;

	collisionType = (CollisionType)((settings & 0xF000000) >> 24);

	doesMoveInfinitely = (settings & 0x10000000);

	if (settings & 0xE0000000) {
		int putItBehind = settings >> 29;
		pos.z = -3600.0f - (putItBehind * 16);
	}
	if (collisionType == NoneWithZ500)
		pos.z = 500.0f;

	setupMovement();

	findSourceArea();
	createTiles();

	float fWidth = width * 16.0f;
	float fHeight = height * 16.0f;

	switch (collisionType) {
		case Solid:
			physics.setup(this,
					0.0f, 0.0f, fWidth, -fHeight,
					(void*)&PhysCB1, (void*)&PhysCB2, (void*)&PhysCB3, 1, 0, 0);

			physics.callback1 = (void*)&PhysCB4;
			physics.callback2 = (void*)&PhysCB5;
			physics.callback3 = (void*)&PhysCB6;

			physics.addToList();
			break;
		case SolidOnTop:
			sotCollider.init(this,
					/*xOffset=*/0.0f, /*yOffset=*/0.0f,
					/*topYOffset=*/0,
					/*rightSize=*/fWidth, /*leftSize=*/0,
					/*rotation=*/0, /*_45=*/1
					);

			// What is this for. I dunno
			sotCollider._47 = 0xA;
			sotCollider.flags = 0x80180 | 0xC00;

			sotCollider.addToList();

			break;
		case ThinLineLeft: case ThinLineRight:
		case ThinLineTop: case ThinLineBottom:
			physics.setup(this,
				fWidth * (collisionType == ThinLineRight ? 0.875f : 0.0f),
				fHeight * (collisionType == ThinLineBottom ? -0.75f : 0.0f),
				fWidth * (collisionType == ThinLineLeft ? 0.125f : 1.0f),
				fHeight * (collisionType == ThinLineTop ? -0.25f : -1.0f),
				(void*)&PhysCB1, (void*)&PhysCB2, (void*)&PhysCB3, 1, 0, 0);

			physics.callback1 = (void*)&PhysCB4;
			physics.callback2 = (void*)&PhysCB5;
			physics.callback3 = (void*)&PhysCB6;

			physics.addToList();
			break;
	}

	return 1;
}

int daEnMagicPlatform_c::onDelete() {
	deleteTiles();

	switch (collisionType) {
		case ThinLineLeft: case ThinLineRight:
		case ThinLineTop: case ThinLineBottom:
		case Solid: physics.removeFromList(); break;
	}

	return 1;
}

int daEnMagicPlatform_c::onExecute() {
	handleMovement();

	checkVisibility();

	updateTilePositions();

	switch (collisionType) {
		case ThinLineLeft: case ThinLineRight:
		case ThinLineTop: case ThinLineBottom:
		case Solid: physics.update(); break;
		case SolidOnTop: sotCollider.update(); break;
	}

	return 1;
}

/*****************************************************************************/
// Movement
void daEnMagicPlatform_c::setupMovement() {
	float fMoveLength = 16.0f * moveLength;
	float fMoveSpeed = 0.2f * moveSpeed;

	switch (moveDirection) {
		case 0: // RIGHT
			moveTarget = &pos.x;
			moveMin = pos.x;
			moveMax = pos.x + fMoveLength;
			moveBaseDelta = fMoveSpeed;
			break;
		case 1: // LEFT
			moveTarget = &pos.x;
			moveMin = pos.x - fMoveLength;
			moveMax = pos.x;
			moveBaseDelta = -fMoveSpeed;
			break;
		case 2: // UP
			moveTarget = &pos.y;
			moveMin = pos.y;
			moveMax = pos.y + fMoveLength;
			moveBaseDelta = fMoveSpeed;
			break;
		case 3: // DOWN
			moveTarget = &pos.y;
			moveMin = pos.y - fMoveLength;
			moveMax = pos.y;
			moveBaseDelta = -fMoveSpeed;
			break;
	}

	if (spriteFlagNum == 0) {
		isMoving = (moveSpeed > 0);
		moveDelta = moveBaseDelta;
	} else {
		isMoving = false;
	}

	currentMoveDelay = 0;
}

void daEnMagicPlatform_c::handleMovement() {
	if (spriteFlagNum > 0) {
		// Do event checks
		bool flagOn = ((dFlagMgr_c::instance->flags & spriteFlagMask) != 0);

		if (isMoving) {
			if (!flagOn) {
				// Flag was turned off while moving, so go back
				moveDelta = -moveBaseDelta;
			} else {
				moveDelta = moveBaseDelta;
			}
		} else {
			if (flagOn) {
				// Flag was turned on, so start moving
				moveDelta = moveBaseDelta;
				isMoving = true;
			}
		}
	}

	if (!isMoving)
		return;

	if (currentMoveDelay > 0) {
		currentMoveDelay--;
		return;
	}

	// Do it
	bool goesForward = (moveDelta > 0.0f);
	bool reachedEnd = false;

	*moveTarget += moveDelta;

	// if we're set to move infinitely, never stop
	if (doesMoveInfinitely)
		return;

	if (goesForward) {
		if (*moveTarget >= moveMax) {
			*moveTarget = moveMax;
			reachedEnd = true;
		}
	} else {
		if (*moveTarget <= moveMin) {
			*moveTarget = moveMin;
			reachedEnd = true;
		}
	}

	if (reachedEnd) {
		if (spriteFlagNum > 0) {
			// If event, just do nothing.. depending on what side we are on
			if ((moveDelta > 0.0f && moveBaseDelta > 0.0f) || (moveDelta < 0.0f && moveBaseDelta < 0.0f)) {
				// We reached the end, so keep isMoving on for when we need to reverse
			} else {
				// We're back at the start, so turn it off
				isMoving = false;
			}
		} else {
			// Otherwise, reverse
			moveDelta = -moveDelta;
			currentMoveDelay = moveDelay;
		}
	}
}

/*****************************************************************************/
// Tile Renderers

void daEnMagicPlatform_c::findSourceArea() {
	mRect rect;
	dCourseFull_c::instance->get(GetAreaNum())->getRectByID(rectID, &rect);

	// Round the positions down/up to get the rectangle
	int left = rect.x;
	int right = left + rect.width;
	int top = -rect.y;
	int bottom = top + rect.height;

	left &= 0xFFF0;
	right = (right + 15) & 0xFFF0;

	top &= 0xFFF0;
	bottom = (bottom + 15) & 0xFFF0;

	// Calculate the actual stuff
	srcX = left >> 4;
	srcY = top >> 4;
	width = (right - left) >> 4;
	height = (bottom - top) >> 4;

	//OSReport("Area: %f, %f ; %f x %f\n", rect.x, rect.y, rect.width, rect.height);
	//OSReport("Source: %d, %d ; Size: %d x %d\n", srcX, srcY, width, height);
}


void daEnMagicPlatform_c::createTiles() {
	rendererCount = width * height;
	renderers = new TileRenderer[rendererCount];

	// copy all the fuckers over
	int baseWorldX = srcX << 4, worldY = srcY << 4, rendererID = 0;

	for (int y = 0; y < height; y++) {
		int worldX = baseWorldX;

		for (int x = 0; x < width; x++) {
			u16 *pExistingTile = dBgGm_c::instance->getPointerToTile(worldX, worldY, 0);

			if (*pExistingTile > 0) {
				TileRenderer *r = &renderers[rendererID];
				r->tileNumber = *pExistingTile;
				r->z = pos.z;
			}

			worldX += 16;
			rendererID++;
		}

		worldY += 16;
	}

}

void daEnMagicPlatform_c::deleteTiles() {
	if (renderers != 0) {
		setVisible(false);

		delete[] renderers;
		renderers = 0;
	}
}

void daEnMagicPlatform_c::updateTilePositions() {
	float baseX = pos.x;

	float y = -pos.y;

	int rendererID = 0;

	for (int yIdx = 0; yIdx < height; yIdx++) {
		float x = baseX;

		for (int xIdx = 0; xIdx < width; xIdx++) {
			TileRenderer *r = &renderers[rendererID];
			r->x = x;
			r->y = y;

			x += 16.0f;
			rendererID++;
		}

		y += 16.0f;
	}
}



void daEnMagicPlatform_c::checkVisibility() {
	float effectiveLeft = pos.x, effectiveRight = pos.x + (width * 16.0f);
	float effectiveBottom = pos.y - (height * 16.0f), effectiveTop = pos.y;

	ClassWithCameraInfo *cwci = ClassWithCameraInfo::instance;

	float screenRight = cwci->screenLeft + cwci->screenWidth;
	float screenBottom = cwci->screenTop - cwci->screenHeight;

	bool isOut = (effectiveLeft > screenRight) ||
		(effectiveRight < cwci->screenLeft) ||
		(effectiveTop < screenBottom) ||
		(effectiveBottom > cwci->screenTop);

	setVisible(!isOut);
}

void daEnMagicPlatform_c::setVisible(bool shown) {
	if (isVisible == shown)
		return;
	isVisible = shown;

	TileRenderer::List *list = dBgGm_c::instance->getTileRendererList(0);

	for (int i = 0; i < rendererCount; i++) {
		if (renderers[i].tileNumber > 0) {
			if (shown) {
				list->add(&renderers[i]);
			} else {
				list->remove(&renderers[i]);
			}
		}
	}
}


//
// processed\../src/music.cpp
//

#include <game.h>
#include <sfx.h>
#include "music.h"

struct HijackedStream {
	//const char *original;
	//const char *originalFast;
	u32 stringOffset;
	u32 stringOffsetFast;
	u32 infoOffset;
	u8 originalID;
	int streamID;
};

struct Hijacker {
	HijackedStream stream[2];
	u8 currentStream;
	u8 currentCustomTheme;
};



const char* SongNameList [] = {
	"AIRSHIP",
	"BOSS_TOWER",
	"MENU",
	"UNDERWATER",
	"ATHLETIC",
	"CASTLE",
	"MAIN",
	"MOUNTAIN",
	"TOWER",
	"UNDERGROUND",
	"DESERT",
	"FIRE",
	"FOREST",
	"FREEZEFLAME",
	"JAPAN",
	"PUMPKIN",
	"SEWER",
	"SPACE",
	"BOWSER",
	"BONUS",	
	"AMBUSH",	
	"BRIDGE_DRUMS",	
	"SNOW2",	
	"MINIMEGA",	
	"CLIFFS",
	"AUTUMN",
	"CRYSTALCAVES",
	"GHOST_HOUSE",
	"GRAVEYARD",
	"JUNGLE",
	"TROPICAL",
	"SKY_CITY",
	"SNOW",
	"STAR_HAVEN",
	"SINGALONG",
	"FACTORY",
	"TANK",
	"TRAIN",
	"YOSHIHOUSE",
	"FACTORYB",
	"CAVERN",
	"SAND",
	"SHYGUY",
	"MINIGAME",
	"BONUS_AREA",
	"CHALLENGE",
	"BOWSER_CASTLE",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"BOSS_CASTLE",
	"BOSS_AIRSHIP",
	NULL	
};



// Offsets are from the start of the INFO block, not the start of the brsar.
// INFO begins at 0x212C0, so that has to be subtracted from absolute offsets
// within the brsar.

#define _I(offs) ((offs)-0x212C0)

Hijacker Hijackers[2] = {
	{
		{
			{/*"athletic_lr.n.32.brstm", "athletic_fast_lr.n.32.brstm",*/ _I(0x4A8F8), _I(0x4A938), _I(0x476C4), 4, STRM_BGM_ATHLETIC},
			{/*"BGM_SIRO.32.brstm", "BGM_SIRO_fast.32.brstm",*/ _I(0x4B2E8), _I(0x4B320), _I(0x48164), 10, STRM_BGM_SHIRO}
		},
		0, 0
	},

	{
		{
			{/*"STRM_BGM_CHIJOU.brstm", "STRM_BGM_CHIJOU_FAST.brstm",*/ _I(0x4A83C), _I(0x4A8B4), 0, 1, STRM_BGM_CHIJOU},
			{/*"STRM_BGM_CHIKA.brstm", "STRM_BGM_CHIKA_FAST.brstm",*/ _I(0x4A878), _I(0x4A780), 0, 2, STRM_BGM_CHIKA},
		},
		0, 0
	}
};

extern void *SoundRelatedClass;
inline char *BrsarInfoOffset(u32 offset) {
	return (char*)(*(u32*)(((u32)SoundRelatedClass) + 0x5CC)) + offset;
}

void FixFilesize(u32 streamNameOffset);

u8 hijackMusicWithSongName(const char *songName, int themeID, bool hasFast, int channelCount, int trackCount, int *wantRealStreamID) {
	Hijacker *hj = &Hijackers[channelCount==4?1:0];

	// do we already have this theme in this slot?
	// if so, don't switch streams
	// if we do, NSMBW will think it's a different song, and restart it ...
	// but if it's just an area transition where both areas are using the same
	// song, we don't want that
	if ((themeID >= 0) && hj->currentCustomTheme == themeID)
		return hj->stream[hj->currentStream].originalID;

	// which one do we use this time...?
	int toUse = (hj->currentStream + 1) & 1;

	hj->currentStream = toUse;
	hj->currentCustomTheme = themeID;

	// write the stream's info
	HijackedStream *stream = &hj->stream[hj->currentStream];

	if (stream->infoOffset) {
		u16 *thing = (u16*)(BrsarInfoOffset(stream->infoOffset) + 4);
		OSReport("Modifying stream info, at offset %x which is at pointer %x\n", stream->infoOffset, thing);
		OSReport("It currently has: channel count %d, track bitfield 0x%x\n", thing[0], thing[1]);
		thing[0] = channelCount;
		thing[1] = (1 << trackCount) - 1;
		OSReport("It has been set to: channel count %d, track bitfield 0x%x\n", thing[0], thing[1]);
	}

	sprintf(BrsarInfoOffset(stream->stringOffset), "stream/%s.brstm", songName);
	sprintf(BrsarInfoOffset(stream->stringOffsetFast), hasFast?"stream/%s_F.brstm":"stream/%s.brstm", songName);

	// update filesizes
	FixFilesize(stream->stringOffset);
	FixFilesize(stream->stringOffsetFast);

	// done!
	if (wantRealStreamID)
		*wantRealStreamID = stream->streamID;

	return stream->originalID;
}


//oh for fuck's sake
#include "fileload.h"
//#include <rvl/dvd.h>

void FixFilesize(u32 streamNameOffset) {
	char *streamName = BrsarInfoOffset(streamNameOffset);

	char nameWithSound[80];
	snprintf(nameWithSound, 79, "/Sound/%s", streamName);

	s32 entryNum;
	DVDHandle info;
	
	if ((entryNum = DVDConvertPathToEntrynum(nameWithSound)) >= 0) {
		if (DVDFastOpen(entryNum, &info)) {
			u32 *lengthPtr = (u32*)(streamName - 0x1C);
			*lengthPtr = info.length;
		}
	} else
		OSReport("What, I couldn't find \"%s\" :(\n", nameWithSound);
}



extern "C" u8 after_course_getMusicForZone(u8 realThemeID) {
	if (realThemeID < 100)
		return realThemeID;

	bool usesDrums = (realThemeID >= 200);
	const char *name = SongNameList[realThemeID - (usesDrums ? 200 : 100)];
	return hijackMusicWithSongName(name, realThemeID, true, usesDrums?4:2, usesDrums?2:1, 0);
}



//
// processed\../src/animtiles.cpp
//

#include <common.h>
#include <game.h>
#include "fileload.h"

struct AnimDef_Header {
	u32 magic;
	u32 entryCount;
};

struct AnimDef_Entry {
	u16 texNameOffset;
	u16 frameDelayOffset;
	u16 tileNum;
	u8 tilesetNum;
	u8 reverse;
};

FileHandle fh;

void DoTiles(void* self) {
	AnimDef_Header *header;
	
	header = (AnimDef_Header*)LoadFile(&fh, "/NewerRes/AnimTiles.bin");
	
	if (!header) {
		OSReport("anim load fail\n");
		return;
	}
	
	if (header->magic != 'NWRa') {
		OSReport("anim info incorrect\n");
		FreeFile(&fh);
		return;
	}
	
	AnimDef_Entry *entries = (AnimDef_Entry*)(header+1);
	
	for (int i = 0; i < header->entryCount; i++) {
		AnimDef_Entry *entry = &entries[i];
		char *name = (char*)fh.filePtr+entry->texNameOffset;
		char *frameDelays = (char*)fh.filePtr+entry->frameDelayOffset;
		
		char realName[0x40];
		snprintf(realName, 0x40, "BG_tex/%s", name);
		
		void *blah = BgTexMng__LoadAnimTile(self, entry->tilesetNum, entry->tileNum, realName, frameDelays, entry->reverse);
	}
}


void DestroyTiles(void *self) {
	FreeFile(&fh);
}


extern "C" void CopyAnimTile(u8 *target, int tileNum, u8 *source, int frameNum) {
	int tileRow = tileNum >> 5; // divided by 32
	int tileColumn = tileNum & 31; // modulus by 32

	u8 *baseRow = target + (tileRow * 2 * 32 * 1024);
	u8 *baseTile = baseRow + (tileColumn * 32 * 4 * 2);

	u8 *sourceRow = source + (frameNum * 2 * 32 * 32);

	for (int i = 0; i < 8; i++) {
		memcpy(baseTile, sourceRow, 32*4*2);
		baseTile += (2 * 4 * 1024);
		sourceRow += (2 * 32 * 4);
	}
}

//
// processed\../src/fileload.cpp
//

#include "fileload.h"

extern "C" void UncompressBackward(void *bottom);


void *LoadFile(FileHandle *handle, const char *name) {

	int entryNum = DVDConvertPathToEntrynum(name);

	DVDHandle dvdhandle;
	if (!DVDFastOpen(entryNum, &dvdhandle)) {
		return 0;
	}

	handle->length = dvdhandle.length;
	handle->filePtr = EGG__Heap__alloc((handle->length+0x1F) & ~0x1F, 0x20, GetArchiveHeap());

	int ret = DVDReadPrio(&dvdhandle, handle->filePtr, (handle->length+0x1F) & ~0x1F, 0, 2);

	DVDClose(&dvdhandle);


	return handle->filePtr;
}

bool FreeFile(FileHandle *handle) {
	if (!handle) return false;

	if (handle->filePtr) {
		EGG__Heap__free(handle->filePtr, GetArchiveHeap());
	}

	handle->filePtr = 0;
	handle->length = 0;

	return true;
}




File::File() {
	m_loaded = false;
}

File::~File() {
	close();
}

bool File::open(const char *filename) {
	if (m_loaded)
		close();

	void *ret = LoadFile(&m_handle, filename);
	if (ret != 0)
		m_loaded = true;

	return (ret != 0);
}

/*bool File::openCompressed(const char *filename) {
	if (m_loaded)
		close();

	void *ret = LoadCompressedFile(&m_handle, filename);
	if (ret != 0)
		m_loaded = true;

	return (ret != 0);
}*/

void File::close() {
	if (!m_loaded)
		return;

	m_loaded = false;
	FreeFile(&m_handle);
}

bool File::isOpen() {
	return m_loaded;
}

void *File::ptr() {
	if (m_loaded)
		return m_handle.filePtr;
	else
		return 0;
}

u32 File::length() {
	if (m_loaded)
		return m_handle.length;
	else
		return 0xFFFFFFFF;
}


//
// processed\../src/levelspecial.cpp
//

#include <common.h>
#include <game.h>
#include <dCourse.h>

struct LevelSpecial {
	u32 id;			// 0x00
	u32 settings;	// 0x04
	u16 name;		// 0x08
	u8 _0A[6];		// 0x0A
	u8 _10[0x9C];	// 0x10
	float x;		// 0xAC
	float y;		// 0xB0
	float z;		// 0xB4
	u8 _B8[0x318];	// 0xB8
	// Any variables you add to the class go here; starting at offset 0x3D0
	u64 eventFlag;	// 0x3D0
	u8 type;		// 0x3D4
	u8 effect;		// 0x3D5
	u8 lastEvState;	// 0x3D6
	u8 func;		// 0x3D7
	u32 keepTime;
	u32 setTime;
};


extern u16 TimeStopFlag;
extern u32 AlwaysDrawFlag;
extern u32 AlwaysDrawBranch;

extern float MarioDescentRate;
extern float MarioJumpMax;
extern float MarioJumpArc;
extern float MiniMarioJumpArc;
// extern float MarioSize;

extern float GlobalSpriteSize;
extern float GlobalSpriteSpeed;
extern float GlobalRiderSize;
extern char SizerOn;
extern char ZOrderOn;
extern int GlobalStarsCollected;

extern VEC2 BGScaleFront;
extern VEC2 BGScaleBack;
extern char BGScaleEnabled;

extern u32 GameTimer;

#define time *(u32*)((GameTimer) + 0x4)


static const float GlobalSizeFloatModifications [] = {1, 0.25, 0.5, 0.75, 1.25, 1.5, 1.75, 2, 2.5, 3, 4, 5, 6, 7, 8, 10 };
static const float GlobalRiderFloatModifications [] = {1, 0.6, 0.7, 0.9, 1, 1, 1, 1.1, 1.25, 1.5, 2, 2.5, 3, 3.5, 4, 5};
static const float BGScaleChoices[] = {0.1f, 0.15f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.9f, 1.0f, 1.125f, 1.25f, 1.5f, 1.75f, 2.0f, 2.25f, 2.5f};

bool NoMichaelBuble = false;

void LevelSpecial_Update(LevelSpecial *self);
bool ResetAfterLevel();

#define ACTIVATE	1
#define DEACTIVATE	0

fBase_c *FindActorByID(u32 id);


extern "C" void dAcPy_vf294(void *Mario, dStateBase_c *state, u32 unk);
void MarioStateChanger(void *Mario, dStateBase_c *state, u32 unk) {
	//OSReport("State: %p, %s", state, state->getName());

	if ((strcmp(state->getName(), "dAcPy_c::StateID_Balloon") == 0) && (NoMichaelBuble)) { return; }

	dAcPy_vf294(Mario, state, unk);
}

bool ResetAfterLevel(bool didItWork) {
	// TimeStopFlag = 0;
	MarioDescentRate = -4;
	MarioJumpMax = 3.628;
	MarioJumpArc = 2.5;
	MiniMarioJumpArc = 2.5;
	// MarioSize = 1.0;
	GlobalSpriteSize = 1.0;
	GlobalSpriteSpeed = 1.0;
	GlobalRiderSize = 1.0;
	SizerOn = 0;
	AlwaysDrawFlag = 0x9421FFF0;
	AlwaysDrawBranch = 0x7C0802A6;
	ZOrderOn = 0;
	GlobalStarsCollected = 0;
	NoMichaelBuble = false;
	BGScaleEnabled = 0;
	return didItWork;
}

void FuckinBubbles() {
	dCourse_c *course = dCourseFull_c::instance->get(GetAreaNum());
	bool thing = false;

	int zone = GetZoneNum();
	for (int i = 0; i < course->zoneSpriteCount[zone]; i++) {
		dCourse_c::sprite_s *spr = &course->zoneFirstSprite[zone][i];
		if (spr->type == 246 && (spr->settings & 0xF) == 8)
			thing = true;
	}

	if (thing) {
		OSReport("DISABLING EXISTING BUBBLES.\n");
		for (int i = 0; i < 4; i++)
			Player_Flags[i] &= ~4;
	}
}

bool LevelSpecial_Create(LevelSpecial *self) {
	char eventNum	= (self->settings >> 24)	& 0xFF;
	self->eventFlag = (u64)1 << (eventNum - 1);
	
	self->keepTime  = 0;
	
	self->type		= (self->settings)			& 15;
	self->effect	= (self->settings >> 4)		& 15;
	self->setTime	= (self->settings >> 8)     & 0xFFFF;

	self->lastEvState = 0xFF;
	
	LevelSpecial_Update(self);
	
	return true;
}

bool LevelSpecial_Execute(LevelSpecial *self) {
	if (self->keepTime > 0) {
		time = self->keepTime; }

	LevelSpecial_Update(self);
	return true;
}


void LevelSpecial_Update(LevelSpecial *self) {
	
	u8 newEvState = 0;
	if (dFlagMgr_c::instance->flags & self->eventFlag)
		newEvState = 1;
	
	if (newEvState == self->lastEvState)
		return;
		
	
	u8 offState;
	if (newEvState == ACTIVATE)
	{
		offState = (newEvState == 1) ? 1 : 0;

		switch (self->type) {
			// case 1:											// Time Freeze
			// 	TimeStopFlag = self->effect * 0x100;
			// 	break;
				
			case 2:											// Stop Timer
				self->keepTime  = time;
				break;
		
	
			case 3:											// Mario Gravity
				if (self->effect == 0)
				{											//Low grav
					MarioDescentRate = -2;
					MarioJumpArc = 0.5;
					MiniMarioJumpArc = 0.5;
					MarioJumpMax = 4.5;
				}
				else
				{											//Anti-grav
					MarioDescentRate = 0.5;
					MarioJumpArc = 4.0;
					MiniMarioJumpArc = 4.0;
					MarioJumpMax = 0.0;
				}
				break;
	
			case 4:											// Set Time
				time = (self->setTime << 0xC) - 1; // Possibly - 0xFFF?
				break;


			case 5:											// Global Enemy Size
				SizerOn = 3;

				GlobalSpriteSize = GlobalSizeFloatModifications[self->effect];
				GlobalRiderSize = GlobalRiderFloatModifications[self->effect];
				GlobalSpriteSpeed = GlobalRiderFloatModifications[self->effect];

				AlwaysDrawFlag = 0x38600001;
				AlwaysDrawBranch = 0x4E800020;
				break;
	
			case 6:											// Individual Enemy Size
				AlwaysDrawFlag = 0x38600001;
				AlwaysDrawBranch = 0x4E800020;

				if (self->effect == 0)
				{	
					SizerOn = 1;							// Nyb 5
				}
				else
				{											
					SizerOn = 2;							// Nyb 7
				}
				break;
		
			case 7:											// Z Order Hack
				ZOrderOn = 1;
				break;

			case 8:
				NoMichaelBuble = true;
				break;

			case 9:
				BGScaleEnabled = true;
				BGScaleFront.x = BGScaleChoices[(self->settings >> 20) & 15];
				BGScaleFront.y = BGScaleChoices[(self->settings >> 16) & 15];
				BGScaleBack.x = BGScaleChoices[(self->settings >> 12) & 15];
				BGScaleBack.y = BGScaleChoices[(self->settings >> 8) & 15];
				break;

			default:
				break;
		}
	}
	
	else
	{
		offState = (newEvState == 1) ? 0 : 1;

		switch (self->type) {
			// case 1:											// Time Freeze
			// 	TimeStopFlag = 0;
			// 	break;
				
			case 2:											// Stop Timer
				self->keepTime  = 0;
				break;
		
	
			case 3:											// Mario Gravity
				MarioDescentRate = -4;
				MarioJumpArc = 2.5;
				MiniMarioJumpArc = 2.5;
				MarioJumpMax = 3.628;
				break;
	
			case 4:											// Mario Size
				break;
		
			case 5:											// Global Enemy Size
				SizerOn = 0;

				GlobalSpriteSize = 1.0;
				GlobalRiderSize = 1.0;
				GlobalSpriteSpeed = 1.0;

				AlwaysDrawFlag = 0x9421FFF0;
				AlwaysDrawBranch = 0x7C0802A6;
				break;

			case 6:											// Individual Enemy Size
				SizerOn = 0;

				AlwaysDrawFlag = 0x9421FFF0;
				AlwaysDrawBranch = 0x7C0802A6;
				break;
		
			case 7:											// Z Order Hack
				ZOrderOn = 0;
				break;
				
			case 8:
				NoMichaelBuble = false;
				break;

			case 9:
				BGScaleEnabled = false;
				break;
	
			default:
				break;
		}
	}




	
	
	self->lastEvState = newEvState;
}

#undef time

//
// processed\../src/mrsun.cpp
//

#include <common.h>
#include <game.h>
#include <g3dhax.h>

#include "boss.h"

const char* MSarcNameList [] = {
	"mrsun",
	NULL	
};

class daMrSun_c : public dEn_c {
	int onCreate();
	int onDelete();
	int onExecute();
	int onDraw();

	mHeapAllocator_c allocator;
	m3d::mdl_c bodyModel;
	m3d::mdl_c glowModel;

	bool hasGlow;

	float Baseline;
	float SwoopSlope;
	float SpiralLoop;
	float yThreshold;
	float yAccel;
	Vec	swoopTarget;
	u32 timer;
	float xSpiralOffset;
	float ySpiralOffset;
	float swoopA;
	float swoopB;
	float swoopC;
	float swoopSpeed;
	float glowPos;
	short spinReduceZ;
	short spinReduceY;
	float spinStateOn;
	int dying;
	char sunDying;
	char killFlag;

	u64 eventFlag;


	void dieFall_Execute();
	static daMrSun_c *build();

	void updateModelMatrices();

	void playerCollision(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat1_Fireball_E_Explosion(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat2_IceBall_15_YoshiIce(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat5_Mario(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat13_Hammer(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCatA_PenguinMario(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat9_RollingObject(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat3_StarPower(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCatD_Drill(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat7_GroundPound(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat7_GroundPoundYoshi(ActivePhysics *apThis, ActivePhysics *apOther);

	USING_STATES(daMrSun_c);
	DECLARE_STATE(Follow);
	DECLARE_STATE(Swoop);
	DECLARE_STATE(Spiral);
	DECLARE_STATE(Spit);
	DECLARE_STATE(Spin);
	DECLARE_STATE(Wait);
};

daMrSun_c *daMrSun_c::build() {
	void *buffer = AllocFromGameHeap1(sizeof(daMrSun_c));
	return new(buffer) daMrSun_c;
}


CREATE_STATE(daMrSun_c, Follow);
CREATE_STATE(daMrSun_c, Swoop);
CREATE_STATE(daMrSun_c, Spiral);
CREATE_STATE(daMrSun_c, Spit);
CREATE_STATE(daMrSun_c, Spin);
CREATE_STATE(daMrSun_c, Wait);

#define ACTIVATE	1
#define DEACTIVATE	0




void daMrSun_c::playerCollision(ActivePhysics *apThis, ActivePhysics *apOther) {  DamagePlayer(this, apThis, apOther); }

bool daMrSun_c::collisionCat1_Fireball_E_Explosion(ActivePhysics *apThis, ActivePhysics *apOther) {
	return true;
}
bool daMrSun_c::collisionCat2_IceBall_15_YoshiIce(ActivePhysics *apThis, ActivePhysics *apOther) { 
	
	if (this->settings == 1) {  // It's a moon
		if (apOther->owner->name == 0x76) { // BROS_ICEBALL
			return true; 
			}
	}
	return false;
}
bool daMrSun_c::collisionCat9_RollingObject(ActivePhysics *apThis, ActivePhysics *apOther) { 
	this->timer = 0; 
	PlaySound(this, SE_EMY_DOWN);
	doStateChange(&StateID_DieFall);
	return true;
}
bool daMrSun_c::collisionCat13_Hammer(ActivePhysics *apThis, ActivePhysics *apOther) { 
	this->timer = 0;
	PlaySound(this, SE_EMY_DOWN);
	doStateChange(&StateID_DieFall);
	return true;
}
bool daMrSun_c::collisionCat3_StarPower(ActivePhysics *apThis, ActivePhysics *apOther) { 
	this->timer = 0; 
	PlaySound(this, SE_EMY_DOWN);
	doStateChange(&StateID_DieFall);
	return true;
}
bool daMrSun_c::collisionCatD_Drill(ActivePhysics *apThis, ActivePhysics *apOther) {
	DamagePlayer(this, apThis, apOther);
	return true;
}
bool daMrSun_c::collisionCat7_GroundPound(ActivePhysics *apThis, ActivePhysics *apOther) {
	DamagePlayer(this, apThis, apOther);
	return true;
}
bool daMrSun_c::collisionCat7_GroundPoundYoshi(ActivePhysics *apThis, ActivePhysics *apOther) {
	DamagePlayer(this, apThis, apOther);
	return true;
}
bool daMrSun_c::collisionCatA_PenguinMario(ActivePhysics *apThis, ActivePhysics *apOther) {
	DamagePlayer(this, apThis, apOther);
	return true;
}
bool daMrSun_c::collisionCat5_Mario(ActivePhysics *apThis, ActivePhysics *apOther) {
	DamagePlayer(this, apThis, apOther);
	return true;
}


void daMrSun_c::dieFall_Execute() {
	
	if (this->killFlag == 1) { return; }

	this->timer = this->timer + 1;
	 
	this->dying = this->dying + 0.15;
	
	this->pos.x = this->pos.x + 0.15;
	this->pos.y = this->pos.y - ((-0.2 * (this->dying*this->dying)) + 5);
	
	this->dEn_c::dieFall_Execute();
		
	if (this->timer > 450) {
		
		if ((this->settings >> 28) > 0) { 		
			this->kill();
			this->pos.y = this->pos.y + 800.0; 
			this->killFlag = 1;
			return;
		}
		
		dStageActor_c *Player = GetSpecificPlayerActor(0);
		if (Player == 0) { Player = GetSpecificPlayerActor(1); }
		if (Player == 0) { Player = GetSpecificPlayerActor(2); }
		if (Player == 0) { Player = GetSpecificPlayerActor(3); }
		
	
		if (Player == 0) { 
			this->pos.x = 0;
			doStateChange(&StateID_Follow); }
		else {
			Player->pos;
			this->pos.x = Player->pos.x - 300;
		}
				
		this->pos.y = this->Baseline; 
		
		this->aPhysics.addToList();
		doStateChange(&StateID_Follow);
	}
}


int daMrSun_c::onCreate() {
	allocator.link(-1, GameHeaps[0], 0, 0x20);

	if ((this->settings & 0xF) == 0) { // It's a sun
		hasGlow = true;

		nw4r::g3d::ResFile rf(getResource("mrsun", "g3d/sun.brres"));
		bodyModel.setup(rf.GetResMdl("Sun"), &allocator, 0x224, 1, 0);
		SetupTextures_Map(&bodyModel, 0);

		glowModel.setup(rf.GetResMdl("SunGlow"), &allocator, 0x224, 1, 0);
		SetupTextures_Boss(&glowModel, 0);
	}
	
	else { // It's a moon
		hasGlow = false;

		nw4r::g3d::ResFile rf(getResource("mrsun", "g3d/moon.brres"));
		bodyModel.setup(rf.GetResMdl("Moon"), &allocator, 0x224, 1, 0);
		SetupTextures_Map(&bodyModel, 0);
	}
	
	allocator.unlink();

	this->scale = (Vec){0.5, 0.5, 0.5};


	ActivePhysics::Info HitMeBaby;
	HitMeBaby.xDistToCenter = 0.0;
	HitMeBaby.yDistToCenter = 0.0;
	HitMeBaby.category1 = 0x3;
	HitMeBaby.category2 = 0x0;
	HitMeBaby.bitfield1 = 0x6F;

	if ((this->settings & 0xF) == 0) { // It's a sun
		HitMeBaby.bitfield2 = 0xffbafffc; 
		HitMeBaby.xDistToEdge = 24.0;
		HitMeBaby.yDistToEdge = 24.0;
	}	
	else { // It's a moon
		HitMeBaby.bitfield2 = 0xffbafffe; 
		HitMeBaby.xDistToEdge = 12.0;
		HitMeBaby.yDistToEdge = 12.0;
	}

	HitMeBaby.unkShort1C = 0;
	HitMeBaby.callback = &dEn_c::collisionCallback;


	this->aPhysics.initWithStruct(this, &HitMeBaby);
	this->aPhysics.addToList();

	this->Baseline = this->pos.y;
	this->SwoopSlope = 0.0;
	this->SpiralLoop = 0;
	this->yThreshold = 15.0;
	this->yAccel = 0.2;
	this->timer = 0;
	this->xSpiralOffset = 0.0;
	this->ySpiralOffset = 0.0;
	this->dying = -5;
	this->sunDying = 0;
	this->killFlag = 0;
	
	if (this->settings == 1)
		this->pos.z = 6000.0f; // moon
	else
		this->pos.z = 5750.0f; // sun


	char eventNum	= (this->settings >> 16) & 0xFF;

	this->eventFlag = (u64)1 << (eventNum - 1);


	
	doStateChange(&StateID_Follow);

	// this->onExecute();
	return true;
}

int daMrSun_c::onDelete() {
	return true;
}

int daMrSun_c::onExecute() {
	acState.execute();
	updateModelMatrices();
		
	if (dFlagMgr_c::instance->flags & this->eventFlag) {
		if (this->killFlag == 0 && acState.getCurrentState()->isNotEqual(&StateID_DieFall)) {
			this->kill();
			this->pos.y = this->pos.y + 800.0; 
			this->killFlag = 1;
			doStateChange(&StateID_DieFall);
		}
	}
		
	return true;
}

int daMrSun_c::onDraw() {
	bodyModel.scheduleForDrawing();
	if (hasGlow)
		glowModel.scheduleForDrawing();

	return true;
}


void daMrSun_c::updateModelMatrices() {
	// This won't work with wrap because I'm lazy.
	matrix.translation(pos.x, pos.y, pos.z);
	matrix.applyRotationYXZ(&rot.x, &rot.y, &rot.z);

	bodyModel.setDrawMatrix(matrix);
	bodyModel.setScale(&scale);
	bodyModel.calcWorld(false);

	if (hasGlow) {
		mMtx glowMatrix;
		short rotY;
		
		glowPos += 0.01666666666666;
		if (glowPos > 1) { glowPos = 0; }
		
		rotY = (1000 * sin(glowPos * 3.14)) + 500;


		glowMatrix.translation(pos.x, pos.y, pos.z);
		glowMatrix.applyRotationX(&rot.x);
		glowMatrix.applyRotationY(&rotY);

		glowModel.setDrawMatrix(glowMatrix);
		glowModel.setScale(&scale);
		glowModel.calcWorld(false);
	}
}


// Follow State

void daMrSun_c::beginState_Follow() { 
	this->timer = 0;
	this->rot.x = 18000;
	this->rot.y = 0;
	this->rot.z = 0;
}
void daMrSun_c::executeState_Follow() { 

	if (this->timer > 200) { this->doStateChange(&StateID_Wait); }

	this->direction = dSprite_c__getXDirectionOfFurthestPlayerRelativeToVEC3(this, this->pos);
	
	float speedDelta;
	if ((this->settings & 0xF) == 0) { speedDelta = 0.1; } // It's a sun
	else { speedDelta = 0.15; } // It's a moon


	if (this->direction == 0) {
		this->speed.x = this->speed.x + speedDelta;
		
		if (this->speed.x < 0) { this->speed.x = this->speed.x + (speedDelta / 2); }
		if (this->speed.x < 6.0) { this->speed.x = this->speed.x + (speedDelta); }
	}
	else {
		this->speed.x = this->speed.x - speedDelta;

		if (this->speed.x > 0) { this->speed.x = this->speed.x - (speedDelta / 2); }
		if (this->speed.x > 6.0) { this->speed.x = this->speed.x - (speedDelta); }
	}
	
	this->HandleXSpeed();
	
	
	float yDiff;
	yDiff = (this->Baseline - this->pos.y) / 8;
	this->speed.y = yDiff;
		
	this->HandleYSpeed();

	this->UpdateObjectPosBasedOnSpeedValuesReal();

	this->timer = this->timer + 1;
}
void daMrSun_c::endState_Follow() { 
	this->speed.y = 0;
}


// Swoop State

void daMrSun_c::beginState_Swoop() { 
	
	// Not enough space to swoop, spit instead.
	if (this->swoopTarget.y < (this->pos.y - 50)) { doStateChange(&StateID_Spit); }
	if (((this->pos.x - 96) < this->swoopTarget.x) && (this->swoopTarget.x < (this->pos.x + 96))) { doStateChange(&StateID_Spit); }

	if ((this->settings & 0xF) == 0) { 
		this->swoopTarget.y = this->swoopTarget.y - 16;
	} // It's a sun
	
	else { 
		this->swoopTarget.y = this->swoopTarget.y - 4;
	} // It's a moon	
	
	
	float x1, x2, x3, y1, y2, y3;

	x1 = this->pos.x - this->swoopTarget.x;
	x2 = 0;
	x3 = -x1;

	y1 = this->pos.y - this->swoopTarget.y;
	y2 = 0;
	y3 = y1;
	
	float denominator = (x1 - x2) * (x1 - x3) * (x2 - x3);
	this->swoopA      = (x3 * (y2 - y1) + x2 * (y1 - y3) + x1 * (y3 - y2)) / denominator;
	this->swoopB      = (x3*x3 * (y1 - y2) + x2*x2 * (y3 - y1) + x1*x1 * (y2 - y3)) / denominator;
	this->swoopC      = (x2 * x3 * (x2 - x3) * y1 + x3 * x1 * (x3 - x1) * y2 + x1 * x2 * (x1 - x2) * y3) / denominator;

	this->swoopSpeed = x3 * 2 / 75;
	
	
	PlaySound(this, 284);
	
}
void daMrSun_c::executeState_Swoop() { 

	// Everything is calculated up top, just need to modify it.

	this->pos.x = this->pos.x + this->swoopSpeed;

	this->pos.y = ( this->swoopA*(this->pos.x - this->swoopTarget.x)*(this->pos.x - this->swoopTarget.x) + this->swoopB*(this->pos.x - this->swoopTarget.x) + this->swoopC ) + this->swoopTarget.y;

	if (this->pos.y > this->Baseline) { doStateChange(&StateID_Follow); }

}
void daMrSun_c::endState_Swoop() { 
	this->speed.y = 0;
}



// Spiral State

void daMrSun_c::beginState_Spiral() { 

	this->SpiralLoop = 0;
	this->xSpiralOffset = this->pos.x;
	this->ySpiralOffset = this->pos.y;

	PlaySound(this, 284);
}
void daMrSun_c::executeState_Spiral() { 

	float Loops;
	float Magnitude;
	float Period;

	Loops = 6.0;
	Magnitude = 11.0;

	// Use a period of 0.1 for the moon
	if ((this->settings & 0xF) == 0) { Period = 0.1; } // It's a sun
	else { Period = 0.125; } // It's a moon	

	this->pos.x = this->xSpiralOffset + Magnitude*((this->SpiralLoop * cos(this->SpiralLoop)));
	this->pos.y = this->ySpiralOffset + Magnitude*((this->SpiralLoop * sin(this->SpiralLoop)));

	this->SpiralLoop = this->SpiralLoop + Period;

	if (this->SpiralLoop > (3.14 * Loops)) { doStateChange(&StateID_Follow); }

}
void daMrSun_c::endState_Spiral() { }



// Spit State

void daMrSun_c::beginState_Spit() { 

	this->timer = 0;
	this->spinStateOn = 1;

}
void daMrSun_c::executeState_Spit() { 
	
	if (this->timer == 10) {

		PlaySound(this, 431);
	
		this->direction = dSprite_c__getXDirectionOfFurthestPlayerRelativeToVEC3(this, this->pos);
		
		float neg = -1.0;
		if (this->direction == 0) { neg = 1.0; }
		

		if ((this->settings & 0xF) == 0) { 
			dStageActor_c *spawner = CreateActor(106, 0, this->pos, 0, 0);
			spawner->speed.x = 6.0 * neg;
			spawner->speed.y = -2.5;
			spawner->pos.z = 5550.0;
			
			spawner = CreateActor(106, 0, this->pos, 0, 0);
			spawner->speed.x = 0.0 * neg;
			spawner->speed.y = -6.0;
			spawner->pos.z = 5550.0;
		
			spawner = CreateActor(106, 0, this->pos, 0, 0);
			spawner->speed.x = 3.5 * neg;
			spawner->speed.y = -6.0;
			spawner->pos.z = 5550.0;
		} // It's a sun
		
		
		else { 
			dStageActor_c *spawner = CreateActor(118, 0, this->pos, 0, 0);
			spawner->speed.x = 6.0 * neg;
			spawner->speed.y = -2.5;
			spawner->pos.z = 5550.0;
			*((u32 *) (((char *) spawner) + 0x3DC)) = this->id;
			
			spawner = CreateActor(118, 0, this->pos, 0, 0);
			spawner->speed.x = 0.0 * neg;
			spawner->speed.y = -6.0;
			spawner->pos.z = 5550.0;
			*((u32 *) (((char *) spawner) + 0x3DC)) = this->id;
		
			spawner = CreateActor(118, 0, this->pos, 0, 0);
			spawner->speed.x = 3.5 * neg;
			spawner->speed.y = -6.0;
			spawner->pos.z = 5550.0;
			*((u32 *) (((char *) spawner) + 0x3DC)) = this->id;
		} // It's a moon	

	}
	
	this->timer = this->timer + 1;

	if (this->timer > 30) { doStateChange(&StateID_Follow); }

}
void daMrSun_c::endState_Spit() { 
	this->spinStateOn = 0;
}



// Spin State

void daMrSun_c::beginState_Spin() { 
	this->spinReduceZ = 0;
	this->spinReduceY = 0;
}
void daMrSun_c::executeState_Spin() { 
	
	PlaySound(this, 282);

	this->direction = dSprite_c__getXDirectionOfFurthestPlayerRelativeToVEC3(this, this->pos);
	
	if (this->direction == 0) {
		this->speed.x = this->speed.x + 0.2;

		if (this->speed.x < 0) { this->speed.x = this->speed.x + (0.2 / 2); }
		if (this->speed.x < 80.0) { this->speed.x = this->speed.x + (0.2 * 2); }
	}
	else {
		this->speed.x = this->speed.x - 0.2;

		if (this->speed.x > 0) { this->speed.x = this->speed.x - (0.2 / 2); }
		if (this->speed.x > 80.0) { this->speed.x = this->speed.x - (0.2 * 2); }
	}
	
	this->HandleXSpeed();
	this->UpdateObjectPosBasedOnSpeedValuesReal();

	this->timer = this->timer + 1;

	short rotBonus;
	if (this->timer < 60) { rotBonus = this->timer; }
	else { rotBonus = 120 - this->timer; }

	// 1-59 + 60-1 = 3542
		
//	if (this->timer > 100) { 
//		if (this->spinReduceZ = 0) { 
//			this->spinReduceZ = this->rot.z / 20; }
//		if (this->spinReduceY = 0) { 
//			this->spinReduceY = this->rot.y / 20; }
//			
//		this->rot.z = this->rot.z - this->spinReduceZ;
//		this->rot.y = this->rot.y - this->spinReduceY; }
//		
//	else {
		this->rot.z = this->rot.z + (55.1 * rotBonus);
		this->rot.y = this->rot.y + (18.4 * rotBonus); //}


	float spitspeed;
	if ((this->settings & 0xF) == 0) { spitspeed = 3.0; } // It's a sun
	else { spitspeed = 4.0;  } // It's a moon	

	int randomBall;
	randomBall = GenerateRandomNumber(8);
	if (randomBall == 1) {
		int direction;
		direction = GenerateRandomNumber(8);
		
		float xlaunch;
		float ylaunch;
		
		if (direction == 0) { 
			xlaunch = spitspeed;
			ylaunch = 0.0; }
		else if (direction == 1) { // SE
			xlaunch = spitspeed;
			ylaunch = spitspeed; }
		else if (direction == 2) { // S
			xlaunch = 0.0;
			ylaunch = spitspeed; }
		else if (direction == 3) { // SW
			xlaunch = -spitspeed;
			ylaunch = spitspeed; }
		else if (direction == 4) {	// W
			xlaunch = -spitspeed;
			ylaunch = 0.0; }
		else if (direction == 5) {	// NW
			xlaunch = -spitspeed;
			ylaunch = -spitspeed; }
		else if (direction == 6) {	// N
			xlaunch = 0.0;
			ylaunch = -spitspeed; }
		else if (direction == 7) {	// NE
			xlaunch = spitspeed;
			ylaunch = -spitspeed; }
		
		PlaySound(this, 431);

		if ((this->settings & 0xF) == 0) { 
			dStageActor_c *spawner = CreateActor(106, 0, this->pos, 0, 0);
			spawner->speed.x = xlaunch;
			spawner->speed.y = ylaunch;
			spawner->pos.z = 5550.0;
		} // It's a sun

		else { 
			dStageActor_c *spawner = CreateActor(118, 0, this->pos, 0, 0);
			spawner->speed.x = xlaunch;
			spawner->speed.y = ylaunch;
			spawner->pos.z = 5550.0;
			
			*((u32 *) (((char *) spawner) + 0x3DC)) = this->id;			
		} // It's a moon	
	}

	if (this->timer > 120) { this->doStateChange(&StateID_Follow); }
	
}
void daMrSun_c::endState_Spin() { 

	this->rot.x = 18000;
	this->rot.y = 0;
	this->rot.z = 0;

	this->speed.x = 0;
}



// Wait State

void daMrSun_c::beginState_Wait() {


	this->timer = 0;
	this->speed.x = 0.0;

	dStageActor_c *Player = GetSpecificPlayerActor(0);
	if (Player == 0) { Player = GetSpecificPlayerActor(1); }
	if (Player == 0) { Player = GetSpecificPlayerActor(2); }
	if (Player == 0) { Player = GetSpecificPlayerActor(3); }
	if (Player == 0) { doStateChange(&StateID_Follow); }
	
	this->swoopTarget = Player->pos;
}
void daMrSun_c::executeState_Wait() { 
	int Choice;
	int TimerMax;
	
	if ((this->settings & 0xF) == 0) { TimerMax = 60; } // It's a sun
	else { TimerMax = 30; } // It's a moon	
	
	if (this->timer > TimerMax) {

		Choice = GenerateRandomNumber(9);


		if (Choice == 0) { doStateChange(&StateID_Spit); }
		else if (Choice == 1) { doStateChange(&StateID_Spit); }
		else if (Choice == 2) { doStateChange(&StateID_Spin); }
		else if (Choice == 3) { doStateChange(&StateID_Spiral); }
		else { doStateChange(&StateID_Swoop); }
		
	}

	this->timer = this->timer + 1;
}
void daMrSun_c::endState_Wait() {
	this->timer = 0;
}



//
// processed\../src/boss.cpp
//

#include "boss.h"



void DamagePlayer(dEn_c *actor, ActivePhysics *apThis, ActivePhysics *apOther) {

	actor->dEn_c::playerCollision(apThis, apOther);
	actor->_vf220(apOther->owner);

	// fix multiple player collisions via megazig
	actor->deathInfo.isDead = 0;
	actor->flags_4FC |= (1<<(31-7));
	if (apOther->owner->which_player == 255 ) {
		actor->counter_504[0] = 0;
		actor->counter_504[1] = 0;
		actor->counter_504[2] = 0;
		actor->counter_504[3] = 0;
	}
	else {
		actor->counter_504[apOther->owner->which_player] = 0;
	}
}


void SetupKameck(daBoss *actor, daKameckDemo *Kameck) {

	// Stop the BGM Music
	StopBGMMusic();

	// Set the necessary Flags and make Mario enter Demo Mode
	dStage32C_c::instance->freezeMarioBossFlag = 1;
	WLClass::instance->_4 = 4;
	WLClass::instance->_8 = 0;

	MakeMarioEnterDemoMode();

	// Make sure to use the correct position
	Vec pos = (Vec){actor->pos.x - 124.0, actor->pos.y + 104.0, 3564.0};
	S16Vec rot = (S16Vec){0, 0, 0};

	// Create And use Kameck
	actor->Kameck = (daKameckDemo*)actor->createChild(KAMECK_FOR_CASTLE_DEMO, (dStageActor_c*)actor, 0, &pos, &rot, 0);
	actor->Kameck->doStateChange(&daKameckDemo::StateID_DemoWait);	

}


void CleanupKameck(daBoss *actor, daKameckDemo *Kameck) {
	// Clean up the flags and Kameck
	dStage32C_c::instance->freezeMarioBossFlag = 0;
	WLClass::instance->_8 = 1;

	MakeMarioExitDemoMode();
	StartBGMMusic();

	actor->Kameck->Delete(1);
}


bool GrowBoss(daBoss *actor, daKameckDemo *Kameck, float initialScale, float endScale, float yPosModifier, int timer) {
	if (timer == 130) { actor->Kameck->doStateChange(&daKameckDemo::StateID_DemoSt); }
	if (timer == 400) { actor->Kameck->doStateChange(&daKameckDemo::StateID_DemoSt2); }

	float scaleSpeed, yPosScaling;

	if (timer == 150) { PlaySound(actor, SE_BOSS_IGGY_WANWAN_TO_L);  }
	
	if ((timer > 150) && (timer < 230)) {
		scaleSpeed = (endScale -initialScale) / 80.0;
	
		float modifier;

		modifier = initialScale + ((timer - 150) * scaleSpeed);
		
		actor->scale = (Vec){modifier, modifier, modifier};
		actor->pos.y = actor->pos.y + (yPosModifier/80.0);
	}

	if (timer == 360) { 
		Vec tempPos = (Vec){actor->pos.x - 40.0, actor->pos.y + 120.0, 3564.0};
		SpawnEffect("Wm_ob_greencoinkira", 0, &tempPos, &(S16Vec){0,0,0}, &(Vec){1.0, 1.0, 1.0});
		SpawnEffect("Wm_mr_yoshiicehit_a", 0, &tempPos, &(S16Vec){0,0,0}, &(Vec){1.0, 1.0, 1.0});
		SpawnEffect("Wm_mr_yoshiicehit_b", 0, &tempPos, &(S16Vec){0,0,0}, &(Vec){1.0, 1.0, 1.0});
		SpawnEffect("Wm_ob_redringget", 0, &tempPos, &(S16Vec){0,0,0}, &(Vec){1.0, 1.0, 1.0});
		SpawnEffect("Wm_ob_keyget01", 0, &tempPos, &(S16Vec){0,0,0}, &(Vec){1.0, 1.0, 1.0});
		SpawnEffect("Wm_ob_greencoinkira_a", 0, &tempPos, &(S16Vec){0,0,0}, &(Vec){1.0, 1.0, 1.0});
		SpawnEffect("Wm_ob_keyget01_c", 0, &tempPos, &(S16Vec){0,0,0}, &(Vec){1.0, 1.0, 1.0});
	}

	if (timer > 420) { return true; }
	return false;
}


void OutroSetup(daBoss *actor) {
	actor->removeMyActivePhysics();

	StopBGMMusic();

	WLClass::instance->_4 = 5;
	WLClass::instance->_8 = 0;
	dStage32C_c::instance->freezeMarioBossFlag = 1;

	nw4r::snd::SoundHandle handle;
	PlaySoundWithFunctionB4(SoundRelatedClass, &handle, SE_BOSS_CMN_DAMAGE_LAST, 1);
}


bool ShrinkBoss(daBoss *actor, Vec *pos, float scale, int timer) {
	// Adjust actor to equal the scale of your boss / 80.
	actor->scale.x -= scale / 80.0;
	actor->scale.y -= scale / 80.0;
	actor->scale.z -= scale / 80.0;

	// actor->pos.y += 2.0;
	
	if (timer == 30) {  
		SpawnEffect("Wm_ob_starcoinget_gl", 0, pos, &(S16Vec){0,0,0}, &(Vec){2.0, 2.0, 2.0});
		SpawnEffect("Wm_mr_vshipattack_hosi", 0, pos, &(S16Vec){0,0,0}, &(Vec){2.0, 2.0, 2.0});
		SpawnEffect("Wm_ob_keyget01_b", 0, pos, &(S16Vec){0,0,0}, &(Vec){2.0, 2.0, 2.0});
	}

	if (actor->scale.x < 0) { return true; }
	else { return false; }
}


void BossExplode(daBoss *actor, Vec *pos) {
	actor->scale.x = 0.0;
	actor->scale.y = 0.0;
	actor->scale.z = 0.0;
	
	SpawnEffect("Wm_ob_keyget02", 0, pos, &(S16Vec){0,0,0}, &(Vec){2.0, 2.0, 2.0});
	actor->dying = 1;
	actor->timer = 0;

	nw4r::snd::SoundHandle handle;
	PlaySoundWithFunctionB4(SoundRelatedClass, &handle, STRM_BGM_SHIRO_BOSS_CLEAR, 1);

	//MakeMarioEnterDemoMode();
	BossGoalForAllPlayers();
}

void BossGoalForAllPlayers() {
	for (int i = 0; i < 4; i++) {
		daPlBase_c *player = GetPlayerOrYoshi(i);
		if (player)
			player->setAnimePlayStandardType(2);
	}
}


void PlayerVictoryCries(daBoss *actor) {
	UpdateGameMgr();
	/*nw4r::snd::SoundHandle handle1, handle2, handle3, handle4;

	dAcPy_c *players[4];
	for (int i = 0; i < 4; i++)
		players[i] = (dAcPy_c *)GetSpecificPlayerActor(i);

	if (players[0] && strcmp(players[0]->states2.getCurrentState()->getName(), "dAcPy_c::StateID_Balloon"))
		PlaySoundWithFunctionB4(SoundRelatedClass, &handle1, SE_VOC_MA_CLEAR_BOSS, 1);
	if (players[1] && strcmp(players[1]->states2.getCurrentState()->getName(), "dAcPy_c::StateID_Balloon"))
		PlaySoundWithFunctionB4(SoundRelatedClass, &handle2, SE_VOC_LU_CLEAR_BOSS, 1);
	if (players[2] && strcmp(players[2]->states2.getCurrentState()->getName(), "dAcPy_c::StateID_Balloon"))
		PlaySoundWithFunctionB4(SoundRelatedClass, &handle3, SE_VOC_KO_CLEAR_BOSS, 1);
	if (players[3] && strcmp(players[3]->states2.getCurrentState()->getName(), "dAcPy_c::StateID_Balloon"))
		PlaySoundWithFunctionB4(SoundRelatedClass, &handle4, SE_VOC_KO2_CLEAR_BOSS, 1);*/
}

//
// processed\../src/firelaser.cpp
//

#include <common.h>
#include <game.h>
#include <g3dhax.h>

class daFireLaser_c : public dEn_c {
	int onCreate();
	int onDelete();
	int onExecute();
	int onDraw();

	static daFireLaser_c *build();

	int timer;
	float spitspeed;
	char direction;
	u64 eventFlag;

	USING_STATES(daFireLaser_c);
	DECLARE_STATE(pewpewpew);
};

daFireLaser_c *daFireLaser_c::build() {
	void *buffer = AllocFromGameHeap1(sizeof(daFireLaser_c));
	return new(buffer) daFireLaser_c;
}


CREATE_STATE(daFireLaser_c, pewpewpew);




int daFireLaser_c::onCreate() {

	this->timer = 0;
	this->direction = this->settings & 0xF;
	this->spitspeed = 8.0;
	
	char eventNum	= (this->settings >> 16) & 0xFF;
	this->eventFlag = (u64)1 << (eventNum - 1);

	
	doStateChange(&StateID_pewpewpew);
	this->onExecute();
	return true;
}

int daFireLaser_c::onDelete() {
	return true;
}

int daFireLaser_c::onExecute() {
	acState.execute();
	return true;
}

int daFireLaser_c::onDraw() {
	return true;
}



// Pew Pew State

void daFireLaser_c::beginState_pewpewpew() { 
	this->timer = 0;
}
void daFireLaser_c::executeState_pewpewpew() { 
	
	
	if (dFlagMgr_c::instance->flags & this->eventFlag) {
		
		this->timer = this->timer + 1;
	
		if (this->timer < 20) {
			float xlaunch;
			float ylaunch;
			
			if (this->direction == 0) { 
				xlaunch = this->spitspeed;
				ylaunch = 0.0; }
			else if (this->direction == 1) { // SE
				xlaunch = this->spitspeed;
				ylaunch = this->spitspeed; }
			else if (this->direction == 2) { // S
				xlaunch = 0.0;
				ylaunch = this->spitspeed; }
			else if (this->direction == 3) { // SW
				xlaunch = -this->spitspeed;
				ylaunch = this->spitspeed; }
			else if (this->direction == 4) {	// W
				xlaunch = -this->spitspeed;
				ylaunch = 0.0; }
			else if (this->direction == 5) {	// NW
				xlaunch = -this->spitspeed;
				ylaunch = -this->spitspeed; }
			else if (this->direction == 6) {	// N
				xlaunch = 0.0;
				ylaunch = -this->spitspeed; }
			else if (this->direction == 7) {	// NE
				xlaunch = this->spitspeed;
				ylaunch = -this->spitspeed; }
			
			
			dStageActor_c *spawner = CreateActor(106, 0, this->pos, 0, 0);
			spawner->speed.x = xlaunch;
			spawner->speed.y = ylaunch;
		}
		
		if (this->timer > 60) {
			this->timer = 0;
		}

	}
	
	else { this->timer = 0; }
	
}
void daFireLaser_c::endState_pewpewpew() { 

}














//
// processed\../src/poweruphax.cpp
//

#include <common.h>
#include <game.h>
#include <g3dhax.h>


void ThwompHammer(dEn_c *thwomp, ActivePhysics *apThis, ActivePhysics *apOther) {
	if (thwomp->name == 0x51) {
		thwomp->dEn_c::collisionCat13_Hammer(apThis, apOther);
	}
	return;
}

void BooHammer(dEn_c *boo, ActivePhysics *apThis, ActivePhysics *apOther) {
	if (boo->name == 0xB0) {
		boo->dEn_c::collisionCat13_Hammer(apThis, apOther);
	}
	return;
}

void UrchinHammer(dEn_c *urchin, ActivePhysics *apThis, ActivePhysics *apOther) {
	return;
}


#include "poweruphax.h"

void SetCullModeForMaterial(m3d::mdl_c *model, int materialID, GXCullMode mode);


dHammerSuitRenderer_c *dHammerSuitRenderer_c::build() {
	return new dHammerSuitRenderer_c;
}

dHammerSuitRenderer_c::dHammerSuitRenderer_c() { }
dHammerSuitRenderer_c::~dHammerSuitRenderer_c() { }

void dHammerSuitRenderer_c::setup(dPlayerModelHandler_c *handler) {
	setup(handler, 0);
}

void dHammerSuitRenderer_c::setup(dPlayerModelHandler_c *handler, int sceneID) {
	victim = (dPlayerModel_c*)handler->mdlClass;

	allocator.link(-1, GameHeaps[0], 0, 0x20);

	nw4r::g3d::ResFile rf(getResource("hammerM", "g3d/suit.brres"));

	if (victim->player_id_2 <= 1) {
		helmet.setup(rf.GetResMdl((victim->player_id_2 == 0) ? "marioHelmet" : "luigiHelmet"), &allocator, 0, 1, 0);
		SetupTextures_MapObj(&helmet, sceneID);
	}

	const char *shellNames[] = {
		"shell", "shell", "shell", "shell", "shell"
	};
	shell.setup(rf.GetResMdl(shellNames[victim->player_id_2]), &allocator, 0, 1, 0);
	SetupTextures_MapObj(&shell, sceneID);

	allocator.unlink();


	victimModel = &victim->models[0].body;
	nw4r::g3d::ResMdl *playerResMdl =
		(nw4r::g3d::ResMdl*)(((u32)victimModel->scnObj) + 0xE8);

	//headNodeID = playerResMdl->GetResNode("player_head").GetID();
	if (victim->player_id_2 <= 1)
		headNodeID = playerResMdl->GetResNode("face_1").GetID();
	rootNodeID = playerResMdl->GetResNode("skl_root").GetID();
}

void dHammerSuitRenderer_c::draw() {
	if (victim->powerup_id != 7)
		return;

	if (victim->player_id_2 <= 1) {
		// Materials: 2=hair 3=hat; Modes: BACK=visible ALL=invisible
		SetCullModeForMaterial(&victim->getCurrentModel()->head, 3, GX_CULL_ALL);

		Mtx headMtx;
		victimModel->getMatrixForNode(headNodeID, headMtx);

		helmet.setDrawMatrix(headMtx);
		helmet.setScale(1.0f, 1.0f, 1.0f);
		helmet.calcWorld(false);

		helmet.scheduleForDrawing();
	}

	Mtx rootMtx;
	victimModel->getMatrixForNode(rootNodeID, rootMtx);

	shell.setDrawMatrix(rootMtx);
	shell.setScale(1.0f, 1.0f, 1.0f);
	shell.calcWorld(false);

	shell.scheduleForDrawing();
}








// NEW VERSION
void CrapUpPositions(Vec *out, const Vec *in);

void dStockItem_c::setScalesOfSomeThings() {
	nw4r::lyt::Pane *ppos = N_forUse_PPos[playerCount];

	int howManyPlayers = 0;
	for (int i = 0; i < 4; i++) {
		if (isPlayerActive[i]) {
			int picID = getIconPictureIDforPlayer(howManyPlayers);
			int charID = Player_ID[i];

			if (picID != 24) {
				nw4r::lyt::Picture *pic = P_icon[picID];

				Vec in, out;

				in.x = pic->effectiveMtx[0][3];
				in.y = pic->effectiveMtx[1][3];
				in.z = pic->effectiveMtx[2][3];

				CrapUpPositions(&out, &in);

				u8 *wmp = (u8*)player2d[charID];
				*((float*)(wmp+0xAC)) = out.x;
				*((float*)(wmp+0xB0)) = out.y;
				*((float*)(wmp+0xB4)) = out.z;
				*((float*)(wmp+0x220)) = 0.89999998f;
				*((float*)(wmp+0x224)) = 0.89999998f;
				*((float*)(wmp+0x228)) = 0.89999998f;
				*((float*)(wmp+0x25C)) = 26.0f;
			}
			howManyPlayers++;
		}
	}


	for (int i = 0; i < 8; i++) {
		u8 *item = (u8*)newItemPtr[i];

		nw4r::lyt::Pane *icon = newIconPanes[i];

		Vec in, out;
		in.x = icon->effectiveMtx[0][3];
		in.y = icon->effectiveMtx[1][3];
		in.z = icon->effectiveMtx[2][3];

		CrapUpPositions(&out, &in);

		*((float*)(item+0xAC)) = out.x;
		*((float*)(item+0xB0)) = out.y;
		*((float*)(item+0xB4)) = out.z;
		*((float*)(item+0x1F4)) = P_buttonBase[i]->scale.x;
		*((float*)(item+0x1F8)) = P_buttonBase[i]->scale.y;
		*((float*)(item+0x1FC)) = 1.0f;
	}


	nw4r::lyt::Pane *shdRoot = shadow->rootPane;
	shdRoot->trans.x = N_stockItem->effectiveMtx[0][3];
	shdRoot->trans.y = N_stockItem->effectiveMtx[1][3];
	shdRoot->trans.z = N_stockItem->effectiveMtx[2][3];
	shdRoot->scale.x = N_stockItem_01->effectiveMtx[0][0];
	shdRoot->scale.y = N_stockItem_01->effectiveMtx[1][1];

	for (int i = 0; i < 7; i++)
		shadow->buttonBases[i]->scale = newButtonBase[i]->scale;
	shadow->hammerButtonBase->scale = newButtonBase[7]->scale;
}



//
// processed\../src/randtiles.cpp
//

#include <game.h>

class RandomTileData {
public:
	enum Type {
		CHECK_NONE = 0,
		CHECK_HORZ = 1,
		CHECK_VERT = 2,
		CHECK_BOTH = 3
	};

	enum Special {
		SP_NONE = 0,
		SP_VDOUBLE_TOP = 1,
		SP_VDOUBLE_BOTTOM = 2
	};

	class NameList {
	public:
		u32 count;
		u32 offsets[1]; // variable size

		const char *getName(int index) {
			return ((char*)this) + offsets[index];
		}

		bool contains(const char *name) {
			for (int i = 0; i < count; i++) {
				if (strcmp(name, getName(i)) == 0)
					return true;
			}

			return false;
		}
	};

	class Entry {
	public:
		u8 lowerBound, upperBound;
		u8 count, type;
		u32 tileNumOffset;

		u8 *getTileNums() {
			return ((u8*)this) + tileNumOffset;
		}
	};

	class Section {
	public:
		u32 nameListOffset;
		u32 entryCount;
		Entry entries[1]; // variable size

		NameList *getNameList() {
			return (NameList*)(((u32)this) + nameListOffset);
		}
	};

	u32 magic;
	u32 sectionCount;
	u32 offsets[1]; // variable size

	Section *getSection(int id) {
		return (Section*)(((char*)this) + offsets[id]);
	}

	Section *getSection(const char *name);

	static RandomTileData *instance;
};

class RTilemapClass : public TilemapClass {
public:
	// NEWER ADDITIONS
	RandomTileData::Section *sections[4];
};

RandomTileData::Section *RandomTileData::getSection(const char *name) {
	for (int i = 0; i < sectionCount; i++) {
		RandomTileData::Section *sect = getSection(i);

		if (sect->getNameList()->contains(name))
			return sect;
	}

	return 0;
}


// Real tile handling code

RandomTileData *RandomTileData::instance = 0;

dDvdLoader_c s_levelInfoLoader;
bool s_levelInfoLoaded = false;
dDvdLoader_c RandTileLoader;

// This is a bit hacky but I'm lazy
bool LoadLevelInfo() {
	if (s_levelInfoLoaded)
		return true;

	void *data = s_levelInfoLoader.load("/NewerRes/LevelInfo.bin");
	if (data) {
		s_levelInfoLoaded = true;
		return true;
	}

	return false;
}

extern "C" bool RandTileLoadHook() {
	// OSReport("Trying to load...");
	void *buf = RandTileLoader.load("/NewerRes/RandTiles.bin");
	bool LIresult = LoadLevelInfo();
	if (buf == 0) {
		// OSReport("Failed.\n");
		return false;
	} else {
		// OSReport("Successfully loaded RandTiles.bin [%p].\n", buf);
		RandomTileData::instance = (RandomTileData*)buf;
		return LIresult;
	}
}


extern "C" void IdentifyTilesets(RTilemapClass *self) {
	self->_C0C = 0xFFFFFFFF;

	for (int i = 0; i < 4; i++) {
		const char *tilesetName = BGDatClass::instance->getTilesetName(self->areaID, i);

		self->sections[i] = RandomTileData::instance->getSection(tilesetName);
		// OSReport("[%d] Chose %p for %s\n", i, self->sections[i], tilesetName);
	}
}

extern "C" void TryAndRandomise(RTilemapClass *self, BGRender *bgr) {
	int fullTile = bgr->tileToPlace & 0x3FF;
	int tile = fullTile & 0xFF;
	int tileset = fullTile >> 8;

	RandomTileData::Section *rtSect = self->sections[tileset];
	if (rtSect == 0)
		return;

	for (int i = 0; i < rtSect->entryCount; i++) {
		RandomTileData::Entry *entry = &rtSect->entries[i];

		if (tile >= entry->lowerBound && tile <= entry->upperBound) {
			// Found it!!
			// Try to make one until we meet the conditions
			u8 type = entry->type & 3;
			u8 special = entry->type >> 2;

			u8 *tileNums = entry->getTileNums();
			u16 chosen = 0xFF;

			// If it's the top special, then ignore this tile, we'll place that one
			// once we choose the bottom one
			if (special == RandomTileData::SP_VDOUBLE_TOP)
				break;

			u16 *top = 0, *left = 0, *right = 0, *bottom = 0;
			if (type == RandomTileData::CHECK_HORZ || type == RandomTileData::CHECK_BOTH) {
				left = self->getPointerToTile((bgr->curX - 1) * 16, bgr->curY * 16);
				right = self->getPointerToTile((bgr->curX + 1) * 16, bgr->curY * 16);
			}

			if (type == RandomTileData::CHECK_VERT || type == RandomTileData::CHECK_BOTH) {
				top = self->getPointerToTile(bgr->curX * 16, (bgr->curY - 1) * 16);
				bottom = self->getPointerToTile(bgr->curX * 16, (bgr->curY + 1) * 16);
			}

			int attempts = 0;
			while (true) {
				// is there even a point to using that special random function?
				chosen = (tileset << 8) | tileNums[MakeRandomNumberForTiles(entry->count)];

				// avoid infinite loops
				attempts++;
				if (attempts > 5)
					break;

				if (top != 0 && *top == chosen)
					continue;
				if (bottom != 0 && *bottom == chosen)
					continue;
				if (left != 0 && *left == chosen)
					continue;
				if (right != 0 && *right == chosen)
					continue;
				break;
			}

			bgr->tileToPlace = chosen;

			if (special == RandomTileData::SP_VDOUBLE_BOTTOM) {
				if (top == 0)
					top = self->getPointerToTile(bgr->curX * 16, (bgr->curY - 1) * 16);

				*top = (chosen - 0x10);
			}

			return;
		}
	}
}



//
// processed\../src/objkinoko.cpp
//

#include <game.h>
#include <g3dhax.h>


class SomethingAboutShrooms {
	public:
		m3d::mdl_c models[3];
		float scale, _C4, offsetToEdge;
		m3d::anmTexPat_c animations[3];

		struct info_s {
			const char *leftName;
			const char *middleName;
			const char *rightName;
			float size; // 8 for small, 16 for big
			const char *lrName;
			const char *middleNameAgain;
		};

		void setup(mAllocator_c *allocator,
				nw4r::g3d::ResFile *resFile, info_s *info,
				float length, float colour, float scale);

		// plus more methods I don't know

		void drawWithMatrix(float yOffset, mMtx *matrix);
};


class dRotatorThing_c {
	public:
		s16 _p5, output, _p1, _p2, _p3;
		s16 _p6, _p7, _p0;
		u32 someBool;

		void setup(s16 a, s16 b, s16 c, s16 d, s16 initialRotation, s16 f, s16 g, s16 h);
		s16 execute();
};


class daObjKinoko_c : public dStageActor_c {
	public:
		mHeapAllocator_c allocator;
		nw4r::g3d::ResFile resFile;
		SomethingAboutShrooms renderer;
		StandOnTopCollider colliders[3];
		dRotatorThing_c xRotator;
		dRotatorThing_c zRotator;
		u8 thickness, touchCompare;

		void loadModels(int thickness, float length, float colour, float scale);
		void addAllColliders();
		void updateAllColliders();
		void removeAllColliders();
		u8 checkIfTouchingObject();

		int onCreate();
		int onExecute();
		int onDraw();
		int onDelete();

		int creationHook();
		int drawHook();

		int original_onCreate();

		~daObjKinoko_c();
};


// This will replace Nintendo's onCreate
int daObjKinoko_c::creationHook() {
	original_onCreate();

	// if rotation is off, do nothing else
	if (!((settings >> 28) & 1))
		return 1;

	// OK, figure out the rotation
	u8 sourceRotation = (settings >> 24) & 0xF;

	// 0 is up. -0x4000 is right, 0x4000 is left ...
	s16 rotation;

	// We'll flip it later.
	// Thus: 0..7 rotates left (in increments of 0x800),
	// 8..15 rotates right (in increments of 0x800 too).
	// To specify facing up, well.. just use 0.

	if (sourceRotation < 8)
		rotation = (sourceRotation * 0x800) - 0x4000;
	else
		rotation = (sourceRotation * 0x800) - 0x3800;

	rotation = -rotation;

	rot.z = rotation;

	/* Original code: */
	int lengthInTiles = settings & 0xF;

	float sizeMult = (thickness == 1) ? 1.0f : 0.5f;
	float length = sizeMult * ((32.0f + (lengthInTiles * 16.0f)) - 16.0f);

	float topPos = (sizeMult * 16.0f);

	float cosThing = nw4r::math::CosFIdx((lengthInTiles * 16.0f) / 256.0f);
	float anotherThing = (4.0f + topPos) / cosThing;

	// Middle Collider
	colliders[0].init(this,
			/*xOffset=*/0.0f, /*yOffset=*/0.0f,
			/*topYOffset=*/topPos,
			/*rightSize=*/length, /*leftSize=*/-length,
			/*rotation=*/rotation, /*_45=*/1
			);

	colliders[0]._47 = 0;
	colliders[0].flags = 0x80180 | 0xC00;

	// Now get the info to move the colliders by ....
	float rotFIdx = ((float)rotation) / 256.0f;
	float sinRot, cosRot;
	nw4r::math::SinCosFIdx(&sinRot, &cosRot, rotFIdx);
	//OSReport("Rotation is %d, so rotFIdx is %f\n", rotation, rotFIdx);
	//OSReport("Sin: %f, Cos: %f\n", sinRot, cosRot);
	
	float leftXOffs = (cosRot * -length) - (sinRot * topPos);
	float leftYOffs = (sinRot * -length) + (cosRot * topPos);
	float rightXOffs = (cosRot * length) - (sinRot * topPos);
	float rightYOffs = (sinRot * length) + (cosRot * topPos);
	//OSReport("leftXOffs: %f, leftYOffs: %f\n", leftXOffs, leftYOffs);
	//OSReport("rightXOffs: %f, rightYOffs: %f\n", rightXOffs, rightYOffs);

	// Right Collider
	colliders[1].init(this,
			/*xOffset=*/rightXOffs, /*yOffset=*/rightYOffs,
			/*topYOffset=*/0.0f,
			/*rightSize=*/anotherThing, /*leftSize=*/0.0f,
			/*rotation=*/rotation - 0x2000, /*_45=*/1
			);

	colliders[1]._47 = 0;
	colliders[1].flags = 0x80100 | 0x800;

	// Left Collider
	colliders[2].init(this,
			/*xOffset=*/leftXOffs, /*yOffset=*/leftYOffs,
			/*topYOffset=*/0.0f,
			/*rightSize=*/0.0f, /*leftSize=*/-anotherThing,
			/*rotation=*/rotation + 0x2000, /*_45=*/1
			);

	colliders[2]._47 = 0;
	colliders[2].flags = 0x80080 | 0x400;

	return 1;
}


// This will replace Nintendo's onDraw
int daObjKinoko_c::drawHook() {
	matrix.translation(pos.x, pos.y, pos.z);
	matrix.applyRotationZ(&rot.z);
	matrix.applyRotationX(&xRotator.output);
	matrix.applyRotationZ(&zRotator.output);
	renderer.drawWithMatrix(0.0f, &matrix);

	return 1;
}



//
// processed\../src/tilegod.cpp
//

#include <game.h>
#include <sfx.h>

//#define REIMPLEMENT

extern "C" bool SpawnEffect(const char*, int, Vec*, S16Vec*, Vec*);

class daChengeBlock_c : public dStageActor_c {
	static daChengeBlock_c *build();

	u32 _394;
	u64 initialFlags;

	enum Action { Destroy, Create };
	enum Pattern { Fill, CheckerA, CheckerB };

	int width, height;
	Action action;
	int blockType;
	int isPermanent;
	Pattern pattern;
	u16 hasTriggered;

	u32 _3BC;


	int onCreate();
	int onExecute();

	void doStuff(Action action, bool wasCalledOnCreation);
	void tryToTrigger();
};

#ifdef REIMPLEMENT
daChengeBlock_c *daChengeBlock_c::build() {
	return new(AllocFromGameHeap1(sizeof(daChengeBlock_c))) daChengeBlock_c;
}



int daChengeBlock_c::onCreate() {
	hasTriggered = 0;

	height = settings & 0xF;
	width = (settings & 0xF0) >> 4;
	blockType = (settings & 0xF000) >> 12;
	pattern = (Pattern)((settings & 0x30000) >> 16);
	isPermanent = (settings & 0xF0000000) >> 28;

	if (width == 0)
		width++;
	if (height == 0)
		height++;

	action = (Action)((settings & 0xF00) >> 8);

	initialFlags = dFlagMgr_c::instance->flags & dStageActor_c::creatingFlagMask;

	if (initialFlags) {
		if (action == Destroy) {
			doStuff(Destroy, true);
		} else {
			doStuff(Create, true);
			hasTriggered = true;
		}

		if (isPermanent)
			return 2;
	}

	return 1;
}


int daChengeBlock_c::onExecute() {
	tryToTrigger();

	if (!hasTriggered)
		checkZoneBoundaries(0);

	return 1;
}


#endif // REIMPLEMENT

// Red, Brick, Blank/Unused, Stone, Wood, Blank
static const u16 Tiles[] = {124, 2, 12, 123, 15, 0};

void daChengeBlock_c::doStuff(Action action, bool wasCalledOnCreation) {
	u16 perTilePatternFlag = 1, perRowPatternFlag = 1;

	u16 worldX = ((u16)pos.x) & 0xFFF0;
	u16 baseWorldX = worldX;
	u16 worldY = ((u16)(-pos.y)) & 0xFFF0;

	if (pattern == CheckerB) {
		perTilePatternFlag = 0;
		perRowPatternFlag = 0;
	}

	u16 tile;
	if (action != Destroy) {
		if (blockType & 8) {
			// Specify a tile number
			tile = 0x8000 | ((blockType & 3) << 8) | ((settings & 0xFF00000) >> 20);
		} else {
			// fall through
			tile = Tiles[blockType];
		}
	} else {
		tile = 0;
	}

	for (u16 y = 0; y < height; y++) {
		for (u16 x = 0; x < width; x++) {
			if (perTilePatternFlag) {
				u16 *pExistingTile = dBgGm_c::instance->getPointerToTile(worldX, worldY, currentLayerID);
				u16 existingTile = pExistingTile ? *pExistingTile : 0;

				dBgGm_c::instance->placeTile(worldX, worldY, currentLayerID, tile);

				if (!wasCalledOnCreation) {
					Vec effectPos;

					if (action == Destroy) {
						if (blockType != 2) {
							effectPos.x = ((float)(worldX)) + 8.0f;
							effectPos.y = ((float)(-worldY)) - 8.0f;
							effectPos.z = pos.z;

							u16 shardType;
							switch (existingTile) {
								case 0x30: shardType = 0; break;
								case 0x31: shardType = 3; break;
								case 0x32: shardType = 4; break;
								case 0x33: shardType = 2; break;
								case 0x34: shardType = 1; break;
								case 0x37: shardType = 5; break;
								default: shardType = 0xFFFF;
							}

							if (!(settings & 0x40000)) {
								if (shardType == 0xFFFF) {
									SpawnEffect("Wm_en_burst_ss", 0, &effectPos, 0, 0);
								} else {
									u32 sets = (shardType << 8) | 3;
									effectPos.y -= 8;
									dEffectBreakMgr_c::instance->spawnTile(&effectPos, sets, 0);
								}
							}

							if (!(settings & 0x80000)) {
								Vec2 soundPos;
								ConvertStagePositionToScreenPosition(&soundPos, &effectPos);
								SoundPlayingClass::instance2->PlaySoundAtPosition(SE_OBJ_BLOCK_BREAK, &soundPos, 0);
							}
						}
					} else {
						effectPos.x = ((float)(worldX)) + 8.0f;
						effectPos.y = ((float)(-worldY)) - 8.0f;
						effectPos.z = pos.z;

						if (!(settings & 0x40000)) {
							if (blockType != 2) {
								SpawnEffect("Wm_en_burst_ss", 0, &effectPos, 0, 0);
							}
						}
					}
				}
			}

			if (pattern != Fill) {
				perTilePatternFlag ^= 1;
			}

			worldX += 16;
		}

		if (pattern != Fill) {
			perRowPatternFlag ^= 1;
			perTilePatternFlag = perRowPatternFlag;
		}

		worldX = baseWorldX;
		worldY += 16;
	}
}


#ifdef REIMPLEMENT
void daChengeBlock_c::tryToTrigger() {
	u64 result = spriteFlagMask & dFlagMgr_c::instance->flags;

	if (action == Destroy) {
		if (result & initialFlags) {
			if (result) {
				doStuff(Destroy, false);
				hasTriggered = true;
			} else {
				doStuff(Create, false);
				hasTriggered = false;
			}

			initialFlags = result;

			if (isPermanent)
				fBase_c::Delete();
		}
	} else {
		if (result & initialFlags) {
			if (result) {
				doStuff(Create, false);
				hasTriggered = true;
			} else {
				doStuff(Destroy, false);
				hasTriggered = false;
			}

			initialFlags = result;

			if (isPermanent)
				fBase_c::Delete();
		}
	}
}
#endif


//
// processed\../src/linegod.cpp
//

#include <common.h>
#include <game.h>

// TODO: make "No Deactivation"

struct BgActor {
	u16 def_id;		// 0x00
	u16 x;			// 0x02
	u16 y;			// 0x04
	u8 layer;		// 0x06
	u8 EXTRA_off;	// 0x07
	u32 actor_id;	// 0x08
};

struct BgActorDef {
	u32 tilenum;
	u16 actor;
	u8 _06[2];
	float x;
	float y;
	float z;
	float width;
	float height;
	u32 extra_var;
};

struct dBgActorManager_c {
	u32 vtable;		// 0x00
	u8 _04[0x34];	// 0x04
	BgActor *array;	// 0x38
	u32 count;		// 0x3C
	u32 type;		// 0x40
};

extern dBgActorManager_c *dBgActorManager;

extern BgActorDef *BgActorDefs;

struct BG_GM_hax {
	u8 _00[0x8FE64];
	float _0x8FE64;
	float _0x8FE68;
	float _0x8FE6C;
	float _0x8FE70;
};

extern BG_GM_hax *BG_GM_ptr;

// Regular class is 0x3D0.
// Let's add stuff to the end just to be safe.
// Size is now 0x400
// 80898798 38600400

#define LINEGOD_FUNC_ACTIVATE	0
#define LINEGOD_FUNC_DEACTIVATE	1

struct LineGod {
	u32 id;			// 0x00
	u32 settings;	// 0x04
	u16 name;		// 0x08
	u8 _0A[6];		// 0x0A
	u8 _10[0x9C];	// 0x10
	float x;		// 0xAC
	float y;		// 0xB0
	float z;		// 0xB4
	u8 _B8[0x318];	// 0xB8
	u64 eventFlag;	// 0x3D0
	u8 func;		// 0x3D4
	u8 width;		// 0x3D5
	u8 height;		// 0x3D6
	u8 lastEvState;	// 0x3D7
	BgActor *ac[8];	// 0x3D8
};


fBase_c *FindActorByID(u32 id);

u16 *GetPointerToTile(BG_GM_hax *self, u16 x, u16 y, u16 layer, short *blockID_p, bool unused);



void LineGod_BuildList(LineGod *self);
bool LineGod_AppendToList(LineGod *self, BgActor *ac);
void LineGod_Update(LineGod *self);


bool LineGod_Create(LineGod *self) {
	char eventNum	= (self->settings >> 24)	& 0xFF;
	self->eventFlag = (u64)1 << (eventNum - 1);
	
	
	
	self->func		= (self->settings)			& 1;
	self->width		= (self->settings >> 4)		& 15;
	self->height	= (self->settings >> 8)		& 15;
	
	self->lastEvState = 0xFF;
	
	LineGod_BuildList(self);
	LineGod_Update(self);
	
	return true;
}

bool LineGod_Execute(LineGod *self) {
	LineGod_Update(self);
	return true;
}

void LineGod_BuildList(LineGod *self) {
	for (int clearIdx = 0; clearIdx < 8; clearIdx++) {
		self->ac[clearIdx] = 0;
	}
	
	

	float gLeft = self->x - (BG_GM_ptr->_0x8FE64 - fmod(BG_GM_ptr->_0x8FE64, 16));
	float gTop = self->y - (BG_GM_ptr->_0x8FE6C - fmod(BG_GM_ptr->_0x8FE6C, 16));

	// 1 unit padding to avoid catching stuff that is not in our rectangle
	Vec grect1 = (Vec){
		gLeft + 1, gTop - (self->height * 16) + 1, 0
	};

	Vec grect2 = (Vec){
		gLeft + (self->width * 16) - 1, gTop - 1, 0
	};

	
	for (int i = 0; i < dBgActorManager->count; i++) {
		BgActor *ac = &dBgActorManager->array[i];

		// the Def width/heights are padded with 8 units on each side
		// except for one of the steep slopes, which differs for no reason

		BgActorDef *def = &BgActorDefs[ac->def_id];
		float aXCentre = (ac->x * 16) + def->x;
		float aYCentre = (-ac->y * 16) + def->y;

		float xDistToCentre = (def->width - 16) / 2;
		float yDistToCentre = (def->height - 16) / 2;

		Vec arect1 = (Vec){
			aXCentre - xDistToCentre, aYCentre - yDistToCentre, 0
		};
		
		Vec arect2 = (Vec){
			aXCentre + xDistToCentre, aYCentre + yDistToCentre, 0
		};

		if (RectanglesOverlap(&arect1, &arect2, &grect1, &grect2))
			LineGod_AppendToList(self, ac);
	}
}

bool LineGod_AppendToList(LineGod *self, BgActor *ac) {
	
	for (int search = 0; search < 8; search++) {
		if (self->ac[search] == 0) {
			self->ac[search] = ac;
			return true;
		}
	}
	
	return false;
}

void LineGod_Update(LineGod *self) {
	
	u8 newEvState = 0;
	if (dFlagMgr_c::instance->flags & self->eventFlag)
		newEvState = 1;
	
	if (newEvState == self->lastEvState)
		return;
	
	u16 x_bias = (BG_GM_ptr->_0x8FE64 / 16);
	u16 y_bias = -(BG_GM_ptr->_0x8FE6C / 16);
	
	
	u8 offState;
	if (self->func == LINEGOD_FUNC_ACTIVATE)
		offState = (newEvState == 1) ? 1 : 0;
	else
		offState = (newEvState == 1) ? 0 : 1;
	
	
	for (int i = 0; i < 8; i++) {
		if (self->ac[i] != 0) {
			BgActor *ac = self->ac[i];
			
			
			ac->EXTRA_off = offState;
			if (offState == 1 && ac->actor_id != 0) {
				fBase_c *assoc_ac = FindActorByID(ac->actor_id);
				if (assoc_ac != 0)
					assoc_ac->Delete();
				ac->actor_id = 0;
			}
			
			u16 *tile = GetPointerToTile(BG_GM_ptr, (ac->x + x_bias) * 16, (ac->y + y_bias) * 16, 0, 0, 0);
			if (offState == 1)
				*tile = 0;
			else
				*tile = BgActorDefs[ac->def_id].tilenum;
			
		}
	}
	
	
	
	self->lastEvState = newEvState;
}

//
// processed\../src/tilesetfixer.cpp
//

#include <common.h>
#include <game.h>

const char *GetTilesetName(void *cls, int areaNum, int slotNum);

void DoFixes(int areaNumber, int slotNumber);
void SwapObjData(u8 *data, int slotNumber);

extern "C" void *OriginalTilesetLoadingThing(void *, void *, int, int);

// Main hook
void *TilesetFixerHack(void *cls, void *heap, int areaNum, int layerNum) {
	if (layerNum == 0) {
		for (int i = 1; i < 4; i++) {
			DoFixes(areaNum, i);
		}
	}

	return OriginalTilesetLoadingThing(cls, heap, areaNum, layerNum);
}



// File format definitions
struct ObjLookupEntry {
	u16 offset;
	u8 width;
	u8 height;
};


void DoFixes(int areaNumber, int slotNumber) {
	// This is where it all starts
	const char *tsName = GetTilesetName(BGDatClass, areaNumber, slotNumber);

	if (tsName == 0 || tsName[0] == 0) {
		return;
	}


	char untHDname[64], untname[64];
	snprintf(untHDname, 64, "BG_unt/%s_hd.bin", tsName);
	snprintf(untname, 64, "BG_unt/%s.bin", tsName);

	u32 unt_hd_length;
	void *bg_unt_hd_data = DVD_GetFile(GetDVDClass2(), tsName, untHDname, &unt_hd_length);
	void *bg_unt = DVD_GetFile(GetDVDClass2(), tsName, untname);


	ObjLookupEntry *lookups = (ObjLookupEntry*)bg_unt_hd_data;

	int objCount = unt_hd_length / sizeof(ObjLookupEntry);

	for (int i = 0; i < objCount; i++) {
		// process each object
		u8 *thisObj = (u8*)((u32)bg_unt + lookups[i].offset);

		SwapObjData(thisObj, slotNumber);
	}
}


void SwapObjData(u8 *data, int slotNumber) {
	// rudimentary parser which will hopefully work

	while (*data != 0xFF) {
		u8 cmd = *data;

		if (cmd == 0xFE || (cmd & 0x80) != 0) {
			data++;
			continue;
		}

		if ((data[2] & 3) != 0) {
			data[2] &= 0xFC;
			data[2] |= slotNumber;
		}
		data += 3;
	}

}


//
// processed\../src/eventblock.cpp
//

#include <common.h>
#include <game.h>

// Patches MIST_INTERMITTENT (sprite 239)

class daEnEventBlock_c : public daEnBlockMain_c {
public:
	enum Mode {
		TOGGLE_EVENT = 0,
		SWAP_EVENTS = 1
	};

	TileRenderer tile;
	Physics::Info physicsInfo;

	u8 event1;
	u8 event2;
	Mode mode;

	void equaliseEvents();

	int onCreate();
	int onDelete();
	int onExecute();

	void calledWhenUpMoveExecutes();
	void calledWhenDownMoveExecutes();

	void blockWasHit(bool isDown);

	USING_STATES(daEnEventBlock_c);
	DECLARE_STATE(Wait);

	static daEnEventBlock_c *build();
};


CREATE_STATE(daEnEventBlock_c, Wait);


int daEnEventBlock_c::onCreate() {
	blockInit(pos.y);

	physicsInfo.x1 = -8;
	physicsInfo.y1 = 16;
	physicsInfo.x2 = 8;
	physicsInfo.y2 = 0;

	physicsInfo.otherCallback1 = &daEnBlockMain_c::OPhysicsCallback1;
	physicsInfo.otherCallback2 = &daEnBlockMain_c::OPhysicsCallback2;
	physicsInfo.otherCallback3 = &daEnBlockMain_c::OPhysicsCallback3;

	physics.setup(this, &physicsInfo, 3, currentLayerID);
	physics.flagsMaybe = 0x260;
	physics.callback1 = &daEnBlockMain_c::PhysicsCallback1;
	physics.callback2 = &daEnBlockMain_c::PhysicsCallback2;
	physics.callback3 = &daEnBlockMain_c::PhysicsCallback3;
	physics.addToList();

	TileRenderer::List *list = dBgGm_c::instance->getTileRendererList(0);
	list->add(&tile);

	tile.x = pos.x - 8;
	tile.y = -(16 + pos.y);
	tile.tileNumber = 0x97;

	mode = (Mode)((settings >> 16) & 0xF);
	event1 = ((settings >> 8) & 0xFF) - 1;
	event2 = (settings & 0xFF) - 1;

	equaliseEvents();

	doStateChange(&daEnEventBlock_c::StateID_Wait);

	return true;
}


int daEnEventBlock_c::onDelete() {
	TileRenderer::List *list = dBgGm_c::instance->getTileRendererList(0);
	list->remove(&tile);

	physics.removeFromList();

	return true;
}


int daEnEventBlock_c::onExecute() {
	acState.execute();
	physics.update();
	blockUpdate();

	tile.setPosition(pos.x-8, -(16+pos.y), pos.z);
	tile.setVars(scale.x);

	equaliseEvents();

	bool isActive = dFlagMgr_c::instance->active(event2);

	tile.tileNumber = (isActive ? 0x96 : 0x97);

	// now check zone bounds based on state
	if (acState.getCurrentState()->isEqual(&StateID_Wait)) {
		checkZoneBoundaries(0);
	}

	return true;
}


daEnEventBlock_c *daEnEventBlock_c::build() {

	void *buffer = AllocFromGameHeap1(sizeof(daEnEventBlock_c));
	daEnEventBlock_c *c = new(buffer) daEnEventBlock_c;


	return c;
}


void daEnEventBlock_c::equaliseEvents() {
	if (mode != SWAP_EVENTS)
		return;

	bool f1 = dFlagMgr_c::instance->active(event1);
	bool f2 = dFlagMgr_c::instance->active(event2);

	if (!f1 && !f2) {
		dFlagMgr_c::instance->set(event1, 0, true, false, false);
	}

	if (f1 && f2) {
		dFlagMgr_c::instance->set(event2, 0, false, false, false);
	}
}


void daEnEventBlock_c::blockWasHit(bool isDown) {
	pos.y = initialY;

	if (mode == TOGGLE_EVENT) {
		if (dFlagMgr_c::instance->active(event2))
			dFlagMgr_c::instance->set(event2, 0, false, false, false);
		else
			dFlagMgr_c::instance->set(event2, 0, true, false, false);

	} else if (mode == SWAP_EVENTS) {
		if (dFlagMgr_c::instance->active(event1)) {
			dFlagMgr_c::instance->set(event1, 0, false, false, false);
			dFlagMgr_c::instance->set(event2, 0, true, false, false);
		} else {
			dFlagMgr_c::instance->set(event1, 0, true, false, false);
			dFlagMgr_c::instance->set(event2, 0, false, false, false);
		}
	}

	physics.setup(this, &physicsInfo, 3, currentLayerID);
	physics.addToList();
	
	doStateChange(&StateID_Wait);
}



void daEnEventBlock_c::calledWhenUpMoveExecutes() {
	if (initialY >= pos.y)
		blockWasHit(false);
}

void daEnEventBlock_c::calledWhenDownMoveExecutes() {
	if (initialY <= pos.y)
		blockWasHit(true);
}



void daEnEventBlock_c::beginState_Wait() {
}

void daEnEventBlock_c::endState_Wait() {
}

void daEnEventBlock_c::executeState_Wait() {
	int result = blockResult();

	if (result == 0)
		return;

	if (result == 1) {
		doStateChange(&daEnBlockMain_c::StateID_UpMove);
		anotherFlag = 2;
		isGroundPound = false;
	} else {
		doStateChange(&daEnBlockMain_c::StateID_DownMove);
		anotherFlag = 1;
		isGroundPound = true;
	}
}



//
// processed\../src/msgbox.cpp
//

#include <common.h>
#include <game.h>
#include <sfx.h>
#include "msgbox.h"

// Replaces: EN_LIFT_ROTATION_HALF (Sprite 107; Profile ID 481 @ 80AF96F8)


dMsgBoxManager_c *dMsgBoxManager_c::instance = 0;
dMsgBoxManager_c *dMsgBoxManager_c::build() {
	void *buffer = AllocFromGameHeap1(sizeof(dMsgBoxManager_c));
	dMsgBoxManager_c *c = new(buffer) dMsgBoxManager_c;

	instance = c;
	return c;
}

#define ANIM_BOX_APPEAR 0
#define ANIM_BOX_DISAPPEAR 1

extern int MessageBoxIsShowing;

/*****************************************************************************/
// Events
int dMsgBoxManager_c::onCreate() {
	if (!layoutLoaded) {
		if (!layout.loadArc("msgbox.arc", false))
			return false;

		static const char *brlanNames[2] = {
			"BoxAppear.brlan",
			"BoxDisappear.brlan",
		};

		static const char *groupNames[2] = {
			"G_Box", "G_Box",
		};

		layout.build("MessageBox.brlyt");

		if (IsWideScreen()) {
			layout.layout.rootPane->scale.x = 0.7711f;
		}

		layout.loadAnimations(brlanNames, 2);
		layout.loadGroups(groupNames, (int[2]){0,1}, 2);
		layout.disableAllAnimations();

		layout.drawOrder = 140;

		layoutLoaded = true;
	}

	visible = false;

	return true;
}

int dMsgBoxManager_c::onExecute() {
	state.execute();

	layout.execAnimations();
	layout.update();

	return true;
}

int dMsgBoxManager_c::onDraw() {
	if (visible) {
		layout.scheduleForDrawing();
	}
	
	return true;
}

int dMsgBoxManager_c::onDelete() {
	instance = 0;

	MessageBoxIsShowing = false;
	if (canCancel && StageC4::instance)
		StageC4::instance->_1D = 0; // disable no-pause
	msgDataLoader.unload();

	return layout.free();
}

/*****************************************************************************/
// Load Resources
CREATE_STATE_E(dMsgBoxManager_c, LoadRes);

void dMsgBoxManager_c::executeState_LoadRes() {
	if (msgDataLoader.load("/NewerRes/Messages.bin")) {
		state.setState(&StateID_Wait);
	} else {
	}
}

/*****************************************************************************/
// Waiting
CREATE_STATE_E(dMsgBoxManager_c, Wait);

void dMsgBoxManager_c::executeState_Wait() {
	// null
}

/*****************************************************************************/
// Show Box
void dMsgBoxManager_c::showMessage(int id, bool canCancel, int delay) {
	if (!this) {
		OSReport("ADD A MESSAGE BOX MANAGER YOU MORON\n");
		return;
	}

	// get the data file
	header_s *data = (header_s*)msgDataLoader.buffer;

	const wchar_t *title = 0, *msg = 0;

	for (int i = 0; i < data->count; i++) {
		if (data->entry[i].id == id) {
			title = (const wchar_t*)((u32)data + data->entry[i].titleOffset);
			msg = (const wchar_t*)((u32)data + data->entry[i].msgOffset);
			break;
		}
	}

	if (title == 0) {
		OSReport("Message Box: Message %08x not found\n", id);
		return;
	}

	layout.findTextBoxByName("T_title")->SetString(title);
	layout.findTextBoxByName("T_msg")->SetString(msg);

	this->canCancel = canCancel;
	this->delay = delay;
	layout.findPictureByName("button")->SetVisible(canCancel);

	state.setState(&StateID_BoxAppearWait);
}


CREATE_STATE(dMsgBoxManager_c, BoxAppearWait);

void dMsgBoxManager_c::beginState_BoxAppearWait() {
	visible = true;
	MessageBoxIsShowing = true;
	StageC4::instance->_1D = 1; // enable no-pause
	layout.enableNonLoopAnim(ANIM_BOX_APPEAR);

	nw4r::snd::SoundHandle handle;
	PlaySoundWithFunctionB4(SoundRelatedClass, &handle, SE_SYS_KO_DIALOGUE_IN, 1);
}

void dMsgBoxManager_c::executeState_BoxAppearWait() {
	if (!layout.isAnimOn(ANIM_BOX_APPEAR)) {
		state.setState(&StateID_ShownWait);
	}
}

void dMsgBoxManager_c::endState_BoxAppearWait() { }

/*****************************************************************************/
// Wait For Player To Finish
CREATE_STATE(dMsgBoxManager_c, ShownWait);

void dMsgBoxManager_c::beginState_ShownWait() { }
void dMsgBoxManager_c::executeState_ShownWait() {
	if (canCancel) {
		int nowPressed = Remocon_GetPressed(GetActiveRemocon());

		if (nowPressed & WPAD_TWO)
			state.setState(&StateID_BoxDisappearWait);
	}

	if (delay > 0) {
		delay--;
		if (delay == 0)
			state.setState(&StateID_BoxDisappearWait);
	}
}
void dMsgBoxManager_c::endState_ShownWait() { }

/*****************************************************************************/
// Hide Box
CREATE_STATE(dMsgBoxManager_c, BoxDisappearWait);

void dMsgBoxManager_c::beginState_BoxDisappearWait() {
	layout.enableNonLoopAnim(ANIM_BOX_DISAPPEAR);

	nw4r::snd::SoundHandle handle;
	PlaySoundWithFunctionB4(SoundRelatedClass, &handle, SE_SYS_DIALOGUE_OUT_AUTO, 1);
}

void dMsgBoxManager_c::executeState_BoxDisappearWait() {
	if (!layout.isAnimOn(ANIM_BOX_DISAPPEAR)) {
		state.setState(&StateID_Wait);

		for (int i = 0; i < 2; i++)
			layout.resetAnim(i);
		layout.disableAllAnimations();
	}
}

void dMsgBoxManager_c::endState_BoxDisappearWait() {
	visible = false;
	MessageBoxIsShowing = false;
	if (canCancel && StageC4::instance)
		StageC4::instance->_1D = 0; // disable no-pause
}



/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
// Replaces: EN_BLUR (Sprite 152; Profile ID 603 @ 80ADD890)


class daEnMsgBlock_c : public daEnBlockMain_c {
public:
	TileRenderer tile;
	Physics::Info physicsInfo;

	int onCreate();
	int onDelete();
	int onExecute();

	void calledWhenUpMoveExecutes();
	void calledWhenDownMoveExecutes();

	void blockWasHit(bool isDown);

	USING_STATES(daEnMsgBlock_c);
	DECLARE_STATE(Wait);

	static daEnMsgBlock_c *build();
};


CREATE_STATE(daEnMsgBlock_c, Wait);


int daEnMsgBlock_c::onCreate() {
	blockInit(pos.y);

	physicsInfo.x1 = -8;
	physicsInfo.y1 = 16;
	physicsInfo.x2 = 8;
	physicsInfo.y2 = 0;

	physicsInfo.otherCallback1 = &daEnBlockMain_c::OPhysicsCallback1;
	physicsInfo.otherCallback2 = &daEnBlockMain_c::OPhysicsCallback2;
	physicsInfo.otherCallback3 = &daEnBlockMain_c::OPhysicsCallback3;

	physics.setup(this, &physicsInfo, 3, currentLayerID);
	physics.flagsMaybe = 0x260;
	physics.callback1 = &daEnBlockMain_c::PhysicsCallback1;
	physics.callback2 = &daEnBlockMain_c::PhysicsCallback2;
	physics.callback3 = &daEnBlockMain_c::PhysicsCallback3;
	physics.addToList();

	TileRenderer::List *list = dBgGm_c::instance->getTileRendererList(0);
	list->add(&tile);

	tile.x = pos.x - 8;
	tile.y = -(16 + pos.y);
	tile.tileNumber = 0x98;

	doStateChange(&daEnMsgBlock_c::StateID_Wait);

	return true;
}


int daEnMsgBlock_c::onDelete() {
	TileRenderer::List *list = dBgGm_c::instance->getTileRendererList(0);
	list->remove(&tile);

	physics.removeFromList();

	return true;
}


int daEnMsgBlock_c::onExecute() {
	acState.execute();
	physics.update();
	blockUpdate();

	tile.setPosition(pos.x-8, -(16+pos.y), pos.z);
	tile.setVars(scale.x);

	// now check zone bounds based on state
	if (acState.getCurrentState()->isEqual(&StateID_Wait)) {
		checkZoneBoundaries(0);
	}

	return true;
}


daEnMsgBlock_c *daEnMsgBlock_c::build() {
	void *buffer = AllocFromGameHeap1(sizeof(daEnMsgBlock_c));
	return new(buffer) daEnMsgBlock_c;
}


void daEnMsgBlock_c::blockWasHit(bool isDown) {
	pos.y = initialY;

	if (dMsgBoxManager_c::instance)
		dMsgBoxManager_c::instance->showMessage(settings);
	else
		Delete(false);

	physics.setup(this, &physicsInfo, 3, currentLayerID);
	physics.addToList();
	
	doStateChange(&StateID_Wait);
}



void daEnMsgBlock_c::calledWhenUpMoveExecutes() {
	if (initialY >= pos.y)
		blockWasHit(false);
}

void daEnMsgBlock_c::calledWhenDownMoveExecutes() {
	if (initialY <= pos.y)
		blockWasHit(true);
}



void daEnMsgBlock_c::beginState_Wait() {
}

void daEnMsgBlock_c::endState_Wait() {
}

void daEnMsgBlock_c::executeState_Wait() {
	int result = blockResult();

	if (result == 0)
		return;

	if (result == 1) {
		doStateChange(&daEnBlockMain_c::StateID_UpMove);
		anotherFlag = 2;
		isGroundPound = false;
	} else {
		doStateChange(&daEnBlockMain_c::StateID_DownMove);
		anotherFlag = 1;
		isGroundPound = true;
	}
}



//
// processed\../src/eventlooper.cpp
//

#include <common.h>
#include <game.h>

struct EventLooper {
	u32 id;			// 0x00
	u32 settings;	// 0x04
	u16 name;		// 0x08
	u8 _0A[6];		// 0x0A
	u8 _10[0x9C];	// 0x10
	float x;		// 0xAC
	float y;		// 0xB0
	float z;		// 0xB4
	u8 _B8[0x318];	// 0xB8
	// Any variables you add to the class go here; starting at offset 0x3D0
	u64 eventFlag;	// 0x3D0
	u64 eventActive;	// 0x3D0
	u8 delay;		// 0x3D4
	u8 delayCount;	// 0x3D7
};

void EventLooper_Update(EventLooper *self);



bool EventLooper_Create(EventLooper *self) {
	char eventStart	= (self->settings >> 24)	& 0xFF;
	char eventEnd	= (self->settings >> 16)	& 0xFF;

	// Putting all the events into the flag
	int i;
	u64 q = (u64)0;
	for(i=eventStart;i<(eventEnd+1);i++)
	{
		q = q | ((u64)1 << (i - 1));
	}
		
	self->eventFlag = q;
	
	self->delay		= (((self->settings) & 0xFF) + 1) * 10;
	self->delayCount = 0;
	
	char tmpEvent= (self->settings >> 8)	& 0xFF;
	if (tmpEvent == 0)
	{
		self->eventActive = (u64)0xFFFFFFFFFFFFFFFF;
	}
	else
	{
		self->eventActive = (u64)1 << (tmpEvent - 1);
		
	}
	

	if (dFlagMgr_c::instance->flags & self->eventActive)
	{
		u64 evState = (u64)1 << (eventStart - 1);
		dFlagMgr_c::instance->flags |= evState;
	}

	EventLooper_Update(self);
	
	return true;
}

bool EventLooper_Execute(EventLooper *self) {
	EventLooper_Update(self);
	return true;
}


void EventLooper_Update(EventLooper *self) {
	
	if ((dFlagMgr_c::instance->flags & self->eventActive) == 0)
		return;

	// Waiting for the right moment
	if (self->delayCount < self->delay) 
	{

		self->delayCount = self->delayCount + 1;
		return;
	}	
	
	// Reset the delay
	self->delayCount = 0;
	
	// Find which event(s) is/are on
	u64 evState = dFlagMgr_c::instance->flags & self->eventFlag;
	
	// Turn off the old events
	dFlagMgr_c::instance->flags = dFlagMgr_c::instance->flags & (~self->eventFlag);
	
	// Shift them right if they can, if not, reset!
	evState = evState << 1;
	if (evState < self->eventFlag)
	{
		dFlagMgr_c::instance->flags = dFlagMgr_c::instance->flags | evState;
	}
	
	else
	{
		char eventStart	= (self->settings >> 24)	& 0xFF;
		evState = (u64)1 << (eventStart - 1);
		dFlagMgr_c::instance->flags = dFlagMgr_c::instance->flags | evState;
	}
	
	
}

//
// processed\../src/spritespawner.cpp
//

#include <game.h>

class dSpriteSpawner_c : public dStageActor_c {
	public:
		static dSpriteSpawner_c *build();

		u64 classicEventOverride;
		Actors profileID;
		bool respawn;
		u32 childSettings;
		u32 childID;

		int onCreate();
		int onExecute();
};

/*****************************************************************************/
// Glue Code
dSpriteSpawner_c *dSpriteSpawner_c::build() {
	void *buffer = AllocFromGameHeap1(sizeof(dSpriteSpawner_c));
	dSpriteSpawner_c *c = new(buffer) dSpriteSpawner_c;
	return c;
}


int dSpriteSpawner_c::onCreate() {
	char classicEventNum = (settings >> 28) & 0xF;
	classicEventOverride = (classicEventNum == 0) ? 0 : ((u64)1 << (classicEventNum - 1));

	profileID = (Actors)((settings >> 16) & 0x7FF);
	respawn = (settings >> 27) & 1;

	u16 tempSet = settings & 0xFFFF;
	childSettings =
		(tempSet & 3) | ((tempSet & 0xC) << 2) |
		((tempSet & 0x30) << 4) | ((tempSet & 0xC0) << 6) |
		((tempSet & 0x300) << 8) | ((tempSet & 0xC00) << 10) |
		((tempSet & 0x3000) << 12) | ((tempSet & 0xC000) << 14);

	return true;
}


int dSpriteSpawner_c::onExecute() {
	u64 effectiveFlag = classicEventOverride | spriteFlagMask;

	if (dFlagMgr_c::instance->flags & effectiveFlag) {
		if (!childID) {
			dStageActor_c *newAc = dStageActor_c::create(profileID, childSettings, &pos, 0, 0);
			childID = newAc->id;
		}
	} else {
		if (respawn)
			return true;

		if (childID) {
			dStageActor_c *ac = (dStageActor_c*)fBase_c::search(childID);
			if (ac) {
				pos = ac->pos;
				ac->Delete(1);
			}
			childID = 0;
		}
	}

	if (respawn) {
		if (childID) {
			dStageActor_c *ac = (dStageActor_c*)fBase_c::search(childID);

			if (!ac) {
				dStageActor_c *newAc = dStageActor_c::create(profileID, childSettings, &pos, 0, 0);
				childID = newAc->id;
			}
		}
	}

	return true;

}


//
// processed\../src/spriteswapper.cpp
//

#include <common.h>
#include <game.h>


class SpriteSpawnerTimed : public dStageActor_c {
public:
	int onCreate();
	int onExecute();

	static SpriteSpawnerTimed *build();

	u64 eventFlag;	// 0x3D0
	u16 type;		// 0x3D4
	u32 inheritSet;	// 0x3D6
	u8 lastEvState;	// 0x3DA
	u32 timer;
};


SpriteSpawnerTimed *SpriteSpawnerTimed::build() {
	void *buffer = AllocFromGameHeap1(sizeof(SpriteSpawnerTimed));
	return new(buffer) SpriteSpawnerTimed;
}



int SpriteSpawnerTimed::onCreate() {

	char eventNum	= (this->settings >> 28)	& 0xF;

	this->eventFlag = (u64)1 << (eventNum - 1);
	this->type		= (this->settings >> 16) & 0xFFF;
	
	short tempSet = this->settings & 0xFFFF;
	this->inheritSet = (tempSet & 3) | ((tempSet & 0xC) << 2) | ((tempSet & 0x30) << 4) | ((tempSet & 0xC0) << 6) | ((tempSet & 0x300) << 8) | ((tempSet & 0xC00) << 10) | ((tempSet & 0x3000) << 12) | ((tempSet & 0xC000) << 14);
	
	this->timer = 0;
	
	return true;
}

int SpriteSpawnerTimed::onExecute() {

	if (dFlagMgr_c::instance->flags & this->eventFlag) {		 // If the event is on
		if (this->timer < 1) {						// If the timer is empty
			CreateActor(this->type, this->inheritSet, this->pos, 0, 0);
			this->timer = 120;
		}		

		this->timer--;
	}

	else {
		this->timer = 0;
	}

	return true;
}

//
// processed\../src/topman.cpp
//

#include <common.h>
#include <game.h>
#include <g3dhax.h>
#include <sfx.h>

const char* TMarcNameList [] = {
	"topman",
	NULL	
};

class daTopman : public dEn_c {
	int onCreate();
	int onDelete();
	int onExecute();
	int onDraw();

	mHeapAllocator_c allocator;
	nw4r::g3d::ResFile resFile;

	m3d::mdl_c bodyModel;

	m3d::anmChr_c chrAnimation;

	int timer;
	char damage;
	char isDown;
	float XSpeed;
	u32 cmgr_returnValue;
	bool isBouncing;
	char isInSpace;
	char fromBehind;
	char isWaiting;
	char backFire;
	int directionStore;

	static daTopman *build();

	void bindAnimChr_and_setUpdateRate(const char* name, int unk, float unk2, float rate);
	void updateModelMatrices();
	bool calculateTileCollisions();

	void playerCollision(ActivePhysics *apThis, ActivePhysics *apOther);
	void yoshiCollision(ActivePhysics *apThis, ActivePhysics *apOther);

	bool collisionCat3_StarPower(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat14_YoshiFire(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCatD_Drill(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat7_GroundPound(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat7_GroundPoundYoshi(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat9_RollingObject(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat1_Fireball_E_Explosion(ActivePhysics *apThis, ActivePhysics *apOther);
	// bool collisionCat2_IceBall_15_YoshiIce(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat13_Hammer(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCatA_PenguinMario(ActivePhysics *apThis, ActivePhysics *apOther);

	void _vf148();
	void _vf14C();
	bool CreateIceActors();
	void addScoreWhenHit(void *other);

	USING_STATES(daTopman);
	DECLARE_STATE(Walk);
	DECLARE_STATE(Turn);
	DECLARE_STATE(Wait);
	DECLARE_STATE(KnockBack);
	DECLARE_STATE(Die);
};

daTopman *daTopman::build() {
	void *buffer = AllocFromGameHeap1(sizeof(daTopman));
	return new(buffer) daTopman;
}

///////////////////////
// Externs and States
///////////////////////
	extern "C" void *EN_LandbarrelPlayerCollision(dEn_c* t, ActivePhysics *apThis, ActivePhysics *apOther);

	//FIXME make this dEn_c->used...
	extern "C" char usedForDeterminingStatePress_or_playerCollision(dEn_c* t, ActivePhysics *apThis, ActivePhysics *apOther, int unk1);
	extern "C" int SmoothRotation(short* rot, u16 amt, int unk2);


	CREATE_STATE(daTopman, Walk);
	CREATE_STATE(daTopman, Turn);
	CREATE_STATE(daTopman, Wait);
	CREATE_STATE(daTopman, KnockBack);
	CREATE_STATE(daTopman, Die);

	// 	begoman_attack2"	// wobble back and forth tilted forwards
	// 	begoman_attack3"	// Leaned forward, antennae extended
	// 	begoman_damage"		// Bounces back slightly
	// 	begoman_damage2"	// Stops spinning and wobbles to the ground like a top
	// 	begoman_stand"		// Stands still, waiting
	// 	begoman_wait"		// Dizzily Wobbles
	// 	begoman_wait2"		// spins around just slightly
	// 	begoman_attack"		// Rocks backwards, and then attacks to an upright position, pulsing out his antennae


////////////////////////
// Collision Functions
////////////////////////

	// Collision callback to help shy guy not die at inappropriate times and ruin the dinner

	void daTopman::playerCollision(ActivePhysics *apThis, ActivePhysics *apOther) {

		char hitType;
		hitType = usedForDeterminingStatePress_or_playerCollision(this, apThis, apOther, 0);

		if(hitType == 1 || hitType == 2) {	// regular jump or mini jump
			this->_vf220(apOther->owner);
		} 
		else if(hitType == 3) {	// spinning jump or whatever?
			this->_vf220(apOther->owner);
		} 
		else if(hitType == 0) {
			EN_LandbarrelPlayerCollision(this, apThis, apOther);
			if (this->pos.x > apOther->owner->pos.x) {
				this->backFire = 1;
			}
			else {
				this->backFire = 0;
			}
			doStateChange(&StateID_KnockBack);
		} 

		// fix multiple player collisions via megazig
		deathInfo.isDead = 0;
		this->flags_4FC |= (1<<(31-7));
		this->counter_504[apOther->owner->which_player] = 0;
	}

	void daTopman::yoshiCollision(ActivePhysics *apThis, ActivePhysics *apOther) {
		this->playerCollision(apThis, apOther);
	}

	bool daTopman::collisionCatD_Drill(ActivePhysics *apThis, ActivePhysics *apOther) {
		this->dEn_c::playerCollision(apThis, apOther);
		this->_vf220(apOther->owner);

		deathInfo.isDead = 0;
		this->flags_4FC |= (1<<(31-7));
		this->counter_504[apOther->owner->which_player] = 0;
		return true;
	}

	bool daTopman::collisionCat7_GroundPound(ActivePhysics *apThis, ActivePhysics *apOther) {
		this->collisionCatD_Drill(apThis, apOther);
		return true;
	}

	bool daTopman::collisionCat7_GroundPoundYoshi(ActivePhysics *apThis, ActivePhysics *apOther) {
		this->collisionCatD_Drill(apThis, apOther);
		return true;
	}

	bool daTopman::collisionCat9_RollingObject(ActivePhysics *apThis, ActivePhysics *apOther) {
		backFire = apOther->owner->direction ^ 1;
		// doStateChange(&StateID_KnockBack);
		doStateChange(&StateID_Die);
		return true;
	}

	bool daTopman::collisionCat3_StarPower(ActivePhysics *apThis, ActivePhysics *apOther){
		doStateChange(&StateID_Die);
		return true;
	}

	bool daTopman::collisionCat13_Hammer(ActivePhysics *apThis, ActivePhysics *apOther) {
		doStateChange(&StateID_Die);
		return true;
	}

	bool daTopman::collisionCatA_PenguinMario(ActivePhysics *apThis, ActivePhysics *apOther){
		backFire = apOther->owner->direction ^ 1;
		doStateChange(&StateID_KnockBack);
		return true;
	}

	bool daTopman::collisionCat14_YoshiFire(ActivePhysics *apThis, ActivePhysics *apOther){
		backFire = apOther->owner->direction ^ 1;
		doStateChange(&StateID_KnockBack);
		return true;
	}

	bool daTopman::collisionCat1_Fireball_E_Explosion(ActivePhysics *apThis, ActivePhysics *apOther) {
		backFire = apOther->owner->direction ^ 1;
		doStateChange(&StateID_KnockBack);
		return true;
	}

	// void daTopman::collisionCat2_IceBall_15_YoshiIce(ActivePhysics *apThis, ActivePhysics *apOther) {
	// 	doStateChange(&StateID_DieFall);
	// }

	// These handle the ice crap
	void daTopman::_vf148() {
		dEn_c::_vf148();
		doStateChange(&StateID_Die);
	}
	void daTopman::_vf14C() {
		dEn_c::_vf14C();
		doStateChange(&StateID_Die);
	}

	DoSomethingCool my_struct;

	extern "C" void sub_80024C20(void);
	extern "C" void __destroy_arr(void*, void(*)(void), int, int);
	//extern "C" __destroy_arr(struct DoSomethingCool, void(*)(void), int cnt, int bar);

	bool daTopman::CreateIceActors()
	{
	    struct DoSomethingCool my_struct = { 0, this->pos, {2.5, 2.5, 2.5}, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
	    this->frzMgr.Create_ICEACTORs( (void*)&my_struct, 1 );
	    __destroy_arr( (void*)&my_struct, sub_80024C20, 0x3C, 1 );
	    return true;
	}

	void daTopman::addScoreWhenHit(void *other) {}


bool daTopman::calculateTileCollisions() {
	// Returns true if sprite should turn, false if not.

	HandleXSpeed();
	HandleYSpeed();
	doSpriteMovement();

	cmgr_returnValue = collMgr.isOnTopOfTile();
	collMgr.calculateBelowCollisionWithSmokeEffect();

	if (isBouncing) {
		stuffRelatingToCollisions(0.1875f, 1.0f, 0.5f);
		if (speed.y != 0.0f)
			isBouncing = false;
	}

	float xDelta = pos.x - last_pos.x;
	if (xDelta >= 0.0f)
		direction = 0;
	else
		direction = 1;

	if (collMgr.isOnTopOfTile()) {
		// Walking into a tile branch

		if (cmgr_returnValue == 0)
			isBouncing = true;

		if (speed.x != 0.0f) {
			//playWmEnIronEffect();
		}

		speed.y = 0.0f;

		// u32 blah = collMgr.s_80070760();
		// u8 one = (blah & 0xFF);
		// static const float incs[5] = {0.00390625f, 0.0078125f, 0.015625f, 0.0234375f, 0.03125f};
		// x_speed_inc = incs[one];
		max_speed.x = (direction == 1) ? -0.8f : 0.8f;
	} else {
		x_speed_inc = 0.0f;
	}

	// Bouncing checks
	if (_34A & 4) {
		Vec v = (Vec){0.0f, 1.0f, 0.0f};
		collMgr.pSpeed = &v;

		if (collMgr.calculateAboveCollision(collMgr.outputMaybe))
			speed.y = 0.0f;

		collMgr.pSpeed = &speed;

	} else {
		if (collMgr.calculateAboveCollision(collMgr.outputMaybe))
			speed.y = 0.0f;
	}

	collMgr.calculateAdjacentCollision(0);

	// Switch Direction
	if (collMgr.outputMaybe & (0x15 << direction)) {
		if (collMgr.isOnTopOfTile()) {
			isBouncing = true;
		}
		return true;
	}
	return false;
}

void daTopman::bindAnimChr_and_setUpdateRate(const char* name, int unk, float unk2, float rate) {
	nw4r::g3d::ResAnmChr anmChr = this->resFile.GetResAnmChr(name);
	this->chrAnimation.bind(&this->bodyModel, anmChr, unk);
	this->bodyModel.bindAnim(&this->chrAnimation, unk2);
	this->chrAnimation.setUpdateRate(rate);
}

int daTopman::onCreate() {

	this->deleteForever = true;
	
	// Model creation	
	allocator.link(-1, GameHeaps[0], 0, 0x20);

	this->resFile.data = getResource("topman", "g3d/begoman_spike.brres");
	nw4r::g3d::ResMdl mdl = this->resFile.GetResMdl("begoman");
	bodyModel.setup(mdl, &allocator, 0x224, 1, 0);
	SetupTextures_Map(&bodyModel, 0);


	// Animations start here
	nw4r::g3d::ResAnmChr anmChr = this->resFile.GetResAnmChr("begoman_wait");
	this->chrAnimation.setup(mdl, anmChr, &this->allocator, 0);

	allocator.unlink();

	// Stuff I do understand
	this->scale = (Vec){0.2, 0.2, 0.2};

	// this->pos.y = this->pos.y + 30.0; // X is vertical axis
	this->rot.x = 0; // X is vertical axis
	this->rot.y = 0xD800; // Y is horizontal axis
	this->rot.z = 0; // Z is ... an axis >.>
	this->direction = 1; // Heading left.
	
	this->speed.x = 0.0;
	this->speed.y = 0.0;
	this->max_speed.x = 0.8;
	this->x_speed_inc = 0.0;
	this->XSpeed = 0.8;

	this->isInSpace = this->settings & 0xF;
	this->isWaiting = (this->settings >> 4) & 0xF;
	this->fromBehind = 0;

	ActivePhysics::Info HitMeBaby;

	HitMeBaby.xDistToCenter = 0.0;
	HitMeBaby.yDistToCenter = 12.0;

	HitMeBaby.xDistToEdge = 14.0;
	HitMeBaby.yDistToEdge = 12.0;		

	HitMeBaby.category1 = 0x3;
	HitMeBaby.category2 = 0x0;
	HitMeBaby.bitfield1 = 0x4F;
	HitMeBaby.bitfield2 = 0xffbafffe;
	HitMeBaby.unkShort1C = 0;
	HitMeBaby.callback = &dEn_c::collisionCallback;

	this->aPhysics.initWithStruct(this, &HitMeBaby);
	this->aPhysics.addToList();


	// Tile collider

	// These fucking rects do something for the tile rect
	spriteSomeRectX = 28.0f;
	spriteSomeRectY = 32.0f;
	_320 = 0.0f;
	_324 = 16.0f;

	static const lineSensor_s below(12<<12, 4<<12, 0<<12);
	static const pointSensor_s above(0<<12, 12<<12);
	static const lineSensor_s adjacent(6<<12, 9<<12, 14<<12);

	collMgr.init(this, &below, &above, &adjacent);
	collMgr.calculateBelowCollisionWithSmokeEffect();

	cmgr_returnValue = collMgr.isOnTopOfTile();

	if (collMgr.isOnTopOfTile())
		isBouncing = false;
	else
		isBouncing = true;


	// State Changers
	bindAnimChr_and_setUpdateRate("begoman_wait2", 1, 0.0, 1.0); 
	if (this->isWaiting == 0) {
		doStateChange(&StateID_Walk); }
	else {
		doStateChange(&StateID_Wait); }

	this->onExecute();
	return true;
}

int daTopman::onDelete() {
	return true;
}

int daTopman::onExecute() {
	acState.execute();
	updateModelMatrices();
	
	float rect[] = {0.0, 0.0, 38.0, 38.0};
	int ret = this->outOfZone(this->pos, (float*)&rect, this->currentZoneID);
	if(ret) {
		this->Delete(1);
	}
	return true;
}

int daTopman::onDraw() {
	bodyModel.scheduleForDrawing();

	return true;
}

void daTopman::updateModelMatrices() {
	matrix.translation(pos.x, pos.y - 2.0, pos.z);
	matrix.applyRotationYXZ(&rot.x, &rot.y, &rot.z);

	bodyModel.setDrawMatrix(matrix);
	bodyModel.setScale(&scale);
	bodyModel.calcWorld(false);
}


///////////////
// Walk State
///////////////
	void daTopman::beginState_Walk() {
		this->max_speed.x = (this->direction) ? -this->XSpeed : this->XSpeed;
		this->speed.x = (direction) ? -0.8f : 0.8f;

		this->max_speed.y = (this->isInSpace) ? -2.0 : -4.0;
		this->speed.y = 	(this->isInSpace) ? -2.0 : -4.0;
		this->y_speed_inc = (this->isInSpace) ? -0.09375 : -0.1875;
	}
	void daTopman::executeState_Walk() { 

		if (!this->isOutOfView()) {
			nw4r::snd::SoundHandle *handle = PlaySound(this, SE_BOSS_JR_CROWN_JR_RIDE);
			if (handle)
				handle->SetVolume(0.5f, 0); 
		}
	
		bool ret = calculateTileCollisions();
		if (ret) {
			doStateChange(&StateID_Turn);
		}
		bodyModel._vf1C();

		if(this->chrAnimation.isAnimationDone()) {
			this->chrAnimation.setCurrentFrame(0.0);
		}
	}
	void daTopman::endState_Walk() { this->timer += 1; }


///////////////
// Turn State
///////////////
	void daTopman::beginState_Turn() {
		this->direction ^= 1;
		this->speed.x = 0.0;
	}
	void daTopman::executeState_Turn() { 

		bodyModel._vf1C();
		if(this->chrAnimation.isAnimationDone()) {
			this->chrAnimation.setCurrentFrame(0.0);
		}

		u16 amt = (this->direction == 0) ? 0x2800 : 0xD800;
		int done = SmoothRotation(&this->rot.y, amt, 0x800);

		if(done) {
			this->doStateChange(&StateID_Walk);
		}
	}
	void daTopman::endState_Turn() { }


///////////////
// Wait State
///////////////
	void daTopman::beginState_Wait() {
		this->max_speed.x = 0;
		this->speed.x = 0;

		this->max_speed.y = (this->isInSpace) ? -2.0 : -4.0;
		this->speed.y = 	(this->isInSpace) ? -2.0 : -4.0;
		this->y_speed_inc = (this->isInSpace) ? -0.09375 : -0.1875;
	}
	void daTopman::executeState_Wait() { 

		if (!this->isOutOfView()) {
			nw4r::snd::SoundHandle *handle = PlaySound(this, SE_BOSS_JR_CROWN_JR_RIDE);
			if (handle)
				handle->SetVolume(0.5f, 0); 
		}
	
		bool ret = calculateTileCollisions();
		if (ret) {
			doStateChange(&StateID_Turn);
		}

		bodyModel._vf1C();
		if(this->chrAnimation.isAnimationDone()) {
			this->chrAnimation.setCurrentFrame(0.0);
		}
	}
	void daTopman::endState_Wait() { }


///////////////
// Die State
///////////////
	void daTopman::beginState_Die() {
		dEn_c::dieFall_Begin();

		bindAnimChr_and_setUpdateRate("begoman_damage2", 1, 0.0, 1.0); 
		this->timer = 0;
	}
	void daTopman::executeState_Die() { 

		bodyModel._vf1C();

		PlaySound(this, SE_EMY_MECHAKOOPA_DAMAGE);
		if(this->chrAnimation.isAnimationDone()) {
			this->kill();
			this->Delete(this->deleteForever);
		}
	}
	void daTopman::endState_Die() { }


///////////////
// Knockback State
///////////////
	void daTopman::beginState_KnockBack() {
		bindAnimChr_and_setUpdateRate("begoman_damage", 1, 0.0, 0.75); 

		directionStore = direction;
		speed.x = (backFire) ? XSpeed*5.0f : XSpeed*-5.0f;
		max_speed.x = speed.x;
	}
	void daTopman::executeState_KnockBack() { 

		bool ret = calculateTileCollisions();
		this->speed.x = this->speed.x / 1.1;

		bodyModel._vf1C();
		if(this->chrAnimation.isAnimationDone()) {
			if (this->isWaiting == 0) {
				OSReport("Done being knocked back, going back to Walk state\n");
				doStateChange(&StateID_Walk); }
			else {
				OSReport("Done being knocked back, going back to Wait state\n");
				doStateChange(&StateID_Wait); }
		}

	}
	void daTopman::endState_KnockBack() { 
		direction = directionStore;
		bindAnimChr_and_setUpdateRate("begoman_wait2", 1, 0.0, 1.0); 
	}
	

//
// processed\../src/bossMegaGoomba.cpp
//

#include <common.h>
#include <game.h>
#include <g3dhax.h>
#include <sfx.h>
#include "boss.h"

extern "C" void *StageScreen;

const char* MGarcNameList [] = {
	"kuriboBig",
	"kuriboBoss",
	NULL	
};

class daMegaGoomba_c : public dEn_c {
	public:
	int onCreate();
	int onDelete();
	int onExecute();
	int onDraw();

	mHeapAllocator_c allocator;
	nw4r::g3d::ResFile resFile;
	m3d::mdl_c bodyModel;
	m3d::anmChr_c animationChr;

	float timer;
	float dying;

	lineSensor_s belowSensor;
	lineSensor_s adjacentSensor;

	ActivePhysics leftTrapAPhysics, rightTrapAPhysics;
	ActivePhysics stalkAPhysics;

	HermiteKey keysX[0x10];
	unsigned int Xkey_count;
	HermiteKey keysY[0x10];
	unsigned int Ykey_count;

	char life;
	bool already_hit;

	float XSpeed;
	float JumpHeight;
	float JumpDist;
	float JumpTime;

	char isBigBoss;
	char isPanic;

	bool takeHit(char count);

	void bindAnimChr_and_setUpdateRate(const char* name, int unk, float unk2, float rate);
	
	void dieFall_Begin();
	void dieFall_Execute();
	static daMegaGoomba_c *build();

	void setupBodyModel();
	void setupCollision();

	void updateModelMatrices();

	void stunPlayers();
	void unstunPlayers();

	bool hackOfTheCentury;

	bool playerStunned[4];

	void removeMyActivePhysics();
	void addMyActivePhysics();

	int tryHandleJumpedOn(ActivePhysics *apThis, ActivePhysics *apOther);

	void spriteCollision(ActivePhysics *apThis, ActivePhysics *apOther);
	void playerCollision(ActivePhysics *apThis, ActivePhysics *apOther);

	bool collisionCat1_Fireball_E_Explosion(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat2_IceBall_15_YoshiIce(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat3_StarPower(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat7_GroundPound(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat7_GroundPoundYoshi(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat9_RollingObject(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCatA_PenguinMario(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCatD_Drill(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat11_PipeCannon(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat13_Hammer(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat14_YoshiFire(ActivePhysics *apThis, ActivePhysics *apOther);
	void addScoreWhenHit(void *other);
	bool _vf120(ActivePhysics *apThis, ActivePhysics *apOther);
	bool _vf110(ActivePhysics *apThis, ActivePhysics *apOther);
	bool _vf108(ActivePhysics *apThis, ActivePhysics *apOther);

	void powBlockActivated(bool isNotMPGP);

	void dieOther_Begin();
	void dieOther_Execute();
	void dieOther_End();

	USING_STATES(daMegaGoomba_c);
	DECLARE_STATE(Shrink);
	DECLARE_STATE(Walk);
	DECLARE_STATE(Turn);
};


daMegaGoomba_c *daMegaGoomba_c::build() {
	void *buffer = AllocFromGameHeap1(sizeof(daMegaGoomba_c));
	return new(buffer) daMegaGoomba_c;
}


void daMegaGoomba_c::removeMyActivePhysics() {
	aPhysics.removeFromList();
	stalkAPhysics.removeFromList();
	leftTrapAPhysics.removeFromList();
	rightTrapAPhysics.removeFromList();
}

void daMegaGoomba_c::addMyActivePhysics() {
	aPhysics.addToList();
	stalkAPhysics.addToList();
	leftTrapAPhysics.addToList();
	rightTrapAPhysics.addToList();
}


void setNewActivePhysicsRect(daMegaGoomba_c *actor, Vec *scale) {
	float amtX = scale->x * 0.5f;
	float amtY = scale->y * 0.5f;

	actor->belowSensor.flags = SENSOR_LINE;
	actor->belowSensor.lineA = s32((amtX * -28.0f) * 4096.0f);
	actor->belowSensor.lineB = s32((amtX * 28.0f) * 4096.0f);
	actor->belowSensor.distanceFromCenter = 0;

	actor->adjacentSensor.flags = SENSOR_LINE;
	actor->adjacentSensor.lineA = s32((amtY * 4.0f) * 4096.0f);
	actor->adjacentSensor.lineB = s32((amtY * 32.0f) * 4096.0f);
	actor->adjacentSensor.distanceFromCenter = s32((amtX * 46.0f) * 4096.0f);

	u8 cat1 = 3, cat2 = 0;
	u32 bitfield1 = 0x6f, bitfield2 = 0xffbafffe;

	ActivePhysics::Info info = {
		0.0f, amtY*57.0f, amtX*20.0f, amtY*31.0f,
		cat1, cat2, bitfield1, bitfield2, 0, &dEn_c::collisionCallback};
	actor->aPhysics.initWithStruct(actor, &info);

	// Original trapezium was -12,12 to -48,48
	ActivePhysics::Info left = {
		amtX*-32.0f, amtY*55.0f, amtX*12.0f, amtY*30.0f,
		cat1, cat2, bitfield1, bitfield2, 0, &dEn_c::collisionCallback};
	actor->leftTrapAPhysics.initWithStruct(actor, &left);
	actor->leftTrapAPhysics.trpValue0 = amtX * 12.0f;
	actor->leftTrapAPhysics.trpValue1 = amtX * 12.0f;
	actor->leftTrapAPhysics.trpValue2 = amtX * -12.0f;
	actor->leftTrapAPhysics.trpValue3 = amtX * 12.0f;
	actor->leftTrapAPhysics.collisionCheckType = 3;

	ActivePhysics::Info right = {
		amtX*32.0f, amtY*55.0f, amtX*12.0f, amtY*30.0f,
		cat1, cat2, bitfield1, bitfield2, 0, &dEn_c::collisionCallback};
	actor->rightTrapAPhysics.initWithStruct(actor, &right);
	actor->rightTrapAPhysics.trpValue0 = amtX * -12.0f;
	actor->rightTrapAPhysics.trpValue1 = amtX * -12.0f;
	actor->rightTrapAPhysics.trpValue2 = amtX * -12.0f;
	actor->rightTrapAPhysics.trpValue3 = amtX * 12.0f;
	actor->rightTrapAPhysics.collisionCheckType = 3;

	ActivePhysics::Info stalk = {
		0.0f, amtY*12.0f, amtX*28.0f, amtY*12.0f,
		cat1, cat2, bitfield1, bitfield2, 0, &dEn_c::collisionCallback};
	actor->stalkAPhysics.initWithStruct(actor, &stalk);

}


//FIXME make this dEn_c->used...
extern "C" int SomeStrangeModification(dStageActor_c* actor);
extern "C" void DoStuffAndMarkDead(dStageActor_c *actor, Vec vector, float unk);
extern "C" int SmoothRotation(short* rot, u16 amt, int unk2);

void daMegaGoomba_c::powBlockActivated(bool isNotMPGP) {
}

CREATE_STATE(daMegaGoomba_c, Shrink);
CREATE_STATE(daMegaGoomba_c, Walk);
CREATE_STATE(daMegaGoomba_c, Turn);


//TODO better fix for possible bug with sign (ex. life=120; count=-9;)
bool daMegaGoomba_c::takeHit(char count) {
	OSReport("Taking a hit!\n");
	if(!this->already_hit) {
		int c = count;
		int l = this->life;
		if(l - c > 127) {
			c = 127 - l;
		}
		this->life -= c;
		// this->XSpeed += 0.10;

		// float rate = this->animationChr.getUpdateRate();
		// this->animationChr.setUpdateRate(rate+0.05);
		this->JumpHeight += 12.0;
		this->JumpDist += 12.0;
		this->JumpTime += 5.0;
		doStateChange(&StateID_Shrink);
		this->already_hit = true;
	}
	return (life <= 0) ? true : false;
}

#define ACTIVATE	1
#define DEACTIVATE	0

extern "C" void *EN_LandbarrelPlayerCollision(dEn_c* t, ActivePhysics *apThis, ActivePhysics *apOther);
void daMegaGoomba_c::spriteCollision(ActivePhysics *apThis, ActivePhysics *apOther) {
	//HE'S TOO BADASS TO STOP FOR SMALLER GOOMBAS
	#if 0
		float me = apThis->firstFloatArray[3];
		if(((this->direction == 1) && (me > 0.0)) || ((this->direction == 0) && (me < 0.0))) {
			dStateBase_c* state = this->acState.getCurrentState();
			if(!state->isEqual(&StateID_Turn)) {
				doStateChange(&StateID_Turn);
			}
		}
	#endif
}
void daMegaGoomba_c::playerCollision(ActivePhysics *apThis, ActivePhysics *apOther) { 
	if (apThis == &stalkAPhysics) {
		dEn_c::playerCollision(apThis, apOther);
		return;
	}

	/* * * * * * * * * * * * * * * * * * * * *
	 * 0=normal??,1=dontHit,2=dontKill
	 * daEnBrosBase_c ::player = 0
	 * daEnBrosBase_c::yoshi   = 0
	 * daEnPipePirahna::player = 1
	 * daEnPipePirahna::yoshi  = 1
	 * daEnKuriboBase_c::player = 0
	 * daEnKuriboBase_c::yoshi  = 0
	 * daEnLargeKuribo_c::player = 0
	 * daEnLargeKuribo_c::yoshi  = 2
	 * daEnNokonoko_c::player = 0
	 * daEnNokonoko_c::yoshi  = 0
	 * daEnSubBoss_c     = 2
	 *
	 * * * * * * * * * * * * * * * * * * * * */
	//FIXME rename and make part of dStageActor_c
	//unk=0 does _vfs, unk=1 does playSeCmnStep
	//char ret = usedForDeterminingStatePress_or_playerCollision(this, apThis, apOther, 0);

	if (tryHandleJumpedOn(apThis, apOther) == 0) {
		this->dEn_c::playerCollision(apThis, apOther);
		this->_vf220(apOther->owner);
		this->counter_504[apOther->owner->which_player] = 180;
	}
}

int daMegaGoomba_c::tryHandleJumpedOn(ActivePhysics *apThis, ActivePhysics *apOther) {
	float saveBounce = EnemyBounceValue;
	EnemyBounceValue = 5.2f;

	char ret = usedForDeterminingStatePress_or_playerCollision(this, apThis, apOther, 2);

	EnemyBounceValue = saveBounce;

	if(ret == 1 || ret == 3) {
		apOther->someFlagByte |= 2;
		if(this->takeHit(1)) {
			// kill me
			VEC2 eSpeed = {speed.x, speed.y};
			killWithSpecifiedState(apOther->owner, &eSpeed, &dEn_c::StateID_DieOther);
		}
	}

	return ret;
}
bool daMegaGoomba_c::collisionCat7_GroundPound(ActivePhysics *apThis, ActivePhysics *apOther) { 
	if (this->counter_504[apOther->owner->which_player] > 0) { return false; }
	VEC2 eSpeed = {speed.x, speed.y};
	killWithSpecifiedState(apOther->owner, &eSpeed, &dEn_c::StateID_DieOther);
	return true;
}
bool daMegaGoomba_c::collisionCat7_GroundPoundYoshi(ActivePhysics *apThis, ActivePhysics *apOther) { 
	return collisionCat7_GroundPound(apThis, apOther);
}

bool daMegaGoomba_c::collisionCat1_Fireball_E_Explosion(ActivePhysics *apThis, ActivePhysics *apOther) {
	return false;
}
bool daMegaGoomba_c::collisionCat2_IceBall_15_YoshiIce(ActivePhysics *apThis, ActivePhysics *apOther) {
	return false;
}
extern "C" void dAcPy_vf3F8(void* player, dEn_c* monster, int t);
bool daMegaGoomba_c::collisionCat3_StarPower(ActivePhysics *apThis, ActivePhysics *apOther) {
	if (tryHandleJumpedOn(apThis, apOther) == 0) {
		dAcPy_vf3F8(apOther->owner, this, 3);
		this->counter_504[apOther->owner->which_player] = 0xA;
	}
	return true;
}
bool daMegaGoomba_c::collisionCat9_RollingObject(ActivePhysics *apThis, ActivePhysics *apOther) {
	if(this->takeHit(1))
		doStateChange(&StateID_DieFall);
	return true;
}
bool daMegaGoomba_c::collisionCatA_PenguinMario(ActivePhysics *apThis, ActivePhysics *apOther) {
	if(this->takeHit(1))
		doStateChange(&StateID_DieFall);
	return true;
}
bool daMegaGoomba_c::collisionCatD_Drill(ActivePhysics *apThis, ActivePhysics *apOther) {
	return collisionCat7_GroundPound(apThis, apOther);
}
bool daMegaGoomba_c::collisionCat11_PipeCannon(ActivePhysics *apThis, ActivePhysics *apOther) {
	return true;
}
bool daMegaGoomba_c::collisionCat13_Hammer(ActivePhysics *apThis, ActivePhysics *apOther) {
	return true;
}
bool daMegaGoomba_c::collisionCat14_YoshiFire(ActivePhysics *apThis, ActivePhysics *apOther) {
	if(this->takeHit(1))
		doStateChange(&StateID_DieFall);
	return true;
}
void daMegaGoomba_c::addScoreWhenHit(void *other) {}
bool daMegaGoomba_c::_vf120(ActivePhysics *apThis, ActivePhysics *apOther) {
	return true; // Replicate existing broken behaviour
}
bool daMegaGoomba_c::_vf110(ActivePhysics *apThis, ActivePhysics *apOther) {
	return true; // Replicate existing broken behaviour
}
bool daMegaGoomba_c::_vf108(ActivePhysics *apThis, ActivePhysics *apOther) {
	return true; // Replicate existing broken behaviour
}

void daMegaGoomba_c::bindAnimChr_and_setUpdateRate(const char* name, int unk, float unk2, float rate) {
	nw4r::g3d::ResAnmChr anmChr = this->resFile.GetResAnmChr(name);
	this->animationChr.bind(&this->bodyModel, anmChr, unk);
	this->bodyModel.bindAnim(&this->animationChr, unk2);
	this->animationChr.setUpdateRate(rate);
}

void daMegaGoomba_c::dieFall_Begin() {
	this->dEn_c::dieFall_Begin();
	PlaySound(this, SE_EMY_KURIBO_L_DAMAGE_03);
}
void daMegaGoomba_c::dieFall_Execute() {
	
	this->timer = this->timer + 1.0;
	
	this->dying = this->dying + 0.15;
	
	this->pos.x = this->pos.x + 0.15;
	this->pos.y = this->pos.y + ((-0.2 * (this->dying*this->dying)) + 5);
	
	this->dEn_c::dieFall_Execute();
}

void daMegaGoomba_c::setupBodyModel() {
	allocator.link(-1, GameHeaps[0], 0, 0x20);

	this->resFile.data = getResource("kuriboBoss", "g3d/kuriboBoss.brres");
	nw4r::g3d::ResMdl mdl = this->resFile.GetResMdl("kuriboBig");
	bodyModel.setup(mdl, &allocator, 0x224, 1, 0);
	SetupTextures_Enemy(&bodyModel, 0);

	bool ret;
	nw4r::g3d::ResAnmChr anmChr = this->resFile.GetResAnmChr("walk");
	ret = this->animationChr.setup(mdl, anmChr, &this->allocator, 0);
	this->bindAnimChr_and_setUpdateRate("walk", 1, 0.0, 0.2);

	allocator.unlink();
}

void daMegaGoomba_c::setupCollision() {
	//POINTLESS WITH GROWTH
	this->scale.x = this->scale.y = this->scale.z = 0.666;

	this->collMgr.init(this, &belowSensor, 0, &adjacentSensor);

	char foo = this->appearsOnBackFence;
	this->pos_delta2.x = 0.0;
	this->pos_delta2.y = 16.0;
	this->pos_delta2.z = 0.0;

	this->pos.z = (foo == 0) ? 1500.0 : -2500.0;

	this->_518 = 2;

	//NOT NEEDED
	//this->doStateChange(&StateID_Walk);
}

int daMegaGoomba_c::onCreate() {
	/*80033230 daEnLkuribo_c::onCreate()*/
	this->setupBodyModel();
	this->max_speed.y = -4.0;
	this->direction = dSprite_c__getXDirectionOfFurthestPlayerRelativeToVEC3(this, this->pos);
	this->rot.y = (this->direction) ? 0xE000 : 0x2000;
	this->_518 = 2;

	isBigBoss = this->settings & 0xF;
	this->animationChr.setCurrentFrame(69.0);

	aPhysics.addToList();
	stalkAPhysics.addToList();
	leftTrapAPhysics.addToList();
	rightTrapAPhysics.addToList();

	this->_120 |= 0x200;

	this->_36D = 0;
	this->setupCollision();

	//HOMEMADE//
	speed.y = 0.0;
	dying = 0.0;
	rot.x = rot.z = 0;
	life = 3;
	already_hit = false;
	this->x_speed_inc = 0.1;
	this->pos.y -= 16.0;

	// 2.0 is good final speed
	this->XSpeed = 0.2;
	this->JumpHeight = 48.0;
	this->JumpDist = 64.0;
	this->JumpTime = 50.0;

	// doStateChange(&StateID_Grow);

	scale.x = 4.0f;
	scale.y = 4.0f;
	scale.z = 4.0f;
	setNewActivePhysicsRect(this, &this->scale);
	doStateChange(&StateID_Walk);

	this->onExecute();
	return true;
}

int daMegaGoomba_c::onDelete() {
	unstunPlayers();
	return true;
}

int daMegaGoomba_c::onExecute() {
	//80033450
	acState.execute();
	if (!hackOfTheCentury) {
		hackOfTheCentury = true;
	} else {
		checkZoneBoundaries(0);
	}
	updateModelMatrices();

	return true;
}

int daMegaGoomba_c::onDraw() {
	bodyModel.scheduleForDrawing();
	return true;
}


void daMegaGoomba_c::updateModelMatrices() {
	// This won't work with wrap because I'm lazy.
	matrix.translation(pos.x, pos.y, pos.z);
	matrix.applyRotationYXZ(&rot.x, &rot.y, &rot.z);

	bodyModel.setDrawMatrix(matrix);
	bodyModel.setScale(&scale);
	bodyModel.calcWorld(false);
}



// Shrink State
void daMegaGoomba_c::beginState_Shrink() {
	this->timer = 1.0;
	Xkey_count = 4;
	keysX[0] = (HermiteKey){  0.0, this->scale.y,        0.5 };
	keysX[1] = (HermiteKey){ 10.0, this->scale.y - 0.75, 0.5 };
	keysX[2] = (HermiteKey){ 20.0, this->scale.y - 0.35, 0.5 };
	keysX[3] = (HermiteKey){ 39.0, this->scale.y - 0.75, 0.5 };

	// disable being hit
	Vec tempVec = (Vec){0.0, 0.0, 0.0};
	setNewActivePhysicsRect(this,  &tempVec );
}
void daMegaGoomba_c::executeState_Shrink() { 
	this->timer += 1.0;
	
	float modifier = GetHermiteCurveValue(this->timer, this->keysX, Xkey_count);
	this->scale = (Vec){modifier, modifier, modifier};

	if(this->timer == 2.0)
		PlaySound(this, SE_EMY_KURIBO_L_DAMAGE_02);

	if (this->timer > 40.0) { doStateChange(&StateID_Walk); }
}
void daMegaGoomba_c::endState_Shrink() {
	// enable being hit
	setNewActivePhysicsRect(this, &this->scale);
	this->already_hit = false;
}



// Turn State
void daMegaGoomba_c::beginState_Turn() {
	this->direction ^= 1;
	this->speed.x = 0.0;
}
void daMegaGoomba_c::executeState_Turn() { 
	this->bodyModel._vf1C();

	this->HandleYSpeed();
	this->doSpriteMovement();

	/*this->_vf2D0();	//nullsub();*/
	int ret = SomeStrangeModification(this);

	if(ret & 1)
		this->speed.y = 0.0;
	if(ret & 4)
		this->pos.x = this->last_pos.x;
	DoStuffAndMarkDead(this, this->pos, 1.0);
	u16 amt = (this->direction == 0) ? 0x2000 : 0xE000;
	int done = SmoothRotation(&this->rot.y, amt, 0x80);
	if(done) {
		this->doStateChange(&StateID_Walk);
	}

	int frame = (int)(this->animationChr.getCurrentFrame() * 5.0);
	if ((frame == 100) || (frame == 325) || (frame == 550) || (frame == 775)) {
		ShakeScreen(StageScreen, 0, 1, 0, 0);
		stunPlayers();
		PlaySound(this, SE_BOSS_MORTON_GROUND_SHAKE);
	}

	if (isBigBoss) {
		if ((frame == 250) || (frame == 500) || (frame == 700) || (frame == 900))
			unstunPlayers();
	}
	else {
		if ((frame == 200) || (frame == 425) || (frame == 650) || (frame == 875))
			unstunPlayers();
	}
}
void daMegaGoomba_c::endState_Turn() {
	this->max_speed.x = (this->direction) ? -this->XSpeed : this->XSpeed;
}


// Walk State
void daMegaGoomba_c::beginState_Walk() {
	//inline this piece of code
	//YOU SUCK, WHOEVER ADDED THIS LINE OF CODE AND MADE ME SPEND AGES
	//HUNTING DOWN WHAT WAS BREAKING TURNING. -Treeki
	//this->direction = dSprite_c__getXDirectionOfFurthestPlayerRelativeToVEC3(this, this->pos);
	this->speed.x = this->speed.z = 0.0;
	this->max_speed.x = (this->direction) ? -this->XSpeed : this->XSpeed;
	this->speed.y = -4.0;
	this->y_speed_inc = -0.1875;
}
void daMegaGoomba_c::executeState_Walk() { 
	/* 800345e0 - daEnLkuribo_c::executeState_Walk() */
	this->bodyModel._vf1C();
	//HOMEMADE//
	this->HandleXSpeed();
	this->HandleYSpeed();
	this->doSpriteMovement();
	u16 amt = (this->direction == 0) ? 0x2000 : 0xE000;
	SmoothRotation(&this->rot.y, amt, 0x200);
	/*this->_vf2D0();	//nullsub();*/
	int ret = SomeStrangeModification(this);
	if(ret & 1)
		this->speed.y = 0.0;
	u32 bitfield = this->collMgr.outputMaybe;
	if(bitfield & (0x15<<this->direction)) {
		this->pos.x = this->last_pos.x;
		this->doStateChange(&StateID_Turn);
		//this->acState.setField10ToOne();
	}
	/*u32 bitfield2 = this->collMgr.adjacentTileProps[this->direction];
	if(bitfield2) {
		this->doStateChange(&StateID_Turn);
	}*/
	DoStuffAndMarkDead(this, this->pos, 1.0);


	int frame = (int)(this->animationChr.getCurrentFrame() * 5.0);
	if ((frame == 100) || (frame == 325) || (frame == 550) || (frame == 775)) {
		ShakeScreen(StageScreen, 0, 1, 0, 0);
		stunPlayers();
		PlaySound(this, SE_BOSS_MORTON_GROUND_SHAKE);
	}

	if (isBigBoss) {
		if ((frame == 250) || (frame == 500) || (frame == 700) || (frame == 900))
			unstunPlayers();
	}
	else {
		if ((frame == 200) || (frame == 425) || (frame == 650) || (frame == 875))
			unstunPlayers();
	}

	if(this->animationChr.isAnimationDone()) {
		this->animationChr.setCurrentFrame(0.0);

		int new_dir = dSprite_c__getXDirectionOfFurthestPlayerRelativeToVEC3(this, pos);
		if(this->direction != new_dir)
			doStateChange(&StateID_Turn);
	}
}
void daMegaGoomba_c::endState_Walk() { }





extern "C" void stunPlayer(void *, int);
extern "C" void unstunPlayer(void *);

void daMegaGoomba_c::stunPlayers() {
	for (int i = 0; i < 4; i++) {
		playerStunned[i] = false;

		dStageActor_c *player = GetSpecificPlayerActor(i);
		if (player) {
			if (player->collMgr.isOnTopOfTile() && player->currentZoneID == currentZoneID) {
				stunPlayer(player, 1);
				playerStunned[i] = true;
			}
		}
	}
}

void daMegaGoomba_c::unstunPlayers() {
	for (int i = 0; i < 4; i++) {
		dStageActor_c *player = GetSpecificPlayerActor(i);
		if (player && playerStunned[i]) {
			unstunPlayer(player);
		}
	}
}



void daMegaGoomba_c::dieOther_Begin() {
	animationChr.bind(&bodyModel, resFile.GetResAnmChr("damage"), true);
	bodyModel.bindAnim(&animationChr, 2.0f);
	speed.x = speed.y = speed.z = 0.0f;
	removeMyActivePhysics();

	PlaySound(this, SE_EMY_KURIBO_L_SPLIT_HPDP);

	rot.y = 0;
	counter_500 = 60;
}

void daMegaGoomba_c::dieOther_End() {
	dEn_c::dieOther_End();
}

void daMegaGoomba_c::dieOther_Execute() {
	bodyModel._vf1C();
	if (counter_500 == 0) {
		SpawnEffect("Wm_ob_icebreaksmk", 0, &pos, &(S16Vec){0,0,0}, &(Vec){5.0f, 5.0f, 5.0f});
		Delete(1);
	}
}


//
// processed\../src/effectvideo.cpp
//

#include <common.h>
#include <game.h>
#include <g3dhax.h>
#include <sfx.h>


extern "C" bool SpawnEffect(const char*, int, Vec*, S16Vec*, Vec*);


class EffectVideo : public dEn_c {
	int onCreate();
	int onExecute();
	int onDelete();

	u64 eventFlag;
	s32 timer;
	u32 delay;

	u32 effect;
	u8 type;
	float scale;

	static EffectVideo *build();

};


EffectVideo *EffectVideo::build() {
	void *buffer = AllocFromGameHeap1(sizeof(EffectVideo));
	return new(buffer) EffectVideo;
}


int EffectVideo::onCreate() {
	
	this->timer = 0;

	char eventNum	= (this->settings >> 24) & 0xFF;

	this->eventFlag = (u64)1 << (eventNum - 1);
	
	this->type		= (this->settings >> 16) & 0xF;
	this->effect	= this->settings & 0xFFF;
	this->scale		= float((this->settings >> 20) & 0xF) / 4.0;
	this->delay		= (this->settings >> 12) & 0xF * 30;
	
	if (this->scale == 0.0) { this->scale = 1.0; }

	this->onExecute();
	return true;
}


int EffectVideo::onDelete() {
	return true;
}


int EffectVideo::onExecute() {

	if (dFlagMgr_c::instance->flags & this->eventFlag) {

		if (this->timer == this->delay) {

			if (this->type == 0) { // Plays a sound
				PlaySoundAsync(this, this->effect);
			}
	
			else {	// Plays an Effect

				const char *efName = 0;

				switch (this->effect) {
					case 1: efName = "Wm_mr_2dlandsmoke"; break;
					case 2: efName = "Wm_mr_cmnsndlandsmk"; break;
					case 3: efName = "Wm_mr_cmnlandsmoke"; break;
					case 4: efName = "Wm_en_landsmoke"; break;
					case 5: efName = "Wm_en_landsmoke_s"; break;
					case 6: efName = "Wm_en_sndlandsmk"; break;
					case 7: efName = "Wm_en_sndlandsmk_s"; break;
					case 8: efName = "Wm_en_burst_big"; break;
					case 9: efName = "Wm_en_burst_m"; break;
					case 10: efName = "Wm_en_burst_s"; break;
					case 11: efName = "Wm_en_burst_ss"; break;
					case 12: efName = "Wm_en_burst_water01"; break;
					case 13: efName = "Wm_en_burst_water02"; break;
					case 14: efName = "Wm_en_cmnwatertail"; break;
					case 15: efName = "Wm_en_cmnwaterdash"; break;
					case 16: efName = "Wm_en_cmnwater02"; break;
					case 17: efName = "Wm_en_cmnwater"; break;
					case 18: efName = "Wm_en_waterwave_in"; break;
					case 19: efName = "Wm_en_waterwave_in_a"; break;
					case 20: efName = "Wm_en_waterwave_in_b"; break;
					case 21: efName = "Wm_en_firevanish"; break;
					case 22: efName = "Wm_en_watersplash"; break;
					case 23: efName = "Wm_en_watersplash_cld"; break;
					case 24: efName = "Wm_mr_watersplash"; break;
					case 25: efName = "Wm_en_poisoninbig01"; break;
					case 26: efName = "Wm_en_poisoninbig02"; break;
					case 27: efName = "Wm_en_poisonwave"; break;
					case 28: efName = "Wm_en_poisonwave_a"; break;
					case 29: efName = "Wm_en_poisonwave_b"; break;
					case 30: efName = "Wm_en_cmnmagmawave"; break;
					case 31: efName = "Wm_en_magmawave"; break;
					case 32: efName = "Wm_en_magmawave_a"; break;
					case 33: efName = "Wm_en_magmawave_b"; break;
					case 34: efName = "Wm_en_vshit"; break;
					case 35: efName = "Wm_en_vshit_hit"; break;
					case 36: efName = "Wm_en_vshit_glow"; break;
					case 37: efName = "Wm_en_vshit_star"; break;
					case 38: efName = "Wm_en_vshit_ring"; break;
					case 39: efName = "Wm_en_comattack"; break;
					case 40: efName = "Wm_ob_cmnshotstar"; break;
					case 41: efName = "Wm_ob_cmnshothit"; break;
					case 42: efName = "Wm_ob_cmnshotspark"; break;
					case 43: efName = "Wm_ob_cmnsparkloop"; break;
					case 44: efName = "Wm_ob_cmnspark"; break;
					case 45: efName = "Wm_ob_cmnicekira"; break;
					case 46: efName = "Wm_ob_cmnrockpiece"; break;
					case 47: efName = "Wm_ob_cmnboxpiece"; break;
					case 48: efName = "Wm_ob_cmnboxsmoke"; break;
					case 49: efName = "Wm_ob_cmnboxgrain"; break;
					case 50: efName = "Wm_en_hit"; break;
					case 51: efName = "Wm_en_hit_ring"; break;
					case 52: efName = "Wm_mr_misshit"; break;
					case 53: efName = "Wm_mr_misshit_ring"; break;
					case 54: efName = "Wm_en_quicksand"; break;
					case 55: efName = "Wm_ob_envsunlight"; break;
					case 56: efName = "Wm_ob_envsunlight_a"; break;
					case 57: efName = "Wm_ob_envsunlight_b"; break;
					case 58: efName = "Wm_mr_landsmoke"; break;
					case 59: efName = "Wm_mr_landsmoke_s"; break;
					case 60: efName = "Wm_mr_landsmoke_ss"; break;
					case 61: efName = "Wm_mr_sealandsmk"; break;
					case 62: efName = "Wm_mr_sealandsmk_s"; break;
					case 63: efName = "Wm_mr_sealandsmk_ss"; break;
					case 64: efName = "Wm_mr_sndlandsmk"; break;
					case 65: efName = "Wm_mr_sndlandsmk_s"; break;
					case 66: efName = "Wm_mr_sndlandsmk_ss"; break;
					case 67: efName = "Wm_mr_beachlandsmk"; break;
					case 68: efName = "Wm_mr_beachlandsmk_s"; break;
					case 69: efName = "Wm_mr_beachlandsmk_ss"; break;
					case 70: efName = "Wm_mr_slipsmoke"; break;
					case 71: efName = "Wm_mr_slipsmoke_ss"; break;
					case 72: efName = "Wm_mr_slipsmoke_big"; break;
					case 73: efName = "Wm_mr_sndslipsmk"; break;
					case 74: efName = "Wm_mr_sndslipsmk_ss"; break;
					case 75: efName = "Wm_mr_beachslipsmk"; break;
					case 76: efName = "Wm_mr_beachslipsmk_ss"; break;
					case 77: efName = "Wm_mr_iceslipsmk"; break;
					case 78: efName = "Wm_mr_iceslipsmk_ss"; break;
					case 79: efName = "Wm_mr_brakesmoke"; break;
					case 80: efName = "Wm_mr_brakesmoke_ss"; break;
					case 81: efName = "Wm_mr_sndbrakesmk"; break;
					case 82: efName = "Wm_mr_sndbrakesmk_ss"; break;
					case 83: efName = "Wm_mr_beachbrakesmk"; break;
					case 84: efName = "Wm_mr_beachbrakesmk_ss"; break;
					case 85: efName = "Wm_mr_icebrakesmk"; break;
					case 86: efName = "Wm_mr_icebrakesmk_ss"; break;
					case 87: efName = "Wm_mr_drop"; break;
					case 88: efName = "Wm_mr_quicksand"; break;
					case 89: efName = "Wm_mr_swimpaddle"; break;
					case 90: efName = "Wm_mr_flutterkick"; break;
					case 91: efName = "Wm_mr_ivy"; break;
					case 92: efName = "Wm_en_firebar_fire"; break;
					case 93: efName = "Wm_en_firebar_ind"; break;
					case 94: efName = "Wm_en_firebar"; break;
					case 95: efName = "Wm_mr_fireball_a"; break;
					case 96: efName = "Wm_mr_fireball_b"; break;
					case 97: efName = "Wm_mr_fireball"; break;
					case 98: efName = "Wm_mr_iceball_a"; break;
					case 99: efName = "Wm_mr_iceball_b"; break;
					case 100 : efName = "Wm_mr_iceball"; break;
					case 101 : efName = "Wm_ob_icemisshit"; break;
					case 102 : efName = "Wm_ob_icemisshit_smk"; break;
					case 103 : efName = "Wm_mr_fireball_hit"; break;
					case 104 : efName = "Wm_mr_fireball_hit01"; break;
					case 105 : efName = "Wm_en_bubble"; break;
					case 106 : efName = "Wm_en_bubble_a"; break;
					case 107 : efName = "Wm_en_bubble_b"; break;
					case 108 : efName = "Wm_mr_startail"; break;
					case 109 : efName = "Wm_mr_movecliff"; break;
					case 110 : efName = "Wm_mr_cliffcatch"; break;
					case 111 : efName = "Wm_mr_cliffcatch_cd"; break;
					case 112 : efName = "Wm_en_iron"; break;
					case 113 : efName = "Wm_mr_wallkick_r"; break;
					case 114 : efName = "Wm_mr_wallkick_up_r"; break;
					case 115 : efName = "Wm_mr_wallkick_cld_r"; break;
					case 116 : efName = "Wm_mr_wallkick_dn_r"; break;
					case 117 : efName = "Wm_mr_wallkick_c_r"; break;
					case 118 : efName = "Wm_mr_wallkick_b_r"; break;
					case 119 : efName = "Wm_mr_wallkick_l"; break;
					case 120 : efName = "Wm_mr_wallkick_up_l"; break;
					case 121 : efName = "Wm_mr_wallkick_cld_l"; break;
					case 122 : efName = "Wm_mr_wallkick_dn_l"; break;
					case 123 : efName = "Wm_mr_wallkick_c_l"; break;
					case 124 : efName = "Wm_mr_wallkick_b_l"; break;
					case 125 : efName = "Wm_mr_wallkick_s_r"; break;
					case 126 : efName = "Wm_mr_wallkick_up_s_r"; break;
					case 127 : efName = "Wm_mr_wallkick_cld_s_r"; break;
					case 128 : efName = "Wm_mr_wallkick_dn_s_r"; break;
					case 129 : efName = "Wm_mr_wallkick_c_s_r"; break;
					case 130 : efName = "Wm_mr_wallkick_b_s_r"; break;
					case 131 : efName = "Wm_mr_wallkick_s_l"; break;
					case 132 : efName = "Wm_mr_wallkick_up_s_l"; break;
					case 133 : efName = "Wm_mr_wallkick_cld_s_l"; break;
					case 134 : efName = "Wm_mr_wallkick_dn_s_l"; break;
					case 135 : efName = "Wm_mr_wallkick_c_s_l"; break;
					case 136 : efName = "Wm_mr_wallkick_b_s_l"; break;
					case 137 : efName = "Wm_mr_wallkick_ss_r"; break;
					case 138 : efName = "Wm_mr_wallkick_up_ss_r"; break;
					case 139 : efName = "Wm_mr_wallkick_cld_ss_r"; break;
					case 140 : efName = "Wm_mr_wallkick_dn_ss_r"; break;
					case 141 : efName = "Wm_mr_wallkick_b_ss_r"; break;
					case 142 : efName = "Wm_mr_wallkick_c_ss_r"; break;
					case 143 : efName = "Wm_mr_wallkick_ss_l"; break;
					case 144 : efName = "Wm_mr_wallkick_up_ss_l"; break;
					case 145 : efName = "Wm_mr_wallkick_cld_ss_l"; break;
					case 146 : efName = "Wm_mr_wallkick_dn_ss_l"; break;
					case 147 : efName = "Wm_mr_wallkick_b_ss_l"; break;
					case 148 : efName = "Wm_mr_wallkick_c_ss_l"; break;
					case 149 : efName = "Wm_mr_wallslip_r"; break;
					case 150 : efName = "Wm_mr_wallslip_l"; break;
					case 151 : efName = "Wm_mr_wallslip_cld"; break;
					case 152 : efName = "Wm_mr_wallslip_s_r"; break;
					case 153 : efName = "Wm_mr_wallslip_s_l"; break;
					case 154 : efName = "Wm_mr_wallslip_cld_s"; break;
					case 155 : efName = "Wm_mr_wallslip_ss_r"; break;
					case 156 : efName = "Wm_mr_wallslip_ss_l"; break;
					case 157 : efName = "Wm_mr_wallslip_cld_ss"; break;
					case 158 : efName = "Wm_mr_hardhit"; break;
					case 159 : efName = "Wm_mr_hardhit_glow"; break;
					case 160 : efName = "Wm_mr_hardhit_spak"; break;
					case 161 : efName = "Wm_mr_hardhit_grain"; break;
					case 162 : efName = "Wm_mr_kickhit"; break;
					case 163 : efName = "Wm_mr_kick_glow"; break;
					case 164 : efName = "Wm_mr_kick_grain"; break;
					case 165 : efName = "Wm_mr_softhit"; break;
					case 166 : efName = "Wm_mr_softhit_glow"; break;
					case 167 : efName = "Wm_mr_softhit_spak"; break;
					case 168 : efName = "Wm_mr_wirehit"; break;
					case 169 : efName = "Wm_mr_wirehit_line"; break;
					case 170 : efName = "Wm_mr_wirehit_star"; break;
					case 171 : efName = "Wm_mr_wirehit_glow"; break;
					case 172 : efName = "Wm_mr_wirehit_hit"; break;
					case 173 : efName = "Wm_ob_coin"; break;
					case 174 : efName = "Wm_ob_coinkira"; break;
					case 175 : efName = "Wm_ob_bluecoinkira"; break;
					case 176 : efName = "Wm_ob_greencoinkira"; break;
					case 177 : efName = "Wm_ob_greencoinkira_c"; break;
					case 178 : efName = "Wm_ob_greencoinkira_b"; break;
					case 179 : efName = "Wm_ob_greencoinkira_a"; break;
					case 180 : efName = "Wm_ob_redcoinkira"; break;
					case 181 : efName = "Wm_ob_starcoinget"; break;
					case 182 : efName = "Wm_ob_starcoinget_gl"; break;
					case 183 : efName = "Wm_ob_starcoinget_lighit"; break;
					case 184 : efName = "Wm_ob_starcoinget_hit"; break;
					case 185 : efName = "Wm_ob_starcoinget_str"; break;
					case 186 : efName = "Wm_ob_starcoinget_ring"; break;
					case 187 : efName = "Wm_mr_electricshock"; break;
					case 188 : efName = "Wm_mr_electricshock_glw"; break;
					case 189 : efName = "Wm_mr_electricshock_biri01"; break;
					case 190 : efName = "Wm_mr_electricshock_biri02"; break;
					case 191 : efName = "Wm_mr_electricshock_kira"; break;
					case 192 : efName = "Wm_mr_electricshock_s"; break;
					case 193 : efName = "Wm_mr_electricshock_glw_s"; break;
					case 194 : efName = "Wm_mr_electricshock_biri01_s"; break;
					case 195 : efName = "Wm_mr_electricshock_biri02_s"; break;
					case 196 : efName = "Wm_mr_electricshock_kira_s"; break;
					case 197 : efName = "Wm_en_birikyu"; break;
					case 198 : efName = "Wm_en_birikyu_glw"; break;
					case 199 : efName = "Wm_en_birikyu_kira"; break;
					case 200 : efName = "Wm_en_birikyu_biri"; break;
					case 201 : efName = "Wm_mr_1upkira"; break;
					case 202 : efName = "Wm_mr_1upkira_spin"; break;
					case 203 : efName = "Wm_mr_1upkira_01"; break;
					case 204 : efName = "Wm_mr_1upkira_02"; break;
					case 205 : efName = "Wm_mr_1upkira_s"; break;
					case 206 : efName = "Wm_mr_1upkira_spin_s"; break;
					case 207 : efName = "Wm_mr_1upkira_01_s"; break;
					case 208 : efName = "Wm_mr_1upkira_02_s"; break;
					case 209 : efName = "Wm_mr_1upkira_ss"; break;
					case 210 : efName = "Wm_mr_1upkira_spin_ss"; break;
					case 211 : efName = "Wm_mr_1upkira_01_ss"; break;
					case 212 : efName = "Wm_en_hanapetal"; break;
					case 213 : efName = "Wm_en_hanapetal_a"; break;
					case 214 : efName = "Wm_en_hanapetal_b"; break;
					case 215 : efName = "Wm_en_movecloud"; break;
					case 216 : efName = "Wm_mr_cloud_on"; break;
					case 217 : efName = "Wm_en_blockcloud"; break;
					case 218 : efName = "Wm_en_hanasnort"; break;
					case 219 : efName = "Wm_en_hanasnort_r"; break;
					case 220 : efName = "Wm_en_hanasnort_l"; break;
					case 221 : efName = "Wm_en_hanasnort_cld"; break;
					case 222 : efName = "Wm_mr_gauge"; break;
					case 223 : efName = "Wm_mr_flaggetkira"; break;
					case 224 : efName = "Wm_mr_flaggetkira_s"; break;
					case 225 : efName = "Wm_mr_flaggetkira_ss"; break;
					case 226 : efName = "Wm_ob_flagget"; break;
					case 227 : efName = "Wm_ob_flagget_kira"; break;
					case 228 : efName = "Wm_ob_flaggetkira_cld"; break;
					case 229 : efName = "Wm_ob_flagget_light"; break;
					case 230 : efName = "Wm_ob_icethaw"; break;
					case 231 : efName = "Wm_ob_icebreak"; break;
					case 232 : efName = "Wm_ob_icebreakwt"; break;
					case 233 : efName = "Wm_ob_icebreaksmk"; break;
					case 234 : efName = "Wm_ob_icewait"; break;
					case 235 : efName = "Wm_ob_icewaitwat"; break;
					case 236 : efName = "Wm_ob_iceattack"; break;
					case 237 : efName = "Wm_ob_iceattackkira"; break;
					case 238 : efName = "Wm_ob_iceattackline"; break;
					case 239 : efName = "Wm_ob_iceattacksmk"; break;
					case 240 : efName = "Wm_ob_icehit"; break;
					case 241 : efName = "Wm_ob_icehitwat"; break;
					case 242 : efName = "Wm_ob_icehithit"; break;
					case 243 : efName = "Wm_ob_icehitsmk"; break;
					case 244 : efName = "Wm_ob_iceevaporate"; break;
					case 245 : efName = "Wm_ob_icepoison"; break;
					case 246 : efName = "Wm_ob_waterbreak"; break;
					case 247 : efName = "Wm_ob_waterbreak_a"; break;
					case 248 : efName = "Wm_ob_waterbreak_b"; break;
					case 249 : efName = "Wm_ob_waterbreak_c"; break;
					case 250 : efName = "Wm_en_firesnk_icehit_h"; break;
					case 251 : efName = "Wm_en_firesnk_icehitsmk_h"; break;
					case 252 : efName = "Wm_en_firesnk_icehit_b"; break;
					case 253 : efName = "Wm_en_firesnk_icehitsmk_b"; break;
					case 254 : efName = "Wm_en_firesnkspark01"; break;
					case 255 : efName = "Wm_en_firesnkspark02"; break;
					case 256 : efName = "Wm_mr_vshipattack"; break;
					case 257 : efName = "Wm_mr_vshipattack_line"; break;
					case 258 : efName = "Wm_mr_vshipattack_hosi"; break;
					case 259 : efName = "Wm_mr_vshipattack_gl"; break;
					case 260 : efName = "Wm_mr_vshipattack_ud"; break;
					case 261 : efName = "Wm_mr_vshipattack_ind"; break;
					case 262 : efName = "Wm_mr_vshipattack_ind_a"; break;
					case 263 : efName = "Wm_mr_vshipattack_ind_c"; break;
					case 264 : efName = "Wm_mr_vshipattack_ind_b"; break;
					case 265 : efName = "Wm_mr_spinstart"; break;
					case 266 : efName = "Wm_mr_propellertail"; break;
					case 267 : efName = "Wm_mr_p_iceslip"; break;
					case 268 : efName = "Wm_mr_p_snowslip"; break;
					case 269 : efName = "Wm_mr_penguinsmoke"; break;
					case 270 : efName = "Wm_mr_pdesertsmoke"; break;
					case 271 : efName = "Wm_mr_pbeachsmoke"; break;
					case 272 : efName = "Wm_mr_penguinsnow"; break;
					case 273 : efName = "Wm_mr_penguinice"; break;
					case 274 : efName = "Wm_mr_spinsmoke"; break;
					case 275 : efName = "Wm_mr_spinjump"; break;
					case 276 : efName = "Wm_mr_spinjump_re"; break;
					case 277 : efName = "Wm_mr_spindepart"; break;
					case 278 : efName = "Wm_mr_spindepart_a"; break;
					case 279 : efName = "Wm_mr_spindepart_b"; break;
					case 280 : efName = "Wm_mr_spindown"; break;
					case 281 : efName = "Wm_mr_spindown_a"; break;
					case 282 : efName = "Wm_mr_spindown_b"; break;
					case 283 : efName = "Wm_mr_spindownline"; break;
					case 284 : efName = "Wm_mr_normalspin_pm"; break;
					case 285 : efName = "Wm_mr_normalspin"; break;
					case 286 : efName = "Wm_mr_halfspin"; break;
					case 287 : efName = "Wm_en_shellredtail"; break;
					case 288 : efName = "Wm_en_shellgreentail"; break;
					case 289 : efName = "Wm_mr_starkira"; break;
					case 290 : efName = "Wm_mr_starkira_a"; break;
					case 291 : efName = "Wm_mr_starkira_b"; break;
					case 292 : efName = "Wm_mr_starkira_s"; break;
					case 293 : efName = "Wm_mr_starkira_a_s"; break;
					case 294 : efName = "Wm_mr_starkira_b_s"; break;
					case 295 : efName = "Wm_ob_itemget"; break;
					case 296 : efName = "Wm_ob_itemget_hitlighit"; break;
					case 297 : efName = "Wm_ob_itemget_hit"; break;
					case 298 : efName = "Wm_ob_itemget_ring"; break;
					case 299 : efName = "Wm_mr_itemget01"; break;
					case 300 : efName = "Wm_mr_itemget02"; break;
					case 301 : efName = "Wm_ob_itemappear"; break;
					case 302 : efName = "Wm_ob_itemappear_r"; break;
					case 303 : efName = "Wm_ob_itemappear_gl"; break;
					case 304 : efName = "Wm_ob_itemappear_ss"; break;
					case 305 : efName = "Wm_ob_itemappear_r_ss"; break;
					case 306 : efName = "Wm_ob_itemappear_gl_ss"; break;
					case 307 : efName = "Wm_ob_startail"; break;
					case 308 : efName = "Wm_ob_startail_star"; break;
					case 309 : efName = "Wm_ob_startail_kira"; break;
					case 310 : efName = "Wm_ob_itempropeller"; break;
					case 311 : efName = "Wm_ob_powdown"; break;
					case 312 : efName = "Wm_ob_powdown_ind"; break;
					case 313 : efName = "Wm_ob_powdown_ind_a"; break;
					case 314 : efName = "Wm_ob_powdown_ind_c"; break;
					case 315 : efName = "Wm_ob_powdown_ind_b"; break;
					case 316 : efName = "Wm_ob_itemlandsmoke"; break;
					case 317 : efName = "Wm_ob_itemsealandsmk"; break;
					case 318 : efName = "Wm_ob_itemsndlandsmk"; break;
					case 319 : efName = "Wm_en_spindamage"; break;
					case 320 : efName = "Wm_en_spindamage_rg"; break;
					case 321 : efName = "Wm_en_spindamage_star"; break;
					case 322 : efName = "Wm_en_spindamage_big"; break;
					case 323 : efName = "Wm_en_spindamage_big_st"; break;
					case 324 : efName = "Wm_en_spindamage_big_rg"; break;
					case 325 : efName = "Wm_mr_atitismoke"; break;
					case 326 : efName = "Wm_en_sanbosmoke"; break;
					case 327 : efName = "Wm_en_sanbospillsand"; break;
					case 328 : efName = "Wm_en_sanbohit"; break;
					case 329 : efName = "Wm_en_sanbohit_smk"; break;
					case 330 : efName = "Wm_en_sanbohit_hit"; break;
					case 331 : efName = "Wm_en_sanbohit_ring"; break;
					case 332 : efName = "Wm_en_sanbohitsmk"; break;
					case 333 : efName = "Wm_en_keronpafire"; break;
					case 334 : efName = "Wm_en_keronpafire_f"; break;
					case 335 : efName = "Wm_en_keronpafire_ca"; break;
					case 336 : efName = "Wm_en_keronpalight"; break;
					case 337 : efName = "Wm_en_explosion"; break;
					case 338 : efName = "Wm_en_explosion_ln"; break;
					case 339 : efName = "Wm_en_explosion_gl01"; break;
					case 340 : efName = "Wm_en_explosion_hd"; break;
					case 341 : efName = "Wm_en_explosion_un"; break;
					case 342 : efName = "Wm_en_explosion_gl02"; break;
					case 343 : efName = "Wm_en_explosion_smk"; break;
					case 344 : efName = "Wm_en_bombheibreak"; break;
					case 345 : efName = "Wm_en_bombignition"; break;
					case 346 : efName = "Wm_en_bomignition_ln"; break;
					case 347 : efName = "Wm_en_bomignition_gl01"; break;
					case 348 : efName = "Wm_en_bomignition_pati"; break;
					case 349 : efName = "Wm_mr_sanddive"; break;
					case 350 : efName = "Wm_mr_sanddive_sd"; break;
					case 351 : efName = "Wm_mr_sanddive_in"; break;
					case 352 : efName = "Wm_mr_sanddive_out"; break;
					case 353 : efName = "Wm_mr_sanddive_smk"; break;
					case 354 : efName = "Wm_mr_sanddive_m"; break;
					case 355 : efName = "Wm_mr_sanddive_sd_m"; break;
					case 356 : efName = "Wm_mr_sanddive_in_m"; break;
					case 357 : efName = "Wm_mr_sanddive_out_m"; break;
					case 358 : efName = "Wm_mr_sanddive_smk_m"; break;
					case 359 : efName = "Wm_mr_sanddive_s"; break;
					case 360 : efName = "Wm_mr_sanddive_sb_s"; break;
					case 361 : efName = "Wm_mr_sanddive_smk_s"; break;
					case 362 : efName = "Wm_mr_sandsplash"; break;
					case 363 : efName = "Wm_en_dossunfall01"; break;
					case 364 : efName = "Wm_en_dossunfall02"; break;
					case 365 : efName = "Wm_en_dossunfall03"; break;
					case 366 : efName = "Wm_en_kuribobigsplit"; break;
					case 367 : efName = "Wm_en_kuribobigsplit_sk"; break;
					case 368 : efName = "Wm_en_kuribobigsplit_ht"; break;
					case 369 : efName = "Wm_en_kuribobigsplit_gr02"; break;
					case 370 : efName = "Wm_en_kuribobigsplit_gr01"; break;
					case 371 : efName = "Wm_en_kuribobigsplit_rg"; break;
					case 372 : efName = "Wm_en_kuribosplit"; break;
					case 373 : efName = "Wm_en_kuribosplit_gl02"; break;
					case 374 : efName = "Wm_en_kuribosplit_gl01"; break;
					case 375 : efName = "Wm_en_kuribosplit_sk"; break;
					case 376 : efName = "Wm_en_teresatail"; break;
					case 377 : efName = "Wm_en_teresavanish"; break;
					case 378 : efName = "Wm_en_obakedoor"; break;
					case 379 : efName = "Wm_en_obakedoor_sm"; break;
					case 380 : efName = "Wm_en_obakedoor_ic"; break;
					case 381 : efName = "Wm_mr_yoshistep"; break;
					case 382 : efName = "Wm_mr_yoshistep_a"; break;
					case 383 : efName = "Wm_mr_yoshistep_a_cld"; break;
					case 384 : efName = "Wm_mr_yoshistep_b"; break;
					case 385 : efName = "Wm_mr_fruitget"; break;
					case 386 : efName = "Wm_mr_fruitget_w"; break;
					case 387 : efName = "Wm_mr_fruitget_h"; break;
					case 388 : efName = "Wm_ob_eggbreak_gr"; break;
					case 389 : efName = "Wm_ob_eggbreak_rd"; break;
					case 390 : efName = "Wm_ob_eggbreak_yw"; break;
					case 391 : efName = "Wm_ob_eggbreak_bl"; break;
					case 392 : efName = "Wm_mr_yoshifire"; break;
					case 393 : efName = "Wm_mr_yoshifire_a"; break;
					case 394 : efName = "Wm_mr_yoshifire_b"; break;
					case 395 : efName = "Wm_mr_yoshiiceball"; break;
					case 396 : efName = "Wm_mr_yoshiiceball_b"; break;
					case 397 : efName = "Wm_mr_yoshiiceball_a"; break;
					case 398 : efName = "Wm_mr_yoshifirehit"; break;
					case 399 : efName = "Wm_mr_yoshifirehit01"; break;
					case 400 : efName = "Wm_mr_yoshiicehit"; break;
					case 401 : efName = "Wm_mr_yoshiicehit_a"; break;
					case 402 : efName = "Wm_mr_yoshiicehit_b"; break;
					case 403 : efName = "Wm_mr_yssweatrun"; break;
					case 404 : efName = "Wm_mr_yssweat"; break;
					case 405 : efName = "Wm_mr_ystonguehit"; break;
					case 406 : efName = "Wm_mr_ystonguehit_a"; break;
					case 407 : efName = "Wm_en_crowhit"; break;
					case 408 : efName = "Wm_en_crowfly"; break;
					case 409 : efName = "Wm_en_crowattack_r"; break;
					case 410 : efName = "Wm_en_crowattack_l"; break;
					case 411 : efName = "Wm_en_pakkunfire"; break;
					case 412 : efName = "Wm_en_pakkunfire00"; break;
					case 413 : efName = "Wm_en_firebros_fire"; break;
					case 414 : efName = "Wm_en_firebros_fire_a"; break;
					case 415 : efName = "Wm_en_firebros_fire_b"; break;
					case 416 : efName = "Wm_mr_magmawave"; break;
					case 417 : efName = "Wm_mr_magmawave_a"; break;
					case 418 : efName = "Wm_mr_magmawave_b"; break;
					case 419 : efName = "Wm_ob_magmagear"; break;
					case 420 : efName = "Wm_mr_poisonwave"; break;
					case 421 : efName = "Wm_mr_poisonwave_a"; break;
					case 422 : efName = "Wm_mr_poisonwave_b"; break;
					case 423 : efName = "Wm_mr_waterwave_in"; break;
					case 424 : efName = "Wm_mr_waterwave_in_a"; break;
					case 425 : efName = "Wm_mr_waterwave_in_b"; break;
					case 426 : efName = "Wm_mr_waterwave_in_c"; break;
					case 427 : efName = "Wm_mr_waterwave_in_d"; break;
					case 428 : efName = "Wm_mr_waterwave_out"; break;
					case 429 : efName = "Wm_mr_waterwave_out_a"; break;
					case 430 : efName = "Wm_mr_waterwave_out_b"; break;
					case 431 : efName = "Wm_mr_waterwave_out_c"; break;
					case 432 : efName = "Wm_mr_waterwave_in_ss"; break;
					case 433 : efName = "Wm_mr_waterwave_in_a_ss"; break;
					case 434 : efName = "Wm_mr_waterwave_in_b_ss"; break;
					case 435 : efName = "Wm_mr_waterwave_out_ss"; break;
					case 436 : efName = "Wm_mr_waterwave_out_a_ss"; break;
					case 437 : efName = "Wm_mr_waterwave_out_b_ss"; break;
					case 438 : efName = "Wm_mr_waterrun_l_ss"; break;
					case 439 : efName = "Wm_mr_waterrun_r_ss"; break;
					case 440 : efName = "Wm_mr_waterswim"; break;
					case 441 : efName = "Wm_ob_magmasign01"; break;
					case 442 : efName = "Wm_ob_magmasign02"; break;
					case 443 : efName = "Wm_ob_firespillarunder"; break;
					case 444 : efName = "Wm_ob_firespillar02"; break;
					case 445 : efName = "Wm_ob_firespillar01"; break;
					case 446 : efName = "Wm_mr_foot_snow"; break;
					case 447 : efName = "Wm_mr_foot_ice"; break;
					case 448 : efName = "Wm_mr_foot_sand"; break;
					case 449 : efName = "Wm_mr_foot_beach"; break;
					case 450 : efName = "Wm_mr_foot_water"; break;
					case 451 : efName = "Wm_mr_turn_beach_r"; break;
					case 452 : efName = "Wm_mr_turn_beach_l"; break;
					case 453 : efName = "Wm_mr_turn_water_r"; break;
					case 454 : efName = "Wm_mr_turn_water_l"; break;
					case 455 : efName = "Wm_mr_turn_ice_r"; break;
					case 456 : efName = "Wm_mr_turn_ice_l"; break;
					case 457 : efName = "Wm_mr_turn_sand_r"; break;
					case 458 : efName = "Wm_mr_turn_sand_l"; break;
					case 459 : efName = "Wm_mr_turn_snow_r"; break;
					case 460 : efName = "Wm_mr_turn_snow_l"; break;
					case 461 : efName = "Wm_mr_turn_usual_r"; break;
					case 462 : efName = "Wm_mr_turn_usual_l"; break;
					case 463 : efName = "Wm_ob_sandpillar01"; break;
					case 464 : efName = "Wm_ob_sandpillar02"; break;
					case 465 : efName = "Wm_ob_spillarsign01"; break;
					case 466 : efName = "Wm_ob_spillarsign02"; break;
					case 467 : efName = "Wm_mr_spsmoke"; break;
					case 468 : efName = "Wm_en_spsmoke"; break;
					case 469 : efName = "Wm_en_sphitsmoke"; break;
					case 470 : efName = "Wm_mr_sprisesmoke"; break;
					case 471 : efName = "Wm_en_huhubreathstart"; break;
					case 472 : efName = "Wm_en_huhubreath"; break;
					case 473 : efName = "Wm_en_huhuhaze"; break;
					case 474 : efName = "Wm_en_huhufloat"; break;
					case 475 : efName = "Wm_en_huhudamage01"; break;
					case 476 : efName = "Wm_en_huhudamage02"; break;
					case 477 : efName = "Wm_en_huhurevival01"; break;
					case 478 : efName = "Wm_en_huhurevival02"; break;
					case 479 : efName = "Wm_mr_wfloatsplash"; break;
					case 480 : efName = "Wm_mr_wfloatsplash_a"; break;
					case 481 : efName = "Wm_mr_wfloatsplash_b"; break;
					case 482 : efName = "Wm_en_wfsplash_in_r"; break;
					case 483 : efName = "Wm_en_wfsplash_in01_r"; break;
					case 484 : efName = "Wm_en_wfsplash_in02_r"; break;
					case 485 : efName = "Wm_en_wfsplash_in_l"; break;
					case 486 : efName = "Wm_en_wfsplash_in01_l"; break;
					case 487 : efName = "Wm_en_wfsplash_in02_l"; break;
					case 488 : efName = "Wm_en_wfsplash_out_r"; break;
					case 489 : efName = "Wm_en_wfsplash_out01_r"; break;
					case 490 : efName = "Wm_en_wfsplash_out02_r"; break;
					case 491 : efName = "Wm_en_wfsplash_out_l"; break;
					case 492 : efName = "Wm_en_wfsplash_out01_l"; break;
					case 493 : efName = "Wm_en_wfsplash_out02_l"; break;
					case 494 : efName = "Wm_mr_balloonburst"; break;
					case 495 : efName = "Wm_mr_balloonburst_w"; break;
					case 496 : efName = "Wm_mr_balloonburst_h"; break;
					case 497 : efName = "Wm_en_magkillersmoke"; break;
					case 498 : efName = "Wm_en_mgkillershot_r"; break;
					case 499 : efName = "Wm_en_mgkillershot_l"; break;
					case 500 : efName = "Wm_en_killersmoke"; break;
					case 501 : efName = "Wm_en_killershot"; break;
					case 502 : efName = "Wm_en_killervanish"; break;
					case 503 : efName = "Wm_en_kingkiller"; break;
					case 504 : efName = "Wm_en_kingkiller_gr"; break;
					case 505 : efName = "Wm_en_kingkiller_rg"; break;
					case 506 : efName = "Wm_en_kingkiller_sm"; break;
					case 507 : efName = "Wm_en_mgsearchkiller"; break;
					case 508 : efName = "Wm_en_searchkiller"; break;
					case 509 : efName = "Wm_en_pakkunsweat"; break;
					case 510 : efName = "Wm_en_pakkun_ball01"; break;
					case 511 : efName = "Wm_en_pakkun_ball02"; break;
					case 512 : efName = "Wm_en_pakkun_foo"; break;
					case 513 : efName = "Wm_en_igafirehit"; break;
					case 514 : efName = "Wm_en_patametsweat"; break;
					case 515 : efName = "Wm_ob_fireworks_y"; break;
					case 516 : efName = "Wm_ob_fireworks_y01"; break;
					case 517 : efName = "Wm_ob_fireworks_ycld"; break;
					case 518 : efName = "Wm_ob_fireworks_b"; break;
					case 519 : efName = "Wm_ob_fireworks_b01"; break;
					case 520 : efName = "Wm_ob_fireworks_bcld"; break;
					case 521 : efName = "Wm_ob_fireworks_g"; break;
					case 522 : efName = "Wm_ob_fireworks_g01"; break;
					case 523 : efName = "Wm_ob_fireworks_gcld"; break;
					case 524 : efName = "Wm_ob_fireworks_p"; break;
					case 525 : efName = "Wm_ob_fireworks_p01"; break;
					case 526 : efName = "Wm_ob_fireworks_pcld"; break;
					case 527 : efName = "Wm_ob_fireworks_k"; break;
					case 528 : efName = "Wm_ob_fireworks_kgl01"; break;
					case 529 : efName = "Wm_ob_fireworks_kgl02"; break;
					case 530 : efName = "Wm_ob_fireworks_k01"; break;
					case 531 : efName = "Wm_ob_fireworks_kcld1"; break;
					case 532 : efName = "Wm_ob_fireworks_k02"; break;
					case 533 : efName = "Wm_ob_fireworks_kcld2"; break;
					case 534 : efName = "Wm_ob_fireworks_1up"; break;
					case 535 : efName = "Wm_ob_fireworks_1upgl01"; break;
					case 536 : efName = "Wm_ob_fireworks_1upgl02"; break;
					case 537 : efName = "Wm_ob_fireworks_1up01"; break;
					case 538 : efName = "Wm_ob_fireworks_1upcld1"; break;
					case 539 : efName = "Wm_ob_fireworks_1up02"; break;
					case 540 : efName = "Wm_ob_fireworks_1upcld2"; break;
					case 541 : efName = "Wm_ob_fireworks_star"; break;
					case 542 : efName = "Wm_ob_fireworks_stargl01"; break;
					case 543 : efName = "Wm_ob_fireworks_stargl02"; break;
					case 544 : efName = "Wm_ob_fireworks_star01"; break;
					case 545 : efName = "Wm_ob_fireworks_starcld1"; break;
					case 546 : efName = "Wm_ob_fireworks_star02"; break;
					case 547 : efName = "Wm_ob_fireworks_starcld2"; break;
					case 548 : efName = "Wm_ob_switch"; break;
					case 549 : efName = "Wm_ob_switch01"; break;
					case 550 : efName = "Wm_en_sweat"; break;
					case 551 : efName = "Wm_en_choroappear"; break;
					case 552 : efName = "Wm_en_choroescape"; break;
					case 553 : efName = "Wm_en_brakesmoke"; break;
					case 554 : efName = "Wm_ob_redcioinkira"; break;
					case 555 : efName = "Wm_ob_redcioinkira_cd"; break;
					case 556 : efName = "Wm_ob_redcioinitem01"; break;
					case 557 : efName = "Wm_ob_redcioinitem_cd"; break;
					case 558 : efName = "Wm_ob_redcioinitem02"; break;
					case 559 : efName = "Wm_ob_redcioinitem02_b"; break;
					case 560 : efName = "Wm_ob_redcioinitem02_a"; break;
					case 561 : efName = "Wm_ob_itemfall"; break;
					case 562 : efName = "Wm_ob_itemfall_a"; break;
					case 563 : efName = "Wm_ob_itemfall_b"; break;
					case 564 : efName = "Wm_ob_redringkira"; break;
					case 565 : efName = "Wm_ob_redringget"; break;
					case 566 : efName = "Wm_ob_redringget_a"; break;
					case 567 : efName = "Wm_ob_redringget_b"; break;
					case 568 : efName = "Wm_ob_redringget_c"; break;
					case 569 : efName = "Wm_ob_warpcannonkira"; break;
					case 570 : efName = "Wm_ob_witchcraft"; break;
					case 571 : efName = "Wm_ob_witchcraftup"; break;
					case 572 : efName = "Wm_bs_kameckmagic"; break;
					case 573 : efName = "Wm_bs_kameckmagic_e"; break;
					case 574 : efName = "Wm_bs_kameckmagic_f"; break;
					case 575 : efName = "Wm_bs_kameckmagic_a"; break;
					case 576 : efName = "Wm_bs_kameckmagic_b"; break;
					case 577 : efName = "Wm_bs_kameckmagic_c"; break;
					case 578 : efName = "Wm_bs_kameckmagic_d"; break;
					case 579 : efName = "Wm_ob_keyfall"; break;
					case 580 : efName = "Wm_ob_keywait"; break;
					case 581 : efName = "Wm_ob_keywait_c"; break;
					case 582 : efName = "Wm_ob_keywait_a"; break;
					case 583 : efName = "Wm_ob_keywait_b"; break;
					case 584 : efName = "Wm_ob_keyget01"; break;
					case 585 : efName = "Wm_ob_keyget01_d"; break;
					case 586 : efName = "Wm_ob_keyget01_a"; break;
					case 587 : efName = "Wm_ob_keyget01_b"; break;
					case 588 : efName = "Wm_ob_keyget01_c"; break;
					case 589 : efName = "Wm_ob_keyget02"; break;
					case 590 : efName = "Wm_ob_keyget02_kira"; break;
					case 591 : efName = "Wm_ob_keyget02_ring01"; break;
					case 592 : efName = "Wm_ob_keyget02_ring02"; break;
					case 593 : efName = "Wm_ob_keyget02_gl02"; break;
					case 594 : efName = "Wm_ob_keyget02_gl01"; break;
					case 595 : efName = "Wm_ob_keyget02_lighit"; break;
					case 596 : efName = "Wm_ob_keyget02_hit"; break;
					case 597 : efName = "Wm_ob_keyget02_str"; break;
					case 598 : efName = "Wm_ob_stream"; break;
					case 599 : efName = "Wm_seacloudout"; break;
					case 600 : efName = "Wm_shellopen"; break;
					case 601 : efName = "Wm_shellopen_a"; break;
					case 602 : efName = "Wm_shellopen_b"; break;
					case 603 : efName = "Wm_2d_courseclear"; break;
					case 604 : efName = "Wm_2d_courseclear_da"; break;
					case 605 : efName = "Wm_2d_courseclear_kiraL"; break;
					case 606 : efName = "Wm_2d_courseclear_kiraR"; break;
					case 607 : efName = "Wm_2d_courseclear_cld"; break;
					case 608 : efName = "Wm_2d_courseclear_smkL"; break;
					case 609 : efName = "Wm_2d_courseclearsmcld01"; break;
					case 610 : efName = "Wm_2d_courseclear_smkR"; break;
					case 611 : efName = "Wm_2d_courseclearsmcld02"; break;
					case 612 : efName = "Wm_2d_timeup"; break;
					case 613 : efName = "Wm_2d_timeupsmoke"; break;
					case 614 : efName = "Wm_2d_timeupstar"; break;
					case 615 : efName = "Wm_2d_timeupstarcld"; break;
					case 616 : efName = "Wm_2d_gameover"; break;
					case 617 : efName = "Wm_2d_gameover_a"; break;
					case 618 : efName = "Wm_2d_gameover_b"; break;
					case 619 : efName = "Wm_2d_mrstarkira"; break;
					case 620 : efName = "Wm_2d_1up02"; break;
					case 621 : efName = "Wm_2d_1up01"; break;
					case 622 : efName = "Wm_2d_coin100"; break;
					case 623 : efName = "Wm_2d_coin100a"; break;
					case 624 : efName = "Wm_2d_coinlight"; break;
					case 625 : efName = "Wm_2d_continue"; break;
					case 626 : efName = "Wm_2d_stockitem"; break;
					case 627 : efName = "Wm_2d_stockitem_a"; break;
					case 628 : efName = "Wm_2d_stockitem_b"; break;
					case 629 : efName = "Wm_mr_stockitemuse"; break;
					case 630 : efName = "Wm_mr_stockitemuse_a"; break;
					case 631 : efName = "Wm_mr_stockitemuse_b"; break;
					case 632 : efName = "Wm_mr_stockitemuse_c"; break;
					case 633 : efName = "Wm_2d_moviecoinkira"; break;
					case 634 : efName = "Wm_2d_moviecoinvanish"; break;
					case 635 : efName = "Wm_2d_movieopen"; break;
					case 636 : efName = "Wm_2d_movieopen_a"; break;
					case 637 : efName = "Wm_2d_movieopen_b1"; break;
					case 638 : efName = "Wm_2d_movieopen_b2"; break;
					case 639 : efName = "Wm_2d_resultscore"; break;
					case 640 : efName = "Wm_2d_resultrest"; break;
					case 641 : efName = "Wm_2d_resultno1"; break;
					case 642 : efName = "Wm_2d_result"; break;
					case 643 : efName = "Wm_2d_result_a1"; break;
					case 644 : efName = "Wm_2d_result_a2"; break;
					case 645 : efName = "Wm_2d_result_b1"; break;
					case 646 : efName = "Wm_2d_result_b2"; break;
					case 647 : efName = "Wm_2d_starcoinget"; break;
					case 648 : efName = "Wm_2d_starcoinvanish"; break;
					case 649 : efName = "Wm_2d_multiclear"; break;
					case 650 : efName = "Wm_2d_titlestar01"; break;
					case 651 : efName = "Wm_2d_titlestar02"; break;
					case 652 : efName = "Wm_en_fireburner"; break;
					case 653 : efName = "Wm_en_firebrnsignind"; break;
					case 654 : efName = "Wm_en_firebrnsign"; break;
					case 655 : efName = "Wm_en_fireburner3ind"; break;
					case 656 : efName = "Wm_en_fireburner4ind"; break;
					case 657 : efName = "Wm_en_fireburner6ind"; break;
					case 658 : efName = "Wm_mr_palm_s"; break;
					case 659 : efName = "Wm_mr_palm"; break;
					case 660 : efName = "Wm_ob_boat"; break;
					case 661 : efName = "Wm_ob_fallsmoke"; break;
					case 662 : efName = "Wm_ob_fallsmoke_big"; break;
					case 663 : efName = "Wm_ob_fallsmoke_s"; break;
					case 664 : efName = "Wm_bg_volcano"; break;
					case 665 : efName = "Wm_bg_volcano_a"; break;
					case 666 : efName = "Wm_bg_volcano_b"; break;
					case 667 : efName = "Wm_ob_treasurebox"; break;
					case 668 : efName = "Wm_ob_treasureboxwait"; break;
					case 669 : efName = "Wm_ob_treasureboxwait_a"; break;
					case 670 : efName = "Wm_ob_treasureboxwait_b"; break;
					case 671 : efName = "Wm_ob_treasureboxtail"; break;
					case 672 : efName = "Wm_ob_itemteil"; break;
					case 673 : efName = "Wm_ob_icicle"; break;
					case 674 : efName = "Wm_mr_brosquake"; break;
					case 675 : efName = "Wm_en_atitismoke"; break;
					case 676 : efName = "Wm_en_waterwave_in_a"; break;
					case 677 : efName = "Wm_en_waterwave_in_b"; break;
					case 678 : efName = "Wm_en_watersplash_cld"; break;
					case 679 : efName = "Wm_en_poisonwave_a"; break;
					case 680 : efName = "Wm_en_poisonwave_b"; break;
					case 681 : efName = "Wm_en_magmawave_a"; break;
					case 682 : efName = "Wm_en_magmawave_b"; break;
					case 683 : efName = "Wm_en_vshit_hit"; break;
					case 684 : efName = "Wm_en_vshit_glow"; break;
					case 685 : efName = "Wm_en_vshit_ring"; break;
					case 686 : efName = "Wm_en_vshit_star"; break;
					case 687 : efName = "Wm_en_hit_ring"; break;
					case 688 : efName = "Wm_mr_misshit_ring"; break;
					case 689 : efName = "Wm_ob_envsunlight_b"; break;
					case 690 : efName = "Wm_ob_envsunlight_a"; break;
					case 691 : efName = "Wm_mr_fireball_a"; break;
					case 692 : efName = "Wm_mr_fireball_b"; break;
					case 693 : efName = "Wm_mr_iceball_a"; break;
					case 694 : efName = "Wm_mr_iceball_b"; break;
					case 695 : efName = "Wm_ob_icemisshit_smk"; break;
					case 696 : efName = "Wm_mr_fireball_hit01"; break;
					case 697 : efName = "Wm_en_bubble_a"; break;
					case 698 : efName = "Wm_en_bubble_b"; break;
					case 699 : efName = "Wm_mr_cliffcatch_cd"; break;
					case 700 : efName = "Wm_mr_wallkick_up_r"; break;
					case 701 : efName = "Wm_mr_wallkick_dn_r"; break;
					case 702 : efName = "Wm_mr_wallkick_b_r"; break;
					case 703 : efName = "Wm_mr_wallkick_c_r"; break;
					case 704 : efName = "Wm_mr_wallkick_cld_r"; break;
					case 705 : efName = "Wm_mr_wallkick_cld_r"; break;
					case 706 : efName = "Wm_mr_wallkick_up_l"; break;
					case 707 : efName = "Wm_mr_wallkick_dn_l"; break;
					case 708 : efName = "Wm_mr_wallkick_b_l"; break;
					case 709 : efName = "Wm_mr_wallkick_c_l"; break;
					case 710 : efName = "Wm_mr_wallkick_cld_l"; break;
					case 711 : efName = "Wm_mr_wallkick_cld_l"; break;
					case 712 : efName = "Wm_mr_wallkick_up_s_r"; break;
					case 713 : efName = "Wm_mr_wallkick_dn_s_r"; break;
					case 714 : efName = "Wm_mr_wallkick_b_s_r"; break;
					case 715 : efName = "Wm_mr_wallkick_c_s_r"; break;
					case 716 : efName = "Wm_mr_wallkick_cld_s_r"; break;
					case 717 : efName = "Wm_mr_wallkick_cld_s_r"; break;
					case 718 : efName = "Wm_mr_wallkick_up_s_l"; break;
					case 719 : efName = "Wm_mr_wallkick_dn_s_l"; break;
					case 720 : efName = "Wm_mr_wallkick_b_s_l"; break;
					case 721 : efName = "Wm_mr_wallkick_c_s_l"; break;
					case 722 : efName = "Wm_mr_wallkick_cld_s_l"; break;
					case 723 : efName = "Wm_mr_wallkick_cld_s_l"; break;
					case 724 : efName = "Wm_mr_wallkick_up_ss_r"; break;
					case 725 : efName = "Wm_mr_wallkick_dn_ss_r"; break;
					case 726 : efName = "Wm_mr_wallkick_b_ss_r"; break;
					case 727 : efName = "Wm_mr_wallkick_c_ss_r"; break;
					case 728 : efName = "Wm_mr_wallkick_cld_ss_r"; break;
					case 729 : efName = "Wm_mr_wallkick_cld_ss_r"; break;
					case 730 : efName = "Wm_mr_wallkick_up_ss_l"; break;
					case 731 : efName = "Wm_mr_wallkick_dn_ss_l"; break;
					case 732 : efName = "Wm_mr_wallkick_b_ss_l"; break;
					case 733 : efName = "Wm_mr_wallkick_c_ss_l"; break;
					case 734 : efName = "Wm_mr_wallkick_cld_ss_l"; break;
					case 735 : efName = "Wm_mr_wallkick_cld_ss_l"; break;
					case 736 : efName = "Wm_mr_wallslip_cld"; break;
					case 737 : efName = "Wm_mr_wallslip_cld"; break;
					case 738 : efName = "Wm_mr_wallslip_cld_s"; break;
					case 739 : efName = "Wm_mr_wallslip_cld_s"; break;
					case 740 : efName = "Wm_mr_wallslip_cld_ss"; break;
					case 741 : efName = "Wm_mr_wallslip_cld_ss"; break;
					case 742 : efName = "Wm_mr_hardhit_spak"; break;
					case 743 : efName = "Wm_mr_hardhit_glow"; break;
					case 744 : efName = "Wm_mr_hardhit_grain"; break;
					case 745 : efName = "Wm_mr_kick_glow"; break;
					case 746 : efName = "Wm_mr_kick_grain"; break;
					case 747 : efName = "Wm_mr_softhit_spak"; break;
					case 748 : efName = "Wm_mr_softhit_glow"; break;
					case 749 : efName = "Wm_mr_wirehit_glow"; break;
					case 750 : efName = "Wm_mr_wirehit_hit"; break;
					case 751 : efName = "Wm_mr_wirehit_line"; break;
					case 752 : efName = "Wm_mr_wirehit_star"; break;
					case 753 : efName = "Wm_ob_greencoinkira_c"; break;
					case 754 : efName = "Wm_ob_greencoinkira_b"; break;
					case 755 : efName = "Wm_ob_greencoinkira_a"; break;
					case 756 : efName = "Wm_ob_starcoinget_str"; break;
					case 757 : efName = "Wm_ob_starcoinget_gl"; break;
					case 758 : efName = "Wm_ob_starcoinget_hit"; break;
					case 759 : efName = "Wm_ob_starcoinget_ring"; break;
					case 760 : efName = "Wm_ob_starcoinget_lighit"; break;
					case 761 : efName = "Wm_mr_electricshock_glw"; break;
					case 762 : efName = "Wm_mr_electricshock_biri01"; break;
					case 763 : efName = "Wm_mr_electricshock_biri02"; break;
					case 764 : efName = "Wm_mr_electricshock_kira"; break;
					case 765 : efName = "Wm_mr_electricshock_glw_s"; break;
					case 766 : efName = "Wm_mr_electricshock_biri01_s"; break;
					case 767 : efName = "Wm_mr_electricshock_biri02_s"; break;
					case 768 : efName = "Wm_mr_electricshock_kira_s"; break;
					case 769 : efName = "Wm_en_birikyu_glw"; break;
					case 770 : efName = "Wm_en_birikyu_biri"; break;
					case 771 : efName = "Wm_en_birikyu_kira"; break;
					case 772 : efName = "Wm_mr_1upkira_spin"; break;
					case 773 : efName = "Wm_mr_1upkira_01"; break;
					case 774 : efName = "Wm_mr_1upkira_02"; break;
					case 775 : efName = "Wm_mr_1upkira_spin_s"; break;
					case 776 : efName = "Wm_mr_1upkira_01_s"; break;
					case 777 : efName = "Wm_mr_1upkira_02_s"; break;
					case 778 : efName = "Wm_mr_1upkira_spin_ss"; break;
					case 779 : efName = "Wm_mr_1upkira_01_ss"; break;
					case 780 : efName = "Wm_en_hanapetal_a"; break;
					case 781 : efName = "Wm_en_hanapetal_b"; break;
					case 782 : efName = "Wm_en_hanasnort_r"; break;
					case 783 : efName = "Wm_en_hanasnort_l"; break;
					case 784 : efName = "Wm_en_hanasnort_cld"; break;
					case 785 : efName = "Wm_en_hanasnort_cld"; break;
					case 786 : efName = "Wm_ob_flagget_kira"; break;
					case 787 : efName = "Wm_ob_flagget_light"; break;
					case 788 : efName = "Wm_ob_flaggetkira_cld"; break;
					case 789 : efName = "Wm_ob_icebreakwt"; break;
					case 790 : efName = "Wm_ob_icebreaksmk"; break;
					case 791 : efName = "Wm_ob_icewaitwat"; break;
					case 792 : efName = "Wm_ob_iceattackkira"; break;
					case 793 : efName = "Wm_ob_iceattackline"; break;
					case 794 : efName = "Wm_ob_iceattacksmk"; break;
					case 795 : efName = "Wm_ob_icehithit"; break;
					case 796 : efName = "Wm_ob_icehitwat"; break;
					case 797 : efName = "Wm_ob_icehitsmk"; break;
					case 798 : efName = "Wm_ob_waterbreak_c"; break;
					case 799 : efName = "Wm_ob_waterbreak_a"; break;
					case 800 : efName = "Wm_ob_waterbreak_b"; break;
					case 801 : efName = "Wm_mr_vshipattack_line"; break;
					case 802 : efName = "Wm_mr_vshipattack_hosi"; break;
					case 803 : efName = "Wm_mr_vshipattack_gl"; break;
					case 804 : efName = "Wm_mr_vshipattack_ud"; break;
					case 805 : efName = "Wm_mr_vshipattack_ind_c"; break;
					case 806 : efName = "Wm_mr_vshipattack_ind_a"; break;
					case 807 : efName = "Wm_mr_vshipattack_ind_b"; break;
					case 808 : efName = "Wm_mr_spindepart_b"; break;
					case 809 : efName = "Wm_mr_spindepart_a"; break;
					case 810 : efName = "Wm_mr_spindown_a"; break;
					case 811 : efName = "Wm_mr_spindown_b"; break;
					case 812 : efName = "Wm_mr_starkira_a"; break;
					case 813 : efName = "Wm_mr_starkira_b"; break;
					case 814 : efName = "Wm_mr_starkira_a_s"; break;
					case 815 : efName = "Wm_mr_starkira_b_s"; break;
					case 816 : efName = "Wm_ob_itemget_hit"; break;
					case 817 : efName = "Wm_ob_itemget_ring"; break;
					case 818 : efName = "Wm_ob_itemget_hitlighit"; break;
					case 819 : efName = "Wm_ob_itemappear_r"; break;
					case 820 : efName = "Wm_ob_itemappear_gl"; break;
					case 821 : efName = "Wm_ob_itemappear_r_ss"; break;
					case 822 : efName = "Wm_ob_itemappear_gl_ss"; break;
					case 823 : efName = "Wm_ob_startail_star"; break;
					case 824 : efName = "Wm_ob_startail_kira"; break;
					case 825 : efName = "Wm_ob_powdown_ind_a"; break;
					case 826 : efName = "Wm_ob_powdown_ind_c"; break;
					case 827 : efName = "Wm_ob_powdown_ind_b"; break;
					case 828 : efName = "Wm_en_spindamage_rg"; break;
					case 829 : efName = "Wm_en_spindamage_star"; break;
					case 830 : efName = "Wm_en_spindamage_big_rg"; break;
					case 831 : efName = "Wm_en_spindamage_big_st"; break;
					case 832 : efName = "Wm_en_sanbohit_hit"; break;
					case 833 : efName = "Wm_en_sanbohit_ring"; break;
					case 834 : efName = "Wm_en_sanbohit_smk"; break;
					case 835 : efName = "Wm_en_keronpafire_ca"; break;
					case 836 : efName = "Wm_en_keronpafire_f"; break;
					case 837 : efName = "Wm_en_explosion_ln"; break;
					case 838 : efName = "Wm_en_explosion_gl01"; break;
					case 839 : efName = "Wm_en_explosion_hd"; break;
					case 840 : efName = "Wm_en_explosion_un"; break;
					case 841 : efName = "Wm_en_explosion_gl02"; break;
					case 842 : efName = "Wm_en_explosion_smk"; break;
					case 843 : efName = "Wm_en_bomignition_ln"; break;
					case 844 : efName = "Wm_en_bomignition_gl01"; break;
					case 845 : efName = "Wm_en_bomignition_pati"; break;
					case 846 : efName = "Wm_mr_sanddive_sd"; break;
					case 847 : efName = "Wm_mr_sanddive_in"; break;
					case 848 : efName = "Wm_mr_sanddive_out"; break;
					case 849 : efName = "Wm_mr_sanddive_smk"; break;
					case 850 : efName = "Wm_mr_sanddive_sd_m"; break;
					case 851 : efName = "Wm_mr_sanddive_in_m"; break;
					case 852 : efName = "Wm_mr_sanddive_out_m"; break;
					case 853 : efName = "Wm_mr_sanddive_smk_m"; break;
					case 854 : efName = "Wm_mr_sanddive_sb_s"; break;
					case 855 : efName = "Wm_mr_sanddive_smk_s"; break;
					case 856 : efName = "Wm_en_kuribobigsplit_sk"; break;
					case 857 : efName = "Wm_en_kuribobigsplit_ht"; break;
					case 858 : efName = "Wm_en_kuribobigsplit_gr02"; break;
					case 859 : efName = "Wm_en_kuribobigsplit_gr01"; break;
					case 860 : efName = "Wm_en_kuribobigsplit_rg"; break;
					case 861 : efName = "Wm_en_kuribosplit_sk"; break;
					case 862 : efName = "Wm_en_kuribosplit_gl01"; break;
					case 863 : efName = "Wm_en_kuribosplit_gl02"; break;
					case 864 : efName = "Wm_en_obakedoor_sm"; break;
					case 865 : efName = "Wm_en_obakedoor_ic"; break;
					case 866 : efName = "Wm_mr_yoshistep_b"; break;
					case 867 : efName = "Wm_mr_yoshistep_a"; break;
					case 868 : efName = "Wm_mr_yoshistep_a_cld"; break;
					case 869 : efName = "Wm_mr_fruitget_w"; break;
					case 870 : efName = "Wm_mr_fruitget_h"; break;
					case 871 : efName = "Wm_mr_yoshifire_a"; break;
					case 872 : efName = "Wm_mr_yoshifire_b"; break;
					case 873 : efName = "Wm_mr_yoshiiceball_b"; break;
					case 874 : efName = "Wm_mr_yoshiiceball_a"; break;
					case 875 : efName = "Wm_mr_yoshifirehit01"; break;
					case 876 : efName = "Wm_mr_yoshiicehit_b"; break;
					case 877 : efName = "Wm_mr_yoshiicehit_a"; break;
					case 878 : efName = "Wm_mr_ystonguehit_a"; break;
					case 879 : efName = "Wm_en_firebros_fire_a"; break;
					case 880 : efName = "Wm_en_firebros_fire_b"; break;
					case 881 : efName = "Wm_mr_magmawave_a"; break;
					case 882 : efName = "Wm_mr_magmawave_b"; break;
					case 883 : efName = "Wm_mr_poisonwave_a"; break;
					case 884 : efName = "Wm_mr_poisonwave_b"; break;
					case 885 : efName = "Wm_mr_waterwave_in_a"; break;
					case 886 : efName = "Wm_mr_waterwave_in_b"; break;
					case 887 : efName = "Wm_mr_waterwave_in_c"; break;
					case 888 : efName = "Wm_mr_waterwave_in_d"; break;
					case 889 : efName = "Wm_mr_waterwave_out_a"; break;
					case 890 : efName = "Wm_mr_waterwave_out_b"; break;
					case 891 : efName = "Wm_mr_waterwave_out_c"; break;
					case 892 : efName = "Wm_mr_waterwave_in_a_ss"; break;
					case 893 : efName = "Wm_mr_waterwave_in_b_ss"; break;
					case 894 : efName = "Wm_mr_waterwave_out_a_ss"; break;
					case 895 : efName = "Wm_mr_waterwave_out_b_ss"; break;
					case 896 : efName = "Wm_mr_wfloatsplash_a"; break;
					case 897 : efName = "Wm_mr_wfloatsplash_b"; break;
					case 898 : efName = "Wm_en_wfsplash_in01_r"; break;
					case 899 : efName = "Wm_en_wfsplash_in02_r"; break;
					case 900 : efName = "Wm_en_wfsplash_in01_l"; break;
					case 901 : efName = "Wm_en_wfsplash_in02_l"; break;
					case 902 : efName = "Wm_en_wfsplash_out01_r"; break;
					case 903 : efName = "Wm_en_wfsplash_out02_r"; break;
					case 904 : efName = "Wm_en_wfsplash_out01_l"; break;
					case 905 : efName = "Wm_en_wfsplash_out02_l"; break;
					case 906 : efName = "Wm_mr_balloonburst_w"; break;
					case 907 : efName = "Wm_mr_balloonburst_h"; break;
					case 908 : efName = "Wm_en_kingkiller_gr"; break;
					case 909 : efName = "Wm_en_kingkiller_rg"; break;
					case 910 : efName = "Wm_en_kingkiller_sm"; break;
					case 911 : efName = "Wm_ob_fireworks_y01"; break;
					case 912 : efName = "Wm_ob_fireworks_ycld"; break;
					case 913 : efName = "Wm_ob_fireworks_b01"; break;
					case 914 : efName = "Wm_ob_fireworks_bcld"; break;
					case 915 : efName = "Wm_ob_fireworks_g01"; break;
					case 916 : efName = "Wm_ob_fireworks_gcld"; break;
					case 917 : efName = "Wm_ob_fireworks_p01"; break;
					case 918 : efName = "Wm_ob_fireworks_pcld"; break;
					case 919 : efName = "Wm_ob_fireworks_k02"; break;
					case 920 : efName = "Wm_ob_fireworks_kgl01"; break;
					case 921 : efName = "Wm_ob_fireworks_kgl02"; break;
					case 922 : efName = "Wm_ob_fireworks_k01"; break;
					case 923 : efName = "Wm_ob_fireworks_kcld1"; break;
					case 924 : efName = "Wm_ob_fireworks_kcld2"; break;
					case 925 : efName = "Wm_ob_fireworks_1up02"; break;
					case 926 : efName = "Wm_ob_fireworks_1upgl01"; break;
					case 927 : efName = "Wm_ob_fireworks_1upgl02"; break;
					case 928 : efName = "Wm_ob_fireworks_1up01"; break;
					case 929 : efName = "Wm_ob_fireworks_1upcld1"; break;
					case 930 : efName = "Wm_ob_fireworks_1upcld2"; break;
					case 931 : efName = "Wm_ob_fireworks_star02"; break;
					case 932 : efName = "Wm_ob_fireworks_stargl01"; break;
					case 933 : efName = "Wm_ob_fireworks_stargl02"; break;
					case 934 : efName = "Wm_ob_fireworks_star01"; break;
					case 935 : efName = "Wm_ob_fireworks_starcld1"; break;
					case 936 : efName = "Wm_ob_fireworks_starcld2"; break;
					case 937 : efName = "Wm_ob_switch01"; break;
					case 938 : efName = "Wm_ob_redcioinkira_cd"; break;
					case 939 : efName = "Wm_ob_redcioinitem_cd"; break;
					case 940 : efName = "Wm_ob_redcioinitem02_b"; break;
					case 941 : efName = "Wm_ob_redcioinitem02_a"; break;
					case 942 : efName = "Wm_ob_itemfall_a"; break;
					case 943 : efName = "Wm_ob_itemfall_b"; break;
					case 944 : efName = "Wm_ob_redringget_a"; break;
					case 945 : efName = "Wm_ob_redringget_b"; break;
					case 946 : efName = "Wm_ob_redringget_c"; break;
					case 947 : efName = "Wm_bs_kameckmagic_f"; break;
					case 948 : efName = "Wm_bs_kameckmagic_e"; break;
					case 949 : efName = "Wm_bs_kameckmagic_a"; break;
					case 950 : efName = "Wm_bs_kameckmagic_c"; break;
					case 951 : efName = "Wm_bs_kameckmagic_d"; break;
					case 952 : efName = "Wm_bs_kameckmagic_b"; break;
					case 953 : efName = "Wm_ob_keywait_a"; break;
					case 954 : efName = "Wm_ob_keywait_b"; break;
					case 955 : efName = "Wm_ob_keywait_c"; break;
					case 956 : efName = "Wm_ob_keyget01_d"; break;
					case 957 : efName = "Wm_ob_keyget01_a"; break;
					case 958 : efName = "Wm_ob_keyget01_b"; break;
					case 959 : efName = "Wm_ob_keyget01_c"; break;
					case 960 : efName = "Wm_ob_keyget02_ring02"; break;
					case 961 : efName = "Wm_ob_keyget02_kira"; break;
					case 962 : efName = "Wm_ob_keyget02_gl02"; break;
					case 963 : efName = "Wm_ob_keyget02_str"; break;
					case 964 : efName = "Wm_ob_keyget02_gl01"; break;
					case 965 : efName = "Wm_ob_keyget02_hit"; break;
					case 966 : efName = "Wm_ob_keyget02_ring01"; break;
					case 967 : efName = "Wm_ob_keyget02_lighit"; break;
					case 968 : efName = "Wm_shellopen_a"; break;
					case 969 : efName = "Wm_shellopen_b"; break;
					case 970 : efName = "Wm_2d_courseclear_da"; break;
					case 971 : efName = "Wm_2d_courseclear_kiraL"; break;
					case 972 : efName = "Wm_2d_courseclear_kiraR"; break;
					case 973 : efName = "Wm_2d_courseclear_smkL"; break;
					case 974 : efName = "Wm_2d_courseclear_smkR"; break;
					case 975 : efName = "Wm_2d_courseclear_cld"; break;
					case 976 : efName = "Wm_2d_courseclear_cld"; break;
					case 977 : efName = "Wm_2d_courseclearsmcld01"; break;
					case 978 : efName = "Wm_2d_courseclearsmcld02"; break;
					case 979 : efName = "Wm_2d_timeupsmoke"; break;
					case 980 : efName = "Wm_2d_timeupstar"; break;
					case 981 : efName = "Wm_2d_timeupstarcld"; break;
					case 982 : efName = "Wm_2d_gameover_a"; break;
					case 983 : efName = "Wm_2d_gameover_b"; break;
					case 984 : efName = "Wm_2d_coin100a"; break;
					case 985 : efName = "Wm_2d_coinlight"; break;
					case 986 : efName = "Wm_2d_stockitem_a"; break;
					case 987 : efName = "Wm_2d_stockitem_b"; break;
					case 988 : efName = "Wm_mr_stockitemuse_a"; break;
					case 989 : efName = "Wm_mr_stockitemuse_b"; break;
					case 990 : efName = "Wm_mr_stockitemuse_c"; break;
					case 991 : efName = "Wm_2d_movieopen_a"; break;
					case 992 : efName = "Wm_2d_movieopen_b2"; break;
					case 993 : efName = "Wm_2d_movieopen_b1"; break;
					case 994 : efName = "Wm_2d_result_a2"; break;
					case 995 : efName = "Wm_2d_result_b2"; break;
					case 996 : efName = "Wm_2d_result_a1"; break;
					case 997 : efName = "Wm_2d_result_b1"; break;
					case 998 : efName = "Wm_bg_volcano_a"; break;
					case 999 : efName = "Wm_bg_volcano_b"; break;
					case 1000: efName = "Wm_ob_treasureboxwait_a"; break;
					case 1001: efName = "Wm_ob_treasureboxwait_b"; break;
					case 1002: efName = "Wm_jr_electricstart"; break;
					case 1003: efName = "Wm_jr_electricglow"; break;
					case 1004: efName = "Wm_jr_electricspark"; break;
					case 1005: efName = "Wm_jr_electricline"; break;
				}

				if (efName != 0)
					SpawnEffect(efName, 0, &this->pos, &(S16Vec){0,0,0}, &(Vec){this->scale, this->scale, this->scale});
			}
	
			this->timer = 0;
			if (this->delay == 0) { this->delay = -1; }
		}
		
		this->timer += 1;
	}
	return true;
}


//
// processed\../src/shyguy.cpp
//

#include <common.h>
#include <game.h>
#include <g3dhax.h>
#include <sfx.h>


const char* SGarcNameList [] = {
	"shyguy",
	"iron_ball",
	NULL	
};

// Shy Guy Settings
// 
// Nybble 5: Shy Guy Types
//		0 - Walker 	
//		1 - Pacing Walker
//		2 - Sleeper
//		3 - Jumper
// 		4 - Judo Master 
// 		5 - Spike Thrower
// 		6 - Ballooneer Horizontal
// 		7 - Ballooneer Vertical
// 		8 - Ballooneer Circular 
//		9 - Walking Giant
// 		10 - Pacing Giant
//
// Nybble 9: Distance Moved
//		# - Distance for Pacing Walker, Pacing Giants, and Ballooneers 
//
// If I add items in the balloons....
// I_kinoko, I_fireflower, I_propeller_model, I_iceflower, I_star, I_penguin - model names
// anmChr - wait2

void shyCollisionCallback(ActivePhysics *apThis, ActivePhysics *apOther);	
void ChucklesAndKnuckles(ActivePhysics *apThis, ActivePhysics *apOther);
void balloonSmack(ActivePhysics *apThis, ActivePhysics *apOther);

class daShyGuy : public dEn_c {
	int onCreate();
	int onDelete();
	int onExecute();
	int onDraw();

	mHeapAllocator_c allocator;
	nw4r::g3d::ResFile resFile;
	nw4r::g3d::ResFile anmFile;
	nw4r::g3d::ResFile balloonFile;
	// nw4r::g3d::ResFile carryFile;

	m3d::mdl_c bodyModel;
	m3d::mdl_c balloonModel;
	m3d::mdl_c balloonModelB;
	// m3d::mdl_c carryModel;1

	m3d::anmChr_c chrAnimation;
	// m3d::anmChr_c carryAnm;

	mEf::es2 effect;

	int timer;
	int jumpCounter;
	int baln;
	float dying;
	float Baseline;
	char damage;
	char isDown;
	char renderBalloon;
	Vec initialPos;
	int distance;
	float XSpeed;
	u32 cmgr_returnValue;
	bool isBouncing;
	float balloonSize;
	char backFire;
	char spikeTurn;
	int directionStore;
	dStageActor_c *spikeA;
	dStageActor_c *spikeB;
	bool stillFalling;

	StandOnTopCollider giantRider;
	ActivePhysics Chuckles;
	ActivePhysics Knuckles;
	ActivePhysics balloonPhysics;

	static daShyGuy *build();

	void bindAnimChr_and_setUpdateRate(const char* name, int unk, float unk2, float rate);
	void updateModelMatrices();
	bool calculateTileCollisions();

	void spriteCollision(ActivePhysics *apThis, ActivePhysics *apOther);
	void playerCollision(ActivePhysics *apThis, ActivePhysics *apOther);
	void yoshiCollision(ActivePhysics *apThis, ActivePhysics *apOther);

	bool collisionCat3_StarPower(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat14_YoshiFire(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCatD_Drill(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat7_GroundPound(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat7_GroundPoundYoshi(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat9_RollingObject(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat1_Fireball_E_Explosion(ActivePhysics *apThis, ActivePhysics *apOther);
	// bool collisionCat2_IceBall_15_YoshiIce(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat13_Hammer(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCatA_PenguinMario(ActivePhysics *apThis, ActivePhysics *apOther);

	bool collisionCat5_Mario(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat11_PipeCannon(ActivePhysics *apThis, ActivePhysics *apOther);

	void _vf148();
	void _vf14C();
	bool CreateIceActors();

	bool willWalkOntoSuitableGround();

	USING_STATES(daShyGuy);
	DECLARE_STATE(Walk);
	DECLARE_STATE(Turn);
	DECLARE_STATE(RealWalk);
	DECLARE_STATE(RealTurn);
	DECLARE_STATE(Jump);
	DECLARE_STATE(Sleep);
	DECLARE_STATE(Balloon_H);
	DECLARE_STATE(Balloon_V);
	DECLARE_STATE(Balloon_C);
	DECLARE_STATE(Judo);
	DECLARE_STATE(Spike);

	DECLARE_STATE(GoDizzy);
	DECLARE_STATE(BalloonDrop);
	DECLARE_STATE(FireKnockBack);
	DECLARE_STATE(FlameHit);
	DECLARE_STATE(Recover);

	DECLARE_STATE(Die);

	public: void popBalloon();
	int type;
};

daShyGuy *daShyGuy::build() {
	void *buffer = AllocFromGameHeap1(sizeof(daShyGuy));
	return new(buffer) daShyGuy;
}

///////////////////////
// Externs and States
///////////////////////
	extern "C" bool SpawnEffect(const char*, int, Vec*, S16Vec*, Vec*);

	//FIXME make this dEn_c->used...
	extern "C" char usedForDeterminingStatePress_or_playerCollision(dEn_c* t, ActivePhysics *apThis, ActivePhysics *apOther, int unk1);
	extern "C" int SomeStrangeModification(dStageActor_c* actor);
	extern "C" void DoStuffAndMarkDead(dStageActor_c *actor, Vec vector, float unk);
	extern "C" int SmoothRotation(short* rot, u16 amt, int unk2);
	// extern "C" void addToList(StandOnTopCollider *self);

	extern "C" bool HandlesEdgeTurns(dEn_c* actor);


	CREATE_STATE(daShyGuy, Walk);
	CREATE_STATE(daShyGuy, Turn);
	CREATE_STATE(daShyGuy, RealWalk);
	CREATE_STATE(daShyGuy, RealTurn);
	CREATE_STATE(daShyGuy, Jump);
	CREATE_STATE(daShyGuy, Sleep);
	CREATE_STATE(daShyGuy, Balloon_H);
	CREATE_STATE(daShyGuy, Balloon_V);
	CREATE_STATE(daShyGuy, Balloon_C);
	CREATE_STATE(daShyGuy, Judo);
	CREATE_STATE(daShyGuy, Spike);

	CREATE_STATE(daShyGuy, GoDizzy);
	CREATE_STATE(daShyGuy, BalloonDrop);
	CREATE_STATE(daShyGuy, FireKnockBack);
	CREATE_STATE(daShyGuy, FlameHit);
	CREATE_STATE(daShyGuy, Recover);

	CREATE_STATE(daShyGuy, Die);

////////////////////////
// Collision Functions
////////////////////////

	bool actorCanPopBalloon(dStageActor_c *ac) {
		int n = ac->name;
		return n == PLAYER || n == YOSHI ||
			n == PL_FIREBALL || n == ICEBALL ||
			n == YOSHI_FIRE || n == HAMMER;
	}
	// Collision callback to help shy guy not die at inappropriate times and ruin the dinner

	void shyCollisionCallback(ActivePhysics *apThis, ActivePhysics *apOther) {
		int t = ((daShyGuy*)apThis->owner)->type;
		if (t == 6 || t == 7 || t == 8) {
			// Should I do something about ice blocks here?
			if (actorCanPopBalloon(apOther->owner))
				((daShyGuy*)apThis->owner)->popBalloon();
		}

		if ((apOther->owner->name == 89) && (t == 5)) { return; }
			
		dEn_c::collisionCallback(apThis, apOther); 
	}

	void ChucklesAndKnuckles(ActivePhysics *apThis, ActivePhysics *apOther) {
		if (apOther->owner->name != PLAYER) { return; }
		((dEn_c*)apThis->owner)->_vf220(apOther->owner);
	}

	void balloonSmack(ActivePhysics *apThis, ActivePhysics *apOther) {
		if (((daShyGuy*)apThis->owner)->frzMgr._mstate == 0) {
			if (actorCanPopBalloon(apOther->owner))
				((daShyGuy*)apThis->owner)->popBalloon();
		}
	}

	void daShyGuy::spriteCollision(ActivePhysics *apThis, ActivePhysics *apOther) {
		u16 name = ((dEn_c*)apOther->owner)->name;

		if (name == EN_COIN || name == EN_EATCOIN || name == AC_BLOCK_COIN || name == EN_COIN_JUGEM || name == EN_COIN_ANGLE
			|| name == EN_COIN_JUMP || name == EN_COIN_FLOOR || name == EN_COIN_VOLT || name == EN_COIN_WIND 
			|| name == EN_BLUE_COIN || name == EN_COIN_WATER || name == EN_REDCOIN || name == EN_GREENCOIN
			|| name == EN_JUMPDAI || name == EN_ITEM) 
			{ return; }

		if (acState.getCurrentState() == &StateID_RealWalk) {

			pos.x = ((pos.x - ((dEn_c*)apOther->owner)->pos.x) > 0) ? pos.x + 1.5 : pos.x - 1.5;
			// pos.x = direction ? pos.x + 1.5 : pos.x - 1.5;
			doStateChange(&StateID_RealTurn); }

		if (acState.getCurrentState() == &StateID_FireKnockBack) {
			float distance = pos.x - ((dEn_c*)apOther->owner)->pos.x;
			pos.x = pos.x + (distance / 4.0);
		}

		dEn_c::spriteCollision(apThis, apOther); 
	}

	void daShyGuy::popBalloon() {
		doStateChange(&StateID_BalloonDrop);
	}

	void daShyGuy::playerCollision(ActivePhysics *apThis, ActivePhysics *apOther) {
		dStateBase_c *stateVar;
		dStateBase_c *deathState;
		
		char hitType;
		if (this->type < 6) {  // Regular Shy Guys
			stateVar = &StateID_GoDizzy;
			deathState = &StateID_Die;
		}
		else { // Ballooneers
			stateVar = &StateID_BalloonDrop;
			deathState = &StateID_Die;
		}


		if (this->isDown == 0) { 
			hitType = usedForDeterminingStatePress_or_playerCollision(this, apThis, apOther, 2);
		}
		else { // Shy Guy is in downed mode
			hitType = usedForDeterminingStatePress_or_playerCollision(this, apThis, apOther, 0);
		}

		if(hitType == 1) {	// regular jump
			apOther->someFlagByte |= 2;
			if (this->isDown == 0) { 
				this->playEnemyDownSound1();
				if (damage >= 1) {
					doStateChange(deathState); }
				else {
					doStateChange(stateVar); }
				damage++;
			}
			else { // Shy Guy is in downed mode - kill it with fire
				this->playEnemyDownSound1();
				doStateChange(deathState);
			}				
		} 
		else if(hitType == 3) {	// spinning jump or whatever?
			apOther->someFlagByte |= 2;
			if (this->isDown == 0) { 
				this->playEnemyDownSound1();
				if (damage >= 1) {
					doStateChange(deathState); }
				else {
					doStateChange(stateVar); }
				damage++;
			}
			else { // Shy Guy is in downed mode - kill it with fire
				this->playEnemyDownSound1();
				doStateChange(deathState);
			}				
		} 
		else if(hitType == 0) {
			this->dEn_c::playerCollision(apThis, apOther);
			this->_vf220(apOther->owner);
		} 
		// else if(hitType == 2) { \\ Minimario? } 
	}

	void daShyGuy::yoshiCollision(ActivePhysics *apThis, ActivePhysics *apOther) {
		this->playerCollision(apThis, apOther);
	}
	bool daShyGuy::collisionCatD_Drill(ActivePhysics *apThis, ActivePhysics *apOther) { 
		PlaySound(this, SE_EMY_DOWN); 
		SpawnEffect("Wm_mr_hardhit", 0, &pos, &(S16Vec){0,0,0}, &(Vec){1.0, 1.0, 1.0});
		//addScoreWhenHit accepts a player parameter.
		//DON'T DO THIS:
		// this->addScoreWhenHit(this);
		doStateChange(&StateID_Die); 
		return true;
	}
	bool daShyGuy::collisionCat7_GroundPound(ActivePhysics *apThis, ActivePhysics *apOther) {
		return this->collisionCatD_Drill(apThis, apOther);
	}
	bool daShyGuy::collisionCat7_GroundPoundYoshi(ActivePhysics *apThis, ActivePhysics *apOther) {
		return this->collisionCatD_Drill(apThis, apOther);
	}
	bool daShyGuy::collisionCat9_RollingObject(ActivePhysics *apThis, ActivePhysics *apOther) {
		return this->collisionCatD_Drill(apThis, apOther);
	}
	bool daShyGuy::collisionCatA_PenguinMario(ActivePhysics *apThis, ActivePhysics *apOther){
		return this->collisionCatD_Drill(apThis, apOther);
	}
	bool daShyGuy::collisionCat5_Mario(ActivePhysics *apThis, ActivePhysics *apOther){
		return this->collisionCatD_Drill(apThis, apOther);
	}
	bool daShyGuy::collisionCat11_PipeCannon(ActivePhysics *apThis, ActivePhysics *apOther){
		return this->collisionCatD_Drill(apThis, apOther);
	}
	bool daShyGuy::collisionCat13_Hammer(ActivePhysics *apThis, ActivePhysics *apOther) {
		StageE4::instance->spawnCoinJump(pos, 0, 2, 0);
		return this->collisionCatD_Drill(apThis, apOther);
	}

	bool daShyGuy::collisionCat3_StarPower(ActivePhysics *apThis, ActivePhysics *apOther){
		bool wut = dEn_c::collisionCat3_StarPower(apThis, apOther);
		doStateChange(&StateID_Die);
		return wut;
	}

	bool daShyGuy::collisionCat14_YoshiFire(ActivePhysics *apThis, ActivePhysics *apOther){
		doStateChange(&StateID_DieSmoke);
		return true;
	}
	bool daShyGuy::collisionCat1_Fireball_E_Explosion(ActivePhysics *apThis, ActivePhysics *apOther) {
		this->damage += 1;

		dStateBase_c *stateVar;
		stateVar = &StateID_DieSmoke;
		
		if (this->type < 6) {  // Regular Shy Guys Except Jumper

			backFire = apOther->owner->direction ^ 1;
			
			// if (this->isDown == 0) {
			// 	stateVar = &StateID_FireKnockBack;
			// }
			// else {
				StageE4::instance->spawnCoinJump(pos, 0, 1, 0);
				doStateChange(&StateID_DieSmoke);
			// }
		}
		else { // Ballooneers
			stateVar = &StateID_FlameHit;
		}

		if (this->damage > 1) {
			nw4r::snd::SoundHandle handle;
			PlaySoundWithFunctionB4(SoundRelatedClass, &handle, SE_EMY_DOWN, 1);
			StageE4::instance->spawnCoinJump(pos, 0, 1, 0);
			doStateChange(&StateID_DieSmoke);
		}
		else {
			nw4r::snd::SoundHandle handle;
			PlaySoundWithFunctionB4(SoundRelatedClass, &handle, SE_EMY_KURIBO_L_DAMAGE_01, 1);
			doStateChange(stateVar);
		}
		return true;
	}

	// void daShyGuy::collisionCat2_IceBall_15_YoshiIce(ActivePhysics *apThis, ActivePhysics *apOther) {
		
	// 	dEn_C::collisionCat2_IceBall_15_YoshiIce(apThis, apOther);
	// }

	// These handle the ice crap
	void daShyGuy::_vf148() {
		dEn_c::_vf148();
		doStateChange(&StateID_Die);
	}
	void daShyGuy::_vf14C() {
		dEn_c::_vf14C();
		doStateChange(&StateID_Die);
	}

	extern "C" void sub_80024C20(void);
	extern "C" void __destroy_arr(void*, void(*)(void), int, int);
	//extern "C" __destroy_arr(struct DoSomethingCool, void(*)(void), int cnt, int bar);

	bool daShyGuy::CreateIceActors() {
		struct DoSomethingCool my_struct = { 0, this->pos, {1.2, 1.5, 1.5}, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
	    this->frzMgr.Create_ICEACTORs( (void*)&my_struct, 1 );
	    __destroy_arr( (void*)&my_struct, sub_80024C20, 0x3C, 1 );
	    chrAnimation.setUpdateRate(0.0f);
	    return true;
	}

bool daShyGuy::calculateTileCollisions() {
	// Returns true if sprite should turn, false if not.

	HandleXSpeed();
	HandleYSpeed();
	doSpriteMovement();

	cmgr_returnValue = collMgr.isOnTopOfTile();
	collMgr.calculateBelowCollisionWithSmokeEffect();

	if (isBouncing) {
		stuffRelatingToCollisions(0.1875f, 1.0f, 0.5f);
		if (speed.y != 0.0f)
			isBouncing = false;
	}

	float xDelta = pos.x - last_pos.x;
	if (xDelta >= 0.0f)
		direction = 0;
	else
		direction = 1;

	if (collMgr.isOnTopOfTile()) {
		// Walking into a tile branch

		if (cmgr_returnValue == 0)
			isBouncing = true;

		if (speed.x != 0.0f) {
			//playWmEnIronEffect();
		}

		speed.y = 0.0f;

		// u32 blah = collMgr.s_80070760();
		// u8 one = (blah & 0xFF);
		// static const float incs[5] = {0.00390625f, 0.0078125f, 0.015625f, 0.0234375f, 0.03125f};
		// x_speed_inc = incs[one];
		max_speed.x = (direction == 1) ? -XSpeed : XSpeed;
	} else {
		x_speed_inc = 0.0f;
	}

	// Bouncing checks
	if (_34A & 4) {
		Vec v = (Vec){0.0f, 1.0f, 0.0f};
		collMgr.pSpeed = &v;

		if (collMgr.calculateAboveCollision(collMgr.outputMaybe))
			speed.y = 0.0f;

		collMgr.pSpeed = &speed;

	} else {
		if (collMgr.calculateAboveCollision(collMgr.outputMaybe))
			speed.y = 0.0f;
	}

	collMgr.calculateAdjacentCollision(0);

	// Switch Direction
	if (collMgr.outputMaybe & (0x15 << direction)) {
		if (collMgr.isOnTopOfTile()) {
			isBouncing = true;
		}
		return true;
	}
	return false;
}

void daShyGuy::bindAnimChr_and_setUpdateRate(const char* name, int unk, float unk2, float rate) {
	nw4r::g3d::ResAnmChr anmChr = this->anmFile.GetResAnmChr(name);
	this->chrAnimation.bind(&this->bodyModel, anmChr, unk);
	this->bodyModel.bindAnim(&this->chrAnimation, unk2);
	this->chrAnimation.setUpdateRate(rate);
}

int daShyGuy::onCreate() {

	this->type = this->settings >> 28 & 0xF;
	int baln = this->settings >> 24 & 0xF;
	this->distance = this->settings >> 12 & 0xF;

	stillFalling = 0;

	allocator.link(-1, GameHeaps[0], 0, 0x20);

	this->deleteForever = 1;

	// Balloon Specifics
	if (type == 6 || type == 7 || type == 8) {
		this->renderBalloon = 1;

		this->balloonFile.data = getResource("shyguy", "g3d/balloon.brres");
		nw4r::g3d::ResMdl mdlB = this->balloonFile.GetResMdl("ballon");
		balloonModel.setup(mdlB, &allocator, 0x224, 1, 0);
		balloonModelB.setup(mdlB, &allocator, 0x224, 1, 0);

		ActivePhysics::Info iballoonPhysics;

		iballoonPhysics.xDistToCenter = 0.0;
		iballoonPhysics.yDistToCenter = -18.0;
		iballoonPhysics.xDistToEdge   = 13.0;
		iballoonPhysics.yDistToEdge   = 12.0;

		iballoonPhysics.category1  = 0x3;
		iballoonPhysics.category2  = 0x0;
		iballoonPhysics.bitfield1  = 0x4f;
		iballoonPhysics.bitfield2  = 0xffbafffe;
		iballoonPhysics.unkShort1C = 0x0;
		iballoonPhysics.callback   = balloonSmack;

		balloonPhysics.initWithStruct(this, &iballoonPhysics);
		balloonPhysics.addToList();


		// if (baln != 0) {
		// 	char *itemArc;
		// 	char *itemBrres;
		// 	char *itemMdl;

		// 	if (baln == 1) { 
		// 		itemArc		= "I_kinoko";
		// 		itemBrres	= "g3d/I_kinoko.brres";
		// 		itemMdl		= "I_kinoko";
		// 	}
		// 	else if (baln == 2) { 
		// 		itemArc		= "I_fireflower";
		// 		itemBrres	= "g3d/I_fireflower.brres";
		// 		itemMdl		= "I_fireflower";
		// 	}
		// 	else if (baln == 3) { 
		// 		itemArc		= "I_propeller";
		// 		itemBrres	= "g3d/I_propeller.brres";
		// 		itemMdl		= "I_propeller_model";
		// 	}
		// 	else if (baln == 4) { 
		// 		itemArc		= "I_iceflower";
		// 		itemBrres	= "g3d/I_iceflower.brres";
		// 		itemMdl		= "I_iceflower";
		// 	}
		// 	else if (baln == 5) { 
		// 		itemArc		= "I_star";
		// 		itemBrres	= "g3d/I_star.brres";
		// 		itemMdl		= "I_star";
		// 	}
		// 	else if (baln == 6) { 
		// 		itemArc		= "I_penguin";
		// 		itemBrres	= "g3d/I_penguin.brres";
		// 		itemMdl		= "I_penguin";
		// 	}

		// 	this->carryFile.data = getResource(itemArc, itemBrres);
		// 	nw4r::g3d::ResMdl mdlC = this->carryFile.GetResMdl(itemMdl);
		// 	carryModel.setup(mdlC, &allocator, 0x224, 1, 0);

		// 	nw4r::g3d::ResAnmChr anmChrC = this->carryFile.GetResAnmChr("wait2");
		// 	this->carryAnm.setup(mdlC, anmChrC, &this->allocator, 0);

		// 	this->carryAnm.bind(&this->carryModel, anmChrC, 1);
		// 	this->carryModel.bindAnim(&this->carryAnm, 0.0);
		// 	this->carryAnm.setUpdateRate(1.0);
		// }
	}
	else {this->renderBalloon = 0;}


	// Shy Guy Colours
	if (type == 1 || type == 8 || type == 10) {
		this->resFile.data = getResource("shyguy", "g3d/ShyGuyBlue.brres");
		distance = 1;
	}
	else if (type == 5) {
		this->resFile.data = getResource("shyguy", "g3d/ShyGuyGreen.brres");
	}
	else if (type == 3) {
		this->resFile.data = getResource("shyguy", "g3d/ShyGuyCyan.brres");
	}
	else if (type == 4) {
		this->resFile.data = getResource("shyguy", "g3d/ShyGuyPurple.brres");
	}
	else {
		this->resFile.data = getResource("shyguy", "g3d/ShyGuyRed.brres");
	}
	nw4r::g3d::ResMdl mdl = this->resFile.GetResMdl("body_h");
	bodyModel.setup(mdl, &allocator, 0x224, 1, 0);


	// Animations start here
	this->anmFile.data = getResource("shyguy", "g3d/ShyGuyAnimations.brres");
	nw4r::g3d::ResAnmChr anmChr = this->anmFile.GetResAnmChr("c18_IDLE_R");
	this->chrAnimation.setup(mdl, anmChr, &this->allocator, 0);

	allocator.unlink();

	// Stuff I do understand

	this->scale = (Vec){20.0, 20.0, 20.0};

	this->pos.y += 36.0;
	this->rot.x = 0; // X is vertical axis
	this->rot.y = 0xD800; // Y is horizontal axis
	this->rot.z = 0; // Z is ... an axis >.>
	this->direction = 1; // Heading left.
	
	this->speed.x = 0.0;
	this->speed.y = 0.0;
	this->max_speed.x = 0.6;
	this->x_speed_inc = 0.15;
	this->Baseline = this->pos.y;
	this->XSpeed = 0.6;
	this->balloonSize = 1.5;


	ActivePhysics::Info HitMeBaby;

	// Note: if this gets changed, also change the point where the default
	// values are assigned after de-ballooning
	HitMeBaby.xDistToCenter = 0.0;
	HitMeBaby.yDistToCenter = 12.0;
	HitMeBaby.xDistToEdge = 8.0;
	HitMeBaby.yDistToEdge = 12.0;
	if (renderBalloon) {
		HitMeBaby.yDistToCenter = 9.0f;
		HitMeBaby.yDistToEdge = 9.0f;
	}

	HitMeBaby.category1 = 0x3;
	HitMeBaby.category2 = 0x0;
	HitMeBaby.bitfield1 = 0x6F;
	HitMeBaby.bitfield2 = 0xffbafffe;
	HitMeBaby.unkShort1C = 0;
	HitMeBaby.callback = &shyCollisionCallback;

	this->aPhysics.initWithStruct(this, &HitMeBaby);
	this->aPhysics.addToList();


	// Tile collider

	// These fucking rects do something for the tile rect
	spriteSomeRectX = 28.0f;
	spriteSomeRectY = 32.0f;
	_320 = 0.0f;
	_324 = 16.0f;

	// These structs tell stupid collider what to collide with - these are from koopa troopa
	static const lineSensor_s below(-5<<12, 5<<12, 0<<12);
	static const pointSensor_s above(0<<12, 12<<12);
	static const lineSensor_s adjacent(6<<12, 9<<12, 6<<12);

	collMgr.init(this, &below, &above, &adjacent);
	collMgr.calculateBelowCollisionWithSmokeEffect();

	cmgr_returnValue = collMgr.isOnTopOfTile();

	if (collMgr.isOnTopOfTile())
		isBouncing = false;
	else
		isBouncing = true;


	// State Changers
	
	if (type == 0) {
		bindAnimChr_and_setUpdateRate("c18_EV_WIN_2_R", 1, 0.0, 1.5); 
		doStateChange(&StateID_RealWalk);
	}		
	else if (type == 1) {
		bindAnimChr_and_setUpdateRate("c18_EV_WIN_2_R", 1, 0.0, 1.5); 
		doStateChange(&StateID_RealWalk);
	}		
	else if (type == 2) {
		doStateChange(&StateID_Sleep);
	}		
	else if (type == 3) {
		doStateChange(&StateID_Jump);
	}		
	else if (type == 4) {
		// Chuckles is left, Knuckles is Right
		ActivePhysics::Info iChuckles;
		ActivePhysics::Info iKnuckles;

		iChuckles.xDistToCenter = -27.0;
		iChuckles.yDistToCenter = 12.0;
		iChuckles.xDistToEdge   = 27.0;
		iChuckles.yDistToEdge   = 10.0;

		iKnuckles.xDistToCenter = 27.0;
		iKnuckles.yDistToCenter = 12.0;
		iKnuckles.xDistToEdge   = 27.0;
		iKnuckles.yDistToEdge   = 10.0;

		iKnuckles.category1  = iChuckles.category1  = 0x3;
		iKnuckles.category2  = iChuckles.category2  = 0x0;
		iKnuckles.bitfield1  = iChuckles.bitfield1  = 0x4F;
		iKnuckles.bitfield2  = iChuckles.bitfield2  = 0x0;
		iKnuckles.unkShort1C = iChuckles.unkShort1C = 0x0;
		iKnuckles.callback   = iChuckles.callback   = ChucklesAndKnuckles;

		Chuckles.initWithStruct(this, &iChuckles);
		Knuckles.initWithStruct(this, &iKnuckles);

		doStateChange(&StateID_Judo);
	}		
	else if (type == 5) {
		doStateChange(&StateID_Spike);
	}		
	else if (type == 6) {
		doStateChange(&StateID_Balloon_H);
	}		
	else if (type == 7) {
		doStateChange(&StateID_Balloon_V);
	}		
	else if (type == 8) {
		doStateChange(&StateID_Balloon_C);
	}		

	this->onExecute();
	return true;
}

int daShyGuy::onDelete() {
	return true;
}

int daShyGuy::onExecute() {
	acState.execute();
	updateModelMatrices();
	bodyModel._vf1C();

	return true;
}

int daShyGuy::onDraw() {
	bodyModel.scheduleForDrawing();

	if (this->renderBalloon == 1) {
		balloonModel.scheduleForDrawing();
		balloonModelB.scheduleForDrawing();
	}

	// if (this->baln > 0) {
	// 	carryModel.scheduleForDrawing();
	// 	carryModel._vf1C();

	// 	if(this->carryAnm.isAnimationDone())
	// 		this->carryAnm.setCurrentFrame(0.0);
	// }

	return true;
}

void daShyGuy::updateModelMatrices() {
	// This won't work with wrap because I'm lazy.

	if (this->frzMgr._mstate == 1)
		matrix.translation(pos.x, pos.y, pos.z);
	else
		matrix.translation(pos.x, pos.y - 2.0, pos.z);
	matrix.applyRotationYXZ(&rot.x, &rot.y, &rot.z);

	bodyModel.setDrawMatrix(matrix);
	bodyModel.setScale(&scale);
	bodyModel.calcWorld(false);

	if (this->renderBalloon == 1) {
		matrix.translation(pos.x, pos.y - 32.0, pos.z);

		balloonModel.setDrawMatrix(matrix);
		balloonModel.setScale(balloonSize, balloonSize, balloonSize);
		balloonModel.calcWorld(false);

		balloonModelB.setDrawMatrix(matrix);
		balloonModelB.setScale(balloonSize, balloonSize, balloonSize);
		balloonModelB.calcWorld(false);
	}

	// if (this->baln > 0) {
	// 	matrix.applyRotationYXZ(0,0,0);
	// 	matrix.translation(pos.x+40.0, pos.y - 28.0, pos.z + 1000.0);

	// 	carryModel.setDrawMatrix(matrix);
	// 	carryModel.setScale(21.0, 21.0, 21.0);
	// 	carryModel.calcWorld(false);
	// }

}

///////////////
// Walk State
///////////////
	void daShyGuy::beginState_Walk() { 
		this->timer = 0;
		this->rot.y = (direction) ? 0xD800 : 0x2800;

		this->max_speed.x = 0.0;
		this->speed.x = 0.0;
		this->x_speed_inc = 0.0;
	}
	void daShyGuy::executeState_Walk() { 
		chrAnimation.setUpdateRate(1.5f);

		this->pos.x += (direction) ? -0.4 : 0.4;

		if (this->timer > (this->distance * 32)) {
			doStateChange(&StateID_Turn);
		}

		if(this->chrAnimation.isAnimationDone())
			this->chrAnimation.setCurrentFrame(0.0);

		this->timer = this->timer + 1;
	}
	void daShyGuy::endState_Walk() { 
	}

///////////////
// Turn State
///////////////
	void daShyGuy::beginState_Turn() { 
		// bindAnimChr_and_setUpdateRate("c18_IDLE_R", 1, 0.0, 1.0);
		this->direction ^= 1;
		this->speed.x = 0.0;
	}
	void daShyGuy::executeState_Turn() { 

		if(this->chrAnimation.isAnimationDone())
			this->chrAnimation.setCurrentFrame(0.0);

		u16 amt = (this->direction == 0) ? 0x2800 : 0xD800;
		int done = SmoothRotation(&this->rot.y, amt, 0x800);

		if(done) {
			this->doStateChange(&StateID_Walk);
		}
	}
	void daShyGuy::endState_Turn() { 
	}

///////////////
// Jump State
///////////////
	void daShyGuy::beginState_Jump() { 
		this->max_speed.x = 0.0;
		this->speed.x = 0.0;
		this->x_speed_inc = 0.0;

		this->timer = 0;
		this->jumpCounter = 0;
	}
	void daShyGuy::executeState_Jump() { 

		// Always face Mario
		u8 facing = dSprite_c__getXDirectionOfFurthestPlayerRelativeToVEC3(this, this->pos);

		if (facing != this->direction) {
			this->direction = facing;
			this->rot.y = (direction) ? 0xD800 : 0x2800;
		}

		// Shy Guy is on ground
		if (this->pos.y < this->Baseline) {

			bindAnimChr_and_setUpdateRate("c18_IDLE_R", 1, 0.0, 1.0);
			
			this->timer = this->timer + 1;

			// Make him wait for 0.5 seconds
			if (this->timer > 30) {

				if(this->chrAnimation.isAnimationDone())
					this->chrAnimation.setCurrentFrame(0.0);

				this->speed.x = 0;
				this->speed.y = 0;	
			}

			// Then Jump!
			else { 
				if (this->jumpCounter == 3) { this->jumpCounter = 0; }

				this->pos.y = this->Baseline + 1;
				this->timer = 0;
				this->jumpCounter = this->jumpCounter + 1;


				if (this->jumpCounter == 3) {
					bindAnimChr_and_setUpdateRate("c18_NORMAL_STEAL_R", 1, 0.0, 1.0);
					this->speed.y = 8.0;
					PlaySoundAsync(this, SE_PLY_JUMPDAI_HIGH);
				}
				else {
					bindAnimChr_and_setUpdateRate("c18_EV_WIN_1_R", 1, 0.0, 1.0);
					this->speed.y = 6.0;
					PlaySoundAsync(this, SE_PLY_JUMPDAI);
				}

			}
		}

		// While he's jumping, it's time for gravity.
		else { 

			this->speed.y = this->speed.y - 0.15; 

			if (this->jumpCounter == 3) {
				if(this->chrAnimation.isAnimationDone())
					this->chrAnimation.setCurrentFrame(0.0);
			}
			else {
				if(this->chrAnimation.isAnimationDone())
					this->chrAnimation.setCurrentFrame(0.0);
			}
		}

		this->HandleXSpeed();
		this->HandleYSpeed();
		this->UpdateObjectPosBasedOnSpeedValuesReal();
	}
	void daShyGuy::endState_Jump() { 
	}

///////////////
// Sleep State
///////////////
	void daShyGuy::beginState_Sleep() { 
		bindAnimChr_and_setUpdateRate("c18_EV_LOSE_2_R", 1, 0.0, 1.0);
		this->rot.y = 0x0000;
	}
	void daShyGuy::executeState_Sleep() { 
		if(this->chrAnimation.isAnimationDone())
			this->chrAnimation.setCurrentFrame(0.0);
	}
	void daShyGuy::endState_Sleep() { 
	}

///////////////
// Balloon H State
///////////////
	void daShyGuy::beginState_Balloon_H() { 
		bindAnimChr_and_setUpdateRate("c18_L_DMG_F_3_R", 1, 0.0, 1.0);
		this->timer = 0;
		this->initialPos = this->pos;
		this->rot.x = 0xFE00;
		this->rot.y = 0;
	}
	void daShyGuy::executeState_Balloon_H() { 

		// Makes him bob up and down
		this->pos.y = this->initialPos.y + ( sin(this->timer * 3.14 / 60.0) * 6.0 );

		// Makes him move side to side
		this->pos.x = this->initialPos.x + ( sin(this->timer * 3.14 / 600.0) * (float)this->distance * 8.0);

		this->timer = this->timer + 1;

		if(this->chrAnimation.isAnimationDone())
			this->chrAnimation.setCurrentFrame(0.0);

	}
	void daShyGuy::endState_Balloon_H() { 
	}

///////////////
// Balloon V State
///////////////
	void daShyGuy::beginState_Balloon_V() { 
		bindAnimChr_and_setUpdateRate("c18_L_DMG_F_3_R", 1, 0.0, 1.0);
		this->timer = 0;
		this->initialPos = this->pos;
		this->rot.x = 0xFE00;
		this->rot.y = 0;
	}
	void daShyGuy::executeState_Balloon_V() { 
		// Makes him bob up and down
		this->pos.x = this->initialPos.x + ( sin(this->timer * 3.14 / 60.0) * 6.0 );

		// Makes him move side to side
		this->pos.y = this->initialPos.y + ( sin(this->timer * 3.14 / 600.0) * (float)this->distance * 8.0 );

		this->timer = this->timer + 1;

		if(this->chrAnimation.isAnimationDone())
			this->chrAnimation.setCurrentFrame(0.0);
	}
	void daShyGuy::endState_Balloon_V() { 
	}

///////////////
// Balloon C State
///////////////
	void daShyGuy::beginState_Balloon_C() { 
		bindAnimChr_and_setUpdateRate("c18_L_DMG_F_3_R", 1, 0.0, 1.0);
		this->timer = 0;
		this->initialPos = this->pos;
		this->rot.x = 0xFE00;
		this->rot.y = 0;
	}
	void daShyGuy::executeState_Balloon_C() { 
		// Makes him bob up and down
		this->pos.x = this->initialPos.x + ( sin(this->timer * 3.14 / 600.0) * (float)this->distance * 8.0 );

		// Makes him move side to side
		this->pos.y = this->initialPos.y + ( cos(this->timer * 3.14 / 600.0) * (float)this->distance * 8.0 );

		this->timer = this->timer + 1;

		if(this->chrAnimation.isAnimationDone())
			this->chrAnimation.setCurrentFrame(0.0);
	}
	void daShyGuy::endState_Balloon_C() { 
	}

///////////////
// Judo State
///////////////
	void daShyGuy::beginState_Judo() { 
		this->max_speed.x = 0.0;
		this->speed.x = 0.0;
		this->x_speed_inc = 0.0;
		this->pos.y -= 4.0;

		this->timer = 0;
	}
	void daShyGuy::executeState_Judo() { 

	// chargin 476? 673? 760? 768? 808? 966?
		if (this->timer == 0) { bindAnimChr_and_setUpdateRate("c18_OB_IDLE_R", 1, 0.0, 1.0); }

		this->timer = this->timer + 1;

		if (this->timer == 80) { 
			if (this->direction == 1) { 
				SpawnEffect("Wm_ob_keyget02_lighit", 0, &(Vec){this->pos.x + 7.0, this->pos.y + 14.0, this->pos.z - 5500.0}, &(S16Vec){0,0,0}, &(Vec){0.8, 0.8, 0.8});
			}
			else {
				SpawnEffect("Wm_ob_keyget02_lighit", 0, &(Vec){this->pos.x - 7.0, this->pos.y + 14.0, this->pos.z + 5500.0}, &(S16Vec){0,0,0}, &(Vec){0.8, 0.8, 0.8});
			}	
		}

		if (this->timer < 120) {
			// Always face Mario
			u8 facing = dSprite_c__getXDirectionOfFurthestPlayerRelativeToVEC3(this, this->pos);

			if (facing != this->direction) {
				this->direction = facing;
				if (this->direction == 1) {
					this->rot.y = 0xD800;
				}
				else {
					this->rot.y = 0x2800;
				}
			}


			if(this->chrAnimation.isAnimationDone())
				this->chrAnimation.setCurrentFrame(0.0);
		}

		else if (this->timer == 120) {
			bindAnimChr_and_setUpdateRate("c18_H_CUT_R", 1, 0.0, 1.0);
			
		}

		else if (this->timer == 132) {
			PlaySoundAsync(this, SE_EMY_CRASHER_PUNCH);

			if (this->direction == 1) { 
				SpawnEffect("Wm_mr_wallkick_b_l", 0, &(Vec){this->pos.x - 18.0, this->pos.y + 16.0, this->pos.z}, &(S16Vec){0,0,0}, &(Vec){1.5, 1.5, 1.5});
				Chuckles.addToList();
			}
			else {
				SpawnEffect("Wm_mr_wallkick_s_r", 0, &(Vec){this->pos.x + 18.0, this->pos.y + 16.0, this->pos.z}, &(S16Vec){0,0,0}, &(Vec){1.5, 1.5, 1.5});
				Knuckles.addToList();
			}	
		}

		else {

			if(this->chrAnimation.isAnimationDone()) {
				if (this->direction == 1) { 
					SpawnEffect("Wm_mr_wirehit_hit", 0, &(Vec){this->pos.x - 38.0, this->pos.y + 16.0, this->pos.z}, &(S16Vec){0,0,0}, &(Vec){1.0, 1.0, 1.0});
					Chuckles.removeFromList();
				}
				else {
					SpawnEffect("Wm_mr_wirehit_hit", 0, &(Vec){this->pos.x + 38.0, this->pos.y + 16.0, this->pos.z}, &(S16Vec){0,0,0}, &(Vec){1.0, 1.0, 1.0});
					Knuckles.removeFromList();	
				}

				this->timer = 0;
				PlaySoundAsync(this, SE_EMY_BIG_PAKKUN_DAMAGE_1);
			}
		}
	}
	void daShyGuy::endState_Judo() { 
	}

///////////////
// Spike State
///////////////
	void daShyGuy::beginState_Spike() { 
		this->timer = 80;
		spikeTurn = 0;

		this->max_speed.x = 0.0;
		this->speed.x = 0.0;
		this->x_speed_inc = 0.0;
		this->pos.y -= 4.0;
	}
	void daShyGuy::executeState_Spike() {

		if (this->timer == 0) { bindAnimChr_and_setUpdateRate("c18_OB_IDLE_R", 1, 0.0, 1.0); }

		if (this->timer < 120) {
			// Always face Mario
			u8 facing = dSprite_c__getXDirectionOfFurthestPlayerRelativeToVEC3(this, this->pos);

			if (facing != this->direction) {
				this->direction = facing;
				if (this->direction == 1) {
					this->rot.y = 0xD800;
				}
				else {
					this->rot.y = 0x2800;
				}
			}

			if(this->chrAnimation.isAnimationDone())
				this->chrAnimation.setCurrentFrame(0.0);
		}

		else if (this->timer == 120) {
			bindAnimChr_and_setUpdateRate("c18_H_SHOT_R", 1, 0.0, 1.0);			
		}

		else if (this->timer == 160) {
			PlaySound(this, SE_EMY_KANIBO_THROW);

			Vec pos;
			pos.x = this->pos.x;
			pos.y = this->pos.y;
			pos.z = this->pos.z;
			dStageActor_c *spawned;

			if (this->direction == 1) { 
				spawned = CreateActor(89, 0x2, pos, 0, 0);
				spawned->scale.x = 0.9;
				spawned->scale.y = 0.9;
				spawned->scale.z = 0.9;

				spawned->speed.x = -2.0;
				spawned->speed.y = 2.0;
			}
			else {
				spawned = CreateActor(89, 0x12, pos, 0, 0);
				spawned->scale.x = 0.9;
				spawned->scale.y = 0.9;
				spawned->scale.z = 0.9;

				spawned->speed.x = 2.0;
				spawned->speed.y = 2.0;
			}
		}

		else {

			if(this->chrAnimation.isAnimationDone()) {
				this->timer = 0;
				return;
			}
		}

		this->timer = this->timer + 1;

	}
	void daShyGuy::endState_Spike() { 
	}

///////////////
// Real Walk State
///////////////
bool daShyGuy::willWalkOntoSuitableGround() {
	static const float deltas[] = {2.5f, -2.5f};
	VEC3 checkWhere = {
			pos.x + deltas[direction],
			4.0f + pos.y,
			pos.z};

	u32 props = collMgr.getTileBehaviour2At(checkWhere.x, checkWhere.y, currentLayerID);

	//if (getSubType(props) == B_SUB_LEDGE)
	if (((props >> 16) & 0xFF) == 8)
		return false;

	float someFloat = 0.0f;
	if (collMgr.sub_800757B0(&checkWhere, &someFloat, currentLayerID, 1, -1)) {
		if (someFloat < checkWhere.y && someFloat > (pos.y - 5.0f))
			return true;
	}

	return false;
}


	void daShyGuy::beginState_RealWalk() {
		//inline this piece of code
		this->max_speed.x = (this->direction) ? -this->XSpeed : this->XSpeed;
		this->speed.x = (direction) ? -0.6f : 0.6f;

		this->max_speed.y = -4.0;
		this->speed.y = -4.0;
		this->y_speed_inc = -0.1875;
	}
	void daShyGuy::executeState_RealWalk() { 
		chrAnimation.setUpdateRate(1.5f);

		// if (distance) {
		// 	// What the fuck. Somehow, having this code makes the shyguy not
		// 	// fall through solid-on-top platforms...
		// 	bool turn = collMgr.isOnTopOfTile();
		// 	if (!turn) {
		// 		if (!stillFalling) {
		// 			stillFalling = true;
		// 			pos.x = direction ? pos.x + 1.5 : pos.x - 1.5;
		// 			doStateChange(&StateID_RealTurn);
		// 		}
		// 	} else 
		// }


		if (distance) {
			if (collMgr.isOnTopOfTile()) {
				stillFalling = false;

				if (!willWalkOntoSuitableGround()) {
					pos.x = direction ? pos.x + 1.5 : pos.x - 1.5;
					doStateChange(&StateID_RealTurn);
				}
			}
			else {
				if (!stillFalling) {
					stillFalling = true;
					pos.x = direction ? pos.x + 1.5 : pos.x - 1.5;
					doStateChange(&StateID_RealTurn);
				}
			}
		}

		bool ret = calculateTileCollisions();
		if (ret) {
			doStateChange(&StateID_RealTurn);
		}

		if(this->chrAnimation.isAnimationDone()) {
			this->chrAnimation.setCurrentFrame(0.0);
		}
	}
	void daShyGuy::endState_RealWalk() { }

///////////////
// Real Turn State
///////////////
	void daShyGuy::beginState_RealTurn() {

		this->direction ^= 1;
		this->speed.x = 0.0;
	}
	void daShyGuy::executeState_RealTurn() { 

		if(this->chrAnimation.isAnimationDone()) {
			this->chrAnimation.setCurrentFrame(0.0);
		}

		u16 amt = (this->direction == 0) ? 0x2800 : 0xD800;
		int done = SmoothRotation(&this->rot.y, amt, 0x800);

		if(done) {
			this->doStateChange(&StateID_RealWalk);
		}
	}
	void daShyGuy::endState_RealTurn() {
	}

///////////////
// GoDizzy State
///////////////
	void daShyGuy::beginState_GoDizzy() {
		bindAnimChr_and_setUpdateRate("c18_L_DMG_F_1_R", 1, 0.0, 1.0); 

		// SpawnEffect("Wm_en_spindamage", 0, &(Vec){this->pos.x, this->pos.y + 24.0, 0}, &(S16Vec){0,0,0}, &(Vec){1.0, 1.0, 1.0});

		this->max_speed.x = 0;
		this->speed.x = 0;
		this->x_speed_inc = 0;

		this->max_speed.y = -4.0;
		this->speed.y = -4.0;
		this->y_speed_inc = -0.1875;

		this->timer = 0;
		this->jumpCounter = 0;
		this->isDown = 1;
	}
	void daShyGuy::executeState_GoDizzy() { 
		calculateTileCollisions();
	
		effect.spawn("Wm_en_spindamage", 0, &(Vec){this->pos.x, this->pos.y + 24.0, 0}, &(S16Vec){0,0,0}, &(Vec){1.0, 1.0, 1.0});

		if (this->jumpCounter == 0) {
			if(this->chrAnimation.isAnimationDone()) {
				this->jumpCounter = 1;
				bindAnimChr_and_setUpdateRate("c18_L_DMG_F_3_R", 1, 0.0, 1.0); 
			}
		}

		else {
			if(this->chrAnimation.isAnimationDone()) {
				this->chrAnimation.setCurrentFrame(0.0);
			}

			if (this->timer > 600) {
				doStateChange(&StateID_Recover);
				damage = 0;
			}

			this->timer += 1;
		}
	}
	void daShyGuy::endState_GoDizzy() {}

///////////////
// BalloonDrop State
///////////////
	void daShyGuy::beginState_BalloonDrop() {
		bindAnimChr_and_setUpdateRate("c18_C_BLOCK_BREAK_R", 1, 0.0, 2.0); 

		this->max_speed.x = 0.0;
		this->speed.x = 0.0;
		this->x_speed_inc = 0.0;

		this->max_speed.y = -2.0;
		this->speed.y = -2.0;
		this->y_speed_inc = -0.1875;

		this->isDown = 1;
		this->renderBalloon = 0;

		// char powerup;

		// if (baln == 1) { 
		// 	powerup		= 0x0B000007;
		// }
		// else if (baln == 2) { 
		// 	powerup		= 0x0B000009;
		// }
		// else if (baln == 3) { 
		// 	powerup		= 0x0B000001;
		// }
		// else if (baln == 4) { 
		// 	powerup		= 0x0C00000E;
		// }
		// else if (baln == 5) { 
		// 	powerup		= 0x0C000015;
		// }
		// else if (baln == 6) { 
		// 	powerup		= 0x0C000011;
		// }
		// CreateActor(60, powerup, (Vec){pos.x, pos.y - 28.0, pos.z}, 0, 0);
		// this->baln = 0;

		balloonPhysics.removeFromList();
		SpawnEffect("Wm_en_explosion_ln", 0, &(Vec){this->pos.x, this->pos.y - 32.0, 0}, &(S16Vec){0,0,0}, &(Vec){1.0, 1.0, 1.0});
		PlaySound(this, SE_PLY_BALLOON_BRAKE); 

		if (this->type != 8)
			this-distance == 0;

		type = 0;
	}
	void daShyGuy::executeState_BalloonDrop() { 

		if(this->chrAnimation.isAnimationDone()) {
			this->chrAnimation.setCurrentFrame(0.0);
		}

		bool ret = calculateTileCollisions();

		if (speed.y == 0.0) { 
			SpawnEffect("Wm_en_sndlandsmk_s", 0, &(Vec){this->pos.x, this->pos.y, 0}, &(S16Vec){0,0,0}, &(Vec){1.0, 1.0, 1.0});
			doStateChange(&StateID_GoDizzy);

			aPhysics.info.yDistToCenter = 12.0f;
			aPhysics.info.yDistToEdge = 12.0f;
		}
	}
	void daShyGuy::endState_BalloonDrop() {
	}

///////////////
// FireKnockBack State
///////////////
	void daShyGuy::beginState_FireKnockBack() {
		bindAnimChr_and_setUpdateRate("c18_C_BLOCK_BREAK_R", 1, 0.0, 1.0); 

		// Backfire 0 == Fireball to the right
		// Backfire 1 == Fireball to the left

		directionStore = this->direction;
		speed.x = (this->backFire) ? this->XSpeed : -this->XSpeed;
		speed.x *= 1.2f;
		max_speed.x = speed.x;
		x_speed_inc = 0.0f;
	}
	void daShyGuy::executeState_FireKnockBack() { 

		calculateTileCollisions();
		// move backwards here
		this->speed.x = this->speed.x / 1.02f;

		if(this->chrAnimation.isAnimationDone()) {
			if (aPhysics.result1 == 0 && aPhysics.result2 == 0 && aPhysics.result3 == 0) {
				bindAnimChr_and_setUpdateRate("c18_EV_WIN_2_R", 1, 0.0, 1.5); 
				doStateChange(&StateID_RealWalk);
			}
		}
	}
	void daShyGuy::endState_FireKnockBack() {
		this->direction = directionStore;		
	}

///////////////
// FlameHit State
///////////////
	void daShyGuy::beginState_FlameHit() {
		bindAnimChr_and_setUpdateRate("c18_C_BLOCK_BREAK_R", 1, 0.0, 1.0); 
	}
	void daShyGuy::executeState_FlameHit() { 

		if(this->chrAnimation.isAnimationDone()) {
			if (type == 6) {
				doStateChange(&StateID_Balloon_H);
			}		
			else if (type == 7) {
				doStateChange(&StateID_Balloon_V);
			}		
			else if (type == 8) {
				doStateChange(&StateID_Balloon_C);
			}		
		}
	}
	void daShyGuy::endState_FlameHit() {}

///////////////
// Recover State
///////////////
	void daShyGuy::beginState_Recover() {
		bindAnimChr_and_setUpdateRate("c18_L_DMG_F_4_R", 1, 0.0, 1.0); 
	}
	void daShyGuy::executeState_Recover() { 

		calculateTileCollisions();

		if(this->chrAnimation.isAnimationDone()) {
			if (type == 3) {
				doStateChange(&StateID_Jump);
			}		
			else {
				bindAnimChr_and_setUpdateRate("c18_EV_WIN_2_R", 1, 0.0, 1.5); 
				doStateChange(&StateID_RealWalk);
			}
		}
	}
	void daShyGuy::endState_Recover() {
		this->isDown = 0;		
		this->rot.y = (direction) ? 0xD800 : 0x2800;
	}

///////////////
// Die State
///////////////
	void daShyGuy::beginState_Die() {
		// dEn_c::dieFall_Begin();
		this->removeMyActivePhysics();

		bindAnimChr_and_setUpdateRate("c18_C_BLOCK_BREAK_R", 1, 0.0, 2.0); 
		this->timer = 0;
		this->dying = -10.0;
		this->Baseline = this->pos.y;
		this->rot.y = 0;
		this->rot.x = 0;

		if (type > 5 && type < 9) {
			this->renderBalloon = 0;
			SpawnEffect("Wm_en_explosion_ln", 0, &(Vec){this->pos.x, this->pos.y - 32.0, 0}, &(S16Vec){0,0,0}, &(Vec){1.0, 1.0, 1.0});
		}
	}
	void daShyGuy::executeState_Die() { 

		if(this->chrAnimation.isAnimationDone()) {
			this->chrAnimation.setCurrentFrame(0.0);
		}

		this->timer += 1;
		 		
		// this->pos.x += 0.5; 
		this->pos.y = Baseline + (-0.2 * dying * dying) + 20.0;
		
		this->dying += 0.5;
			
		if (this->timer > 450) {
			OSReport("Killing");
			this->kill();
			this->Delete(this->deleteForever);
		}

		// dEn_c::dieFall_Execute();

	}
	void daShyGuy::endState_Die() {
	}


//
// processed\../src/meteor.cpp
//

#include <common.h>
#include <game.h>
#include <g3dhax.h>
#include <sfx.h>
#include "boss.h"

class dMeteor : public dEn_c {
	int onCreate();
	int onDelete();
	int onExecute();
	int onDraw();

	static dMeteor *build();

	mHeapAllocator_c allocator;
	m3d::mdl_c bodyModel;
	nw4r::g3d::ResFile resFile;
	mEf::es2 effect;

	int timer;
	int spinSpeed;
	char spinDir;
	char isElectric;

	Physics MakeItRound;

	void updateModelMatrices();
	void playerCollision(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat7_GroundPound(ActivePhysics *apThis, ActivePhysics *apOther);

	public:
		void kill();
};

dMeteor *dMeteor::build() {
	void *buffer = AllocFromGameHeap1(sizeof(dMeteor));
	return new(buffer) dMeteor;
}

const char* MEarcNameList [] = {
	"kazan_rock",
	NULL	
};

// extern "C" dStageActor_c *GetSpecificPlayerActor(int num);
// extern "C" void *modifyPlayerPropertiesWithRollingObject(dStageActor_c *Player, float _52C);


void dMeteor::playerCollision(ActivePhysics *apThis, ActivePhysics *apOther) { 
	DamagePlayer(this, apThis, apOther);
}

void MeteorPhysicsCallback(dMeteor *self, dEn_c *other) {
	if (other->name == 657) {
		OSReport("CANNON COLLISION");

		SpawnEffect("Wm_en_explosion", 0, &other->pos, &(S16Vec){0,0,0}, &(Vec){1.0, 1.0, 1.0});
		SpawnEffect("Wm_en_explosion_smk", 0, &other->pos, &(S16Vec){0,0,0}, &(Vec){1.0, 1.0, 1.0});
		PlaySound(other, SE_OBJ_TARU_BREAK);
		other->Delete(1);

		switch ((self->settings >> 24) & 3) {
			case 1:
				dStageActor_c::create(EN_HATENA_BALLOON, 0x100, &self->pos, 0, self->currentLayerID);
				break;
			case 2:
				VEC3 coinPos = {self->pos.x - 16.0f, self->pos.y, self->pos.z};
				dStageActor_c::create(EN_COIN, 9, &coinPos, 0, self->currentLayerID);
				break;
		}

		self->kill();
	}
}

bool dMeteor::collisionCat7_GroundPound(ActivePhysics *apThis, ActivePhysics *apOther) { 
	DamagePlayer(this, apThis, apOther);
	return true;
}


int dMeteor::onCreate() {

	// Setup Model
	allocator.link(-1, GameHeaps[0], 0, 0x20);

	this->resFile.data = getResource("kazan_rock", "g3d/kazan_rock.brres");
	nw4r::g3d::ResMdl mdl = this->resFile.GetResMdl("kazan_rock");
	bodyModel.setup(mdl, &allocator, 0x224, 1, 0);
	SetupTextures_Enemy(&bodyModel, 0);

	allocator.unlink();


	// Retrieve Scale and set it up
	float sca = (float)((this->settings >> 8) & 0xFF);
	sca = (sca/5.0) + 0.2;

	this->scale = (Vec){sca,sca,sca};

	// Other settings
	this->spinDir = this->settings & 0x1;
	this->spinSpeed = ((this->settings >> 16) & 0xFF) * 20;
	this->isElectric = (this->settings >> 4) & 0x1;


	// Setup Physics
	if (isElectric) {
		ActivePhysics::Info elec;
		elec.xDistToCenter = 0.0;
		elec.yDistToCenter = 0.0;

		elec.xDistToEdge = 13.0 * sca;
		elec.yDistToEdge = 13.0 * sca;

		elec.category1 = 0x3;
		elec.category2 = 0x0;
		elec.bitfield1 = 0x4F;
		elec.bitfield2 = 0x200;
		elec.unkShort1C = 0;
		elec.callback = &dEn_c::collisionCallback;

		this->aPhysics.initWithStruct(this, &elec);
		this->aPhysics.addToList();	
	}

	MakeItRound.baseSetup(this, &MeteorPhysicsCallback, &MeteorPhysicsCallback, &MeteorPhysicsCallback, 1, 0);

	MakeItRound.x = 0.0;
	MakeItRound.y = 0.0;

	MakeItRound.diameter = 13.0 * sca;
	MakeItRound.isRound = 1;

	MakeItRound.update();

	MakeItRound.addToList();

	this->pos.z = (settings & 0x1000000) ? -2000.0f : 3458.0f;
		
	this->onExecute();
	return true;
}

int dMeteor::onDelete() {
	return true;
}

int dMeteor::onExecute() {

	if (spinDir == 0) 	{ rot.z -= spinSpeed; }
	else 				{ rot.z += spinSpeed; }

	MakeItRound.update();
	updateModelMatrices();

	if (isElectric) {
		effect.spawn("Wm_en_birikyu_biri", 0, &(Vec){pos.x, pos.y, pos.z+500.0}, &rot, &(Vec){scale.x*0.8, scale.y*0.8, scale.z*0.8});
		PlaySound(this, SE_EMY_BIRIKYU_SPARK);
	}

	// for (i=0; i<4; i++) {
	// 	dStageActor_c *player = GetSpecificPlayerActor(i);
	// 	modifyPlayerPropertiesWithRollingObject(player, );
	// }

	return true;
}

int dMeteor::onDraw() {

	bodyModel.scheduleForDrawing();
	bodyModel._vf1C();
	return true;
}

void dMeteor::updateModelMatrices() {
	// This won't work with wrap because I'm lazy.
	matrix.translation(pos.x, pos.y, pos.z);
	matrix.applyRotationYXZ(&rot.x, &rot.y, &rot.z);

	bodyModel.setDrawMatrix(matrix);
	bodyModel.setScale(&scale);
	bodyModel.calcWorld(false);
}

void dMeteor::kill() {
	PlaySound(this, SE_OBJ_ROCK_LAND);
	SpawnEffect("Wm_ob_cmnboxsmoke", 0, &pos, &rot, &scale);
	SpawnEffect("Wm_ob_cmnboxgrain", 0, &pos, &rot, &scale);

	this->Delete(1);
}



//
// processed\../src/electricLine.cpp
//

#include <common.h>
#include <game.h>
#include <g3dhax.h>
#include <sfx.h>

class daElectricLine : public dEn_c {
	int onCreate();
	int onDelete();
	int onExecute();
	int onDraw();

	mHeapAllocator_c allocator;
	nw4r::g3d::ResFile resFile;

	dEn_c *Needles;
	u32 delay;
	u32 timer;
	char loops;

	static daElectricLine *build();

	USING_STATES(daElectricLine);
	DECLARE_STATE(Activate);
	DECLARE_STATE(Deactivate);
	DECLARE_STATE(Die);
};

daElectricLine *daElectricLine::build() {
	void *buffer = AllocFromGameHeap1(sizeof(daElectricLine));
	return new(buffer) daElectricLine;
}

///////////////////////
// Externs and States
///////////////////////


	CREATE_STATE(daElectricLine, Activate);
	CREATE_STATE(daElectricLine, Deactivate);
	CREATE_STATE(daElectricLine, Die);



int daElectricLine::onCreate() {

	Vec temppos = this->pos;
	temppos.x += 24.0;

	// Settings for rotation: 0 = facing right, 1 = facing left, 2 = facing up, 3 = facing down
	char settings = 0;
	if (this->settings & 0x1) {
		settings = 1;
		temppos.x -= 32.0;
	}


	Needles = (daNeedles*)create(NEEDLE_FOR_KOOPA_JR_B, settings, &temppos, &this->rot, 0);
	Needles->doStateChange(&daNeedles::StateID_DemoWait);
	
	// Needles->aPhysics.info.category1 = 0x3;
	// Needles->aPhysics.info.bitfield1 = 0x4F;
	// Needles->aPhysics.info.bitfield2 = 0xffbafffe;

	// Delay in 1/6ths of a second
	this->delay = (this->settings >> 16) * 10;
	this->loops = (this->settings >> 4);

	// State Changers
	doStateChange(&StateID_Activate);

	this->onExecute();
	return true;
}

int daElectricLine::onDelete() {
	return true;
}

int daElectricLine::onExecute() {
	acState.execute();	
	return true;
}

int daElectricLine::onDraw() {
	return true;
}


// States:
//
// DemoWait - all nullsubs, does nothing
// DemoAwake - moves the spikes in their respective directions
// Idle - Fires off an infinity of effects for some reason.
// Die - removes physics, then nullsubs


///////////////
// Activate State
///////////////
	void daElectricLine::beginState_Activate() { 
		this->timer = this->delay;
		Needles->doStateChange(&daNeedles::StateID_Idle);
	}
	void daElectricLine::executeState_Activate() { 
		if (this->loops) {
			this->timer--;
			if (this->timer == 0) {
				this->loops += 1;
				doStateChange(&StateID_Deactivate);
			}
		}
	}
	void daElectricLine::endState_Activate() { }

///////////////
// Deactivate State
///////////////
	void daElectricLine::beginState_Deactivate() { 
		this->timer = this->delay; 
		Needles->removeMyActivePhysics();
		Needles->doStateChange(&daNeedles::StateID_DemoWait);
	}
	void daElectricLine::executeState_Deactivate() { 

		this->timer--;
		if (this->timer == 0) {
			doStateChange(&StateID_Activate);
		}
	}
	void daElectricLine::endState_Deactivate() { 
		Needles->addMyActivePhysics();
	}


///////////////
// Die State
///////////////
	void daElectricLine::beginState_Die() { Needles->doStateChange(&daNeedles::StateID_Die); }
	void daElectricLine::executeState_Die() { }
	void daElectricLine::endState_Die() { }


//
// processed\../src/thundercloud.cpp
//

#include <common.h>
#include <game.h>
#include <g3dhax.h>

#include "boss.h"

const char* TLCarcNameList [] = {
	"tcloud",
	NULL	
};

class dThunderCloud : public dEn_c {
	int onCreate();
	int onDelete();
	int onExecute();
	int onDraw();

	mHeapAllocator_c allocator;
	m3d::mdl_c bodyModel;
	nw4r::g3d::ResFile resFile;
	m3d::anmChr_c anm;

	mEf::es2 bolt;
	mEf::es2 charge;

	float Baseline;
	u32 timer;
	int dying;
	char killFlag;
	bool stationary;
	float leader;
	pointSensor_s below;

	bool usingEvents;
	u64 eventFlag;

	ActivePhysics Lightning;

	void dieFall_Begin();
	void dieFall_Execute();
	static dThunderCloud *build();

	void updateModelMatrices();

	void playerCollision(ActivePhysics *apThis, ActivePhysics *apOther);
	void yoshiCollision(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat1_Fireball_E_Explosion(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat14_YoshiFire(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat13_Hammer(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat9_RollingObject(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat3_StarPower(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCatD_Drill(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat7_GroundPound(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat7_GroundPoundYoshi(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCatA_PenguinMario(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat5_Mario(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat11_PipeCannon(ActivePhysics *apThis, ActivePhysics *apOther);

	void bindAnimChr_and_setUpdateRate(const char* name, int unk, float unk2, float rate);

	void powBlockActivated(bool isNotMPGP);

	void _vf148();
	void _vf14C();
	bool CreateIceActors();

	void lightningStrike();

	USING_STATES(dThunderCloud);
	DECLARE_STATE(Follow);
	DECLARE_STATE(Lightning);
	DECLARE_STATE(Wait);
};

dThunderCloud *dThunderCloud::build() {
	void *buffer = AllocFromGameHeap1(sizeof(dThunderCloud));
	return new(buffer) dThunderCloud;
}


CREATE_STATE(dThunderCloud, Follow);
CREATE_STATE(dThunderCloud, Lightning);
CREATE_STATE(dThunderCloud, Wait);

void dThunderCloud::powBlockActivated(bool isNotMPGP) { }


// Collision Callbacks
	extern "C" void dAcPy_vf3F4(void* mario, void* other, int t);
	extern "C" bool BigHanaFireball(dEn_c* t, ActivePhysics *apThis, ActivePhysics *apOther);
	extern "C" void *dAcPy_c__ChangePowerupWithAnimation(void * Player, int powerup);
	extern "C" int CheckExistingPowerup(void * Player);

	void dThunderCloud::playerCollision(ActivePhysics *apThis, ActivePhysics *apOther) { 

		if (this->counter_504[apOther->owner->which_player]) {
			if (apThis->info.category2 == 0x9) {
				int p = CheckExistingPowerup(apOther->owner);
				if (p != 3) {	// Powerups - 0 = small; 1 = big; 2 = fire; 3 = mini; 4 = prop; 5 = peng; 6 = ice; 7 = hammer
					dAcPy_c__ChangePowerupWithAnimation(apOther->owner, 3);
				}
				else { dAcPy_vf3F4(apOther->owner, this, 9); }
			}

			else { dAcPy_vf3F4(apOther->owner, this, 9); }
		}

		this->counter_504[apOther->owner->which_player] = 0x20;
	}

	void dThunderCloud::yoshiCollision(ActivePhysics *apThis, ActivePhysics *apOther) { this->playerCollision(apThis, apOther); }
	bool dThunderCloud::collisionCatD_Drill(ActivePhysics *apThis, ActivePhysics *apOther) { this->playerCollision(apThis, apOther); return true; }
	bool dThunderCloud::collisionCat7_GroundPound(ActivePhysics *apThis, ActivePhysics *apOther) { this->playerCollision(apThis, apOther); return true; }
	bool dThunderCloud::collisionCat7_GroundPoundYoshi(ActivePhysics *apThis, ActivePhysics *apOther) { this->playerCollision(apThis, apOther); return true; }
	bool dThunderCloud::collisionCatA_PenguinMario(ActivePhysics *apThis, ActivePhysics *apOther) { this->playerCollision(apThis, apOther); return true; }
	bool dThunderCloud::collisionCat5_Mario(ActivePhysics *apThis, ActivePhysics *apOther) { this->playerCollision(apThis, apOther); return true; }
	bool dThunderCloud::collisionCat11_PipeCannon(ActivePhysics *apThis, ActivePhysics *apOther) { this->playerCollision(apThis, apOther); return true; }

	bool dThunderCloud::collisionCat1_Fireball_E_Explosion(ActivePhysics *apThis, ActivePhysics *apOther) { return BigHanaFireball(this, apThis, apOther); }
	bool dThunderCloud::collisionCat14_YoshiFire(ActivePhysics *apThis, ActivePhysics *apOther) { return BigHanaFireball(this, apThis, apOther); }

	bool dThunderCloud::collisionCat13_Hammer(ActivePhysics *apThis, ActivePhysics *apOther) { 
		if (apThis->info.category2 == 0x9) { return true; }
		PlaySound(this, SE_EMY_DOWN);
		doStateChange(&StateID_DieFall);
		return true;
	}
	bool dThunderCloud::collisionCat9_RollingObject(ActivePhysics *apThis, ActivePhysics *apOther) {
		this->collisionCat13_Hammer(apThis, apOther);
		return true;
	}
	bool dThunderCloud::collisionCat3_StarPower(ActivePhysics *apThis, ActivePhysics *apOther) { 
		if (apThis->info.category2 == 0x9) { return true; }
		dEn_c::collisionCat3_StarPower(apThis, apOther);
		this->collisionCat13_Hammer(apThis, apOther);
		return true;
	}


	// These handle the ice crap
	void dThunderCloud::_vf148() {
		dEn_c::_vf148();
		doStateChange(&StateID_DieFall);
	}
	void dThunderCloud::_vf14C() {
		dEn_c::_vf14C();
		doStateChange(&StateID_DieFall);
	}

	extern "C" void sub_80024C20(void);
	extern "C" void __destroy_arr(void*, void(*)(void), int, int);
	//extern "C" __destroy_arr(struct DoSomethingCool, void(*)(void), int cnt, int bar);

	bool dThunderCloud::CreateIceActors() {
		this->Lightning.removeFromList();

		struct DoSomethingCool my_struct = { 0, (Vec){pos.x, pos.y - 16.0, pos.z}, {1.75, 1.4, 1.5}, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
	    this->frzMgr.Create_ICEACTORs( (void*)&my_struct, 1 );
	    __destroy_arr( (void*)&my_struct, sub_80024C20, 0x3C, 1 );
	    return true;
	}


void dThunderCloud::dieFall_Begin() {
	this->Lightning.removeFromList();
	this->timer = 0; 
	this->dEn_c::dieFall_Begin();
}
void dThunderCloud::dieFall_Execute() {	
	if (this->killFlag == 1) { return; }

	this->timer = this->timer + 1;
	 
	this->dying = this->dying + 0.15;
	
	this->pos.x = this->pos.x + 0.15;
	this->pos.y = this->pos.y - ((-0.2 * (this->dying*this->dying)) + 5);
	
	this->dEn_c::dieFall_Execute();
		
	if (this->timer > 450) {
		
		if (((this->settings >> 28) > 0) || (stationary)) { 		
			this->Delete(1);
			this->killFlag = 1;
			return;
		}
		
		dStageActor_c *Player = GetSpecificPlayerActor(0);
		if (Player == 0) { Player = GetSpecificPlayerActor(1); }
		if (Player == 0) { Player = GetSpecificPlayerActor(2); }
		if (Player == 0) { Player = GetSpecificPlayerActor(3); }
		

		if (Player == 0) { 
			this->pos.x = 0;
		} else {
			this->pos.x = Player->pos.x - 300;
		}
				
		this->pos.y = this->Baseline; 

		SpawnEffect("Wm_en_blockcloud", 0, &pos, &(const S16Vec){0,0,0}, &(const Vec){1.0f,1.0f,1.0f});
		
		scale.x = scale.y = scale.z = 0.0f;
		this->aPhysics.addToList();
		doStateChange(&StateID_Follow);
	}
}


void dThunderCloud::bindAnimChr_and_setUpdateRate(const char* name, int unk, float unk2, float rate) {
	nw4r::g3d::ResAnmChr anmChr = this->resFile.GetResAnmChr(name);
	this->anm.bind(&this->bodyModel, anmChr, unk);
	this->bodyModel.bindAnim(&this->anm, unk2);
	this->anm.setUpdateRate(rate);
}

int dThunderCloud::onCreate() {

	// Setup the model
	allocator.link(-1, GameHeaps[0], 0, 0x20);

	this->resFile.data = getResource("tcloud", "g3d/tcloud.brres");
	nw4r::g3d::ResMdl mdl = this->resFile.GetResMdl("cloud");
	bodyModel.setup(mdl, &allocator, 0x224, 1, 0);
	SetupTextures_Enemy(&bodyModel, 0);

	bool ret;
	nw4r::g3d::ResAnmChr anmChr = this->resFile.GetResAnmChr("cloud_wait");
	ret = this->anm.setup(mdl, anmChr, &this->allocator, 0);

	allocator.unlink();



	scale = (Vec){1.2f, 1.2f, 1.2f};

	// Scale and Physics
	ActivePhysics::Info Cloud;
	Cloud.xDistToCenter = 0.0;
	Cloud.yDistToCenter = 0.0;
	Cloud.category1 = 0x3;
	Cloud.category2 = 0x0;
	Cloud.bitfield1 = 0x4F;

	Cloud.bitfield2 = 0xffba7ffe; 
	Cloud.xDistToEdge = 18.0f * scale.x;
	Cloud.yDistToEdge = 12.0f * scale.y;

	Cloud.unkShort1C = 0;
	Cloud.callback = &dEn_c::collisionCallback;

	this->aPhysics.initWithStruct(this, &Cloud);
	this->aPhysics.addToList();

	below.x = 0;
	below.y = 0;
	collMgr.init(this, &below, 0, 0);

	// Some Settings
	this->Baseline = this->pos.y;
	this->dying = -5;
	this->killFlag = 0;
	this->pos.z = 5750.0f; // sun

	stationary 		= this->settings & 0xF;
	
	char eventNum	= (this->settings >> 16) & 0xFF;
	usingEvents = (stationary != 0) && (eventNum != 0);

	this->eventFlag = (u64)1 << (eventNum - 1);


	// State Change!
	if (stationary) { doStateChange(&StateID_Wait); }
	else 			{ doStateChange(&StateID_Follow); }

	this->onExecute();
	return true;
}

int dThunderCloud::onDelete() {
	return true;
}

int dThunderCloud::onExecute() {
	if (scale.x < 1.0f)
		scale.x = scale.y = scale.z = scale.x + 0.0375f;
	else
		scale.x = scale.y = scale.z = 1.0f;

	acState.execute();
	updateModelMatrices();
	bodyModel._vf1C();

	if ((dFlagMgr_c::instance->flags & this->eventFlag) && (!stationary)) {
		if (this->killFlag == 0 && acState.getCurrentState()->isNotEqual(&StateID_DieFall)) {
			this->kill();
			this->pos.y = this->pos.y + 800.0; 
			this->killFlag = 1;
			doStateChange(&StateID_DieFall);
		}
	}
		
	return true;
}

int dThunderCloud::onDraw() {
	bodyModel.scheduleForDrawing();

	return true;
}


void dThunderCloud::updateModelMatrices() {
	// This won't work with wrap because I'm lazy.
	matrix.translation(pos.x, pos.y, pos.z);
	matrix.applyRotationYXZ(&rot.x, &rot.y, &rot.z);

	bodyModel.setDrawMatrix(matrix);
	bodyModel.setScale(&scale);
	bodyModel.calcWorld(false);
}


// Follow State

void dThunderCloud::beginState_Follow() { 
	this->timer = 0;
	this->bindAnimChr_and_setUpdateRate("cloud_wait", 1, 0.0, 1.0);
	this->rot.x = 0;
	this->rot.y = 0;
	this->rot.z = 0;
	PlaySound(this, SE_AMB_THUNDER_CLOUD);
}
void dThunderCloud::executeState_Follow() {

	charge.spawn("Wm_mr_electricshock_biri02_s", 0, &pos, &(S16Vec){0,0,0}, &(Vec){1.5, 1.5, 1.5});

	if(this->anm.isAnimationDone()) {
		this->anm.setCurrentFrame(0.0); }

	if (this->timer > 200) { this->doStateChange(&StateID_Lightning); }

	this->direction = dSprite_c__getXDirectionOfFurthestPlayerRelativeToVEC3(this, this->pos);
	
	float speedDelta;
	speedDelta = 0.05;

	if (this->direction == 0) { // Going Left
		this->speed.x = this->speed.x + speedDelta; // 
		
		if (this->speed.x < 0) { this->speed.x += (speedDelta / 1.5); }
		if (this->speed.x < -6.0) { this->speed.x += (speedDelta * 2.0); }
	}
	else { // Going Right
		this->speed.x = this->speed.x - speedDelta;

		if (this->speed.x > 0) { this->speed.x -= (speedDelta / 1.5); }
		if (this->speed.x > 6.0) { this->speed.x -= (speedDelta * 2.0); }
	}
	
	this->HandleXSpeed();
	
	float yDiff;
	yDiff = (this->Baseline - this->pos.y) / 8;
	this->speed.y = yDiff;
		
	this->HandleYSpeed();

	this->UpdateObjectPosBasedOnSpeedValuesReal();

	this->timer = this->timer + 1;
}
void dThunderCloud::endState_Follow() { 
	this->speed.y = 0;
}


// Wait State

void dThunderCloud::beginState_Wait() { 
	this->timer = 0;
	this->bindAnimChr_and_setUpdateRate("cloud_wait", 1, 0.0, 1.0);
	this->rot.x = 0;
	this->rot.y = 0;
	this->rot.z = 0;
	PlaySound(this, SE_AMB_THUNDER_CLOUD);
}
void dThunderCloud::executeState_Wait() {

	charge.spawn("Wm_mr_electricshock_biri02_s", 0, &pos, &(S16Vec){0,0,0}, &(Vec){1.5, 1.5, 1.5});

	if(this->anm.isAnimationDone()) {
		this->anm.setCurrentFrame(0.0); }

	if ((this->settings >> 16) & 0xFF) {
		if (dFlagMgr_c::instance->flags & this->eventFlag) {
			this->doStateChange(&StateID_Lightning);
		}
	}
	else {
		if (this->timer > 200) { this->doStateChange(&StateID_Lightning); }
		timer += 1;
	}
}
void dThunderCloud::endState_Wait() { }


// Lightning State
static void lightningCallback(ActivePhysics *one, ActivePhysics *two) {
	if (one->owner->name == WM_BUBBLE && two->owner->name == WM_BUBBLE)
		return;
	dEn_c::collisionCallback(one, two);
}

void dThunderCloud::lightningStrike() {
	PlaySound(this, SE_OBJ_KAZAN_ERUPTION); 

	float boltsize = (leader-14.0)/2;
	float boltpos = -boltsize - 14.0;

	ActivePhysics::Info Shock;
	Shock.xDistToCenter = 0.0;
	Shock.yDistToCenter = boltpos;
	Shock.category1 = 0x3;
	Shock.category2 = 0x9;
	Shock.bitfield1 = 0x4D;

	Shock.bitfield2 = 0x420; 
	Shock.xDistToEdge = 12.0;
	Shock.yDistToEdge = boltsize;

	Shock.unkShort1C = 0;
	Shock.callback = &dEn_c::collisionCallback;

	this->Lightning.initWithStruct(this, &Shock);
	this->Lightning.addToList();
}

void dThunderCloud::beginState_Lightning() {
	float backupY = pos.y, backupYSpeed = speed.y;

	speed.x = 0.0;
	speed.y = -1.0f;

	u32 result = 0;
	while (result == 0 && below.y > (-30 << 16)) {
		pos.y = backupY;
		below.y -= 0x4000;
		//OSReport("Sending out leader to %d", below.y>>12);

		result = collMgr.calculateBelowCollisionWithSmokeEffect();
		if (result == 0) {
			u32 tb1 = collMgr.getTileBehaviour1At(pos.x, pos.y + (below.y >> 12), 0);
			if (tb1 & 0x8000 && !(tb1 & 0x20))
				result = 1;
		}
		//OSReport("Result %d", result);
	}

	if (result == 0) {
		OSReport("Couldn't find any ground, falling back to 13 tiles distance");

		leader = 13 * 16;
	} else {
		OSReport("Lightning strikes at %d", below.y>>12);

		leader = -(below.y >> 12);
	}
	below.y = 0;

	pos.y = backupY;
	speed.y = backupYSpeed;

	if (usingEvents) {
		timer = 2;
		this->bindAnimChr_and_setUpdateRate("thundershoot", 1, 0.0, 1.0); 
		lightningStrike();
	} else {
		timer = 0;
	}
}
void dThunderCloud::executeState_Lightning() { 

	switch (timer) {
		case 0:
			charge.spawn("Wm_en_birikyu", 0, &(Vec){pos.x, pos.y, pos.z}, &(S16Vec){0,0,0}, &(Vec){1.5, 1.5, 1.5});
			break;
		case 1:
			charge.spawn("Wm_en_birikyu", 0, &(Vec){pos.x, pos.y, pos.z}, &(S16Vec){0,0,0}, &(Vec){1.5, 1.5, 1.5});
			break;
		case 2:		
			PlaySound(this, SE_BOSS_JR_ELEC_APP); 
			PlaySound(this, SE_BOSS_JR_DAMAGE_ELEC); 

			float boltsize = (leader-14.0)/2;
			float boltpos = -boltsize - 14.0;

			bolt.spawn("Wm_jr_electricline", 0, &(Vec){pos.x, pos.y + boltpos, pos.z}, &(S16Vec){0,0,0}, &(Vec){1.0, boltsize/36.0, 1.0});
			break;
		case 3:
			this->Lightning.removeFromList();
			charge.spawn("Wm_mr_electricshock_biri02_s", 0, &pos, &(S16Vec){0,0,0}, &(Vec){1.5, 1.5, 1.5});
			break;
		case 4:
			charge.spawn("Wm_mr_electricshock_biri02_s", 0, &pos, &(S16Vec){0,0,0}, &(Vec){1.5, 1.5, 1.5});
			break;
		case 5:
			if (stationary) { doStateChange(&StateID_Wait); }
			else 			{ doStateChange(&StateID_Follow); }
			break;
		default:
			charge.spawn("Wm_mr_electricshock_biri02_s", 0, &pos, &(S16Vec){0,0,0}, &(Vec){1.5, 1.5, 1.5});
			break;
	}

	if(this->anm.isAnimationDone() && this->anm.getCurrentFrame() != 0.0) {
		if (timer == 2 && usingEvents) {
			if (dFlagMgr_c::instance->flags & eventFlag) {
			} else {
				this->Lightning.removeFromList();
				doStateChange(&StateID_Wait);
			}
		} else {
			timer++;
			if (timer == 2) { 
				this->bindAnimChr_and_setUpdateRate("thundershoot", 1, 0.0, 1.0); 
				lightningStrike();
			} else if (timer == 3) {
				this->bindAnimChr_and_setUpdateRate("cloud_wait", 1, 0.0, 1.0);
			}
		}
		this->anm.setCurrentFrame(0.0);
	}
}

void dThunderCloud::endState_Lightning() {
	this->timer = 0;
}


// Thundercloud center = 0
// Thundercloud bottom = -12
// Thundercloud boltpos = -boltsize/2 - 14.0
// Thundercloud boltsize = (leader-14.0)/2
// Thundercloud effSize = 36.0 [*2]
// Thundercloud effScale = boltsize / effSize
// Thundercloud effPos = -boltsize/2 - 14.0



//
// processed\../src/makeYourOwnModelSprite.cpp
//

#include <common.h>
#include <game.h>
#include <g3dhax.h>


//////////////////////////////////////////////////////////
//
//	How it works:
//
//		1) Skip down to line 70 - read the comments along the way if you like
//		2) Change the stuff inside " " to be what you want.
//		3) Copy paste an entire 'case' section of code, and change the number to change the setting it uses
//		4) give it back to Tempus to compile in
//



// This is the class allocator, you don't need to touch this
class dMakeYourOwn : public dStageActor_c {
	// Let's give ourselves a few functions
	int onCreate();
	int onDelete();
	int onExecute();
	int onDraw();

	static dMakeYourOwn *build();

	// And a model and an anmChr
	mHeapAllocator_c allocator;
	m3d::mdl_c bodyModel;
	nw4r::g3d::ResFile resFile;
	m3d::anmChr_c chrAnimation;

	nw4r::g3d::ResMdl mdl;

	// Some variables to use
	int model;
	bool isAnimating;
	float size;
	float zOrder;
	bool customZ;

	void setupAnim(const char* name, float rate);
	void setupModel(const char* arcName, const char* brresName, const char* mdlName);
};

// This sets up how much space we have in memory
dMakeYourOwn *dMakeYourOwn::build() {
	void *buffer = AllocFromGameHeap1(sizeof(dMakeYourOwn));
	return new(buffer) dMakeYourOwn;
}


// Saves space when we do it like this
void dMakeYourOwn::setupAnim(const char* name, float rate) {
	if (isAnimating) {
		nw4r::g3d::ResAnmChr anmChr;

		anmChr = this->resFile.GetResAnmChr(name);
		this->chrAnimation.setup(this->mdl, anmChr, &this->allocator, 0);
		this->chrAnimation.bind(&this->bodyModel, anmChr, 1);
		this->bodyModel.bindAnim(&this->chrAnimation, 0.0);
		this->chrAnimation.setUpdateRate(rate);
	}
}

void dMakeYourOwn::setupModel(const char* arcName, const char* brresName, const char* mdlName) {
	this->resFile.data = getResource(arcName, brresName);
	this->mdl = this->resFile.GetResMdl(mdlName);

	bodyModel.setup(mdl, &allocator, 0x224, 1, 0);
}


// This gets run when the sprite spawns!
int dMakeYourOwn::onCreate() {

	// Settings for your sprite!

	this->model = this->settings & 0xFF; 						// Sets nubble 12 to choose the model you want
	this->isAnimating = this->settings & 0x100;					// Sets nybble 11 to a checkbox for whether or not the model has an anmChr to use
	this->size = (float)((this->settings >> 24) & 0xFF) / 4.0; 	// Sets nybbles 5-6 to size. Size equals value / 4.


	float zLevels[16] = {-6500.0, -5000.0, -4500.0, -2000.0, 
						 -1000.0, 300.0, 800.0, 1600.0, 
						  2000.0, 3600.0, 4000.0, 4500.0, 
						  6000.0, 6500.0, 7000.0, 7500.0 };

	this->zOrder = zLevels[(this->settings >> 16) & 0xF];

	this->customZ = (((this->settings >> 16) & 0xF) != 0);

	// Setup the models inside an allocator
	allocator.link(-1, GameHeaps[0], 0, 0x20);


	// Makes the code shorter and clearer to put these up here

	// A switch case, add extra models in here
	switch (this->model) {

		// TITLESCREEN STUFF
		// DEFAULT 

		case 0:		//Red ballon, bobs

			setupModel("arrow", "g3d/bre0.brres", "ballon_red"); 
			SetupTextures_Item(&bodyModel, 0);
			this->pos.z = -3300.0;

			setupAnim("anim00", 1.0); 

			break;	

		case 1:		//Green ballon, bobs

			setupModel("arrow", "g3d/bre1.brres", "ballon_green"); 
			SetupTextures_Item(&bodyModel, 0);
			this->pos.z = 3300.0;

			setupAnim("anim01", 1.0); 
			break;	
			
		case 2:		// Mario, using "wait" with mouth open

			setupModel("arrow", "g3d/bre2.brres", "mario_ts"); 
			SetupTextures_Player(&bodyModel, 0);
			this->pos.z = -3000.0;

			setupAnim("anim02", 1.0); 
			break;	
						
		case 3:		// Peach, custom anim, bobs

			setupModel("arrow", "g3d/bre3.brres", "peach_ts"); 
			SetupTextures_Enemy(&bodyModel, 0);
			this->pos.z = -3000.0;

			setupAnim("anim03", 1.0); 
			break;	

		case 4:		// Luigi with mouth open using "wait", bobs

			setupModel("arrow", "g3d/bre4.brres", "luigi_ts"); 
			SetupTextures_Player(&bodyModel, 0);
			this->pos.z = 3000.0;

			setupAnim("anim04", 1.0); 
			break;	
			
		case 5:	 // Yellow Toad with mouth open, does wait, bobs

			setupModel("arrow", "g3d/bre5.brres", "toady_ts"); 
			SetupTextures_Player(&bodyModel, 0);
			this->pos.z = 3000.0;

			setupAnim("anim05", 1.0); 
			break;	

		case 6:		// Blue Toad with mouth open, bobs head and himself

			setupModel("arrow", "g3d/bre6.brres", "toadb_ts"); 
			SetupTextures_Player(&bodyModel, 0);
			this->pos.z = 3000.0;

			setupAnim("anim06", 1.0); 
			break;	
			
		// BOWSER BEAT TS
		
		case 7:		// Mario's clowncar, bobs, animates propeller

			setupModel("block_arrow", "g3d/bre7.brres", "clowncar_mario"); 
			SetupTextures_MapObj(&bodyModel, 0);
			this->pos.z = 3300.0;

			setupAnim("anim07", 1.0); 
			break;	
			
		case 8:		// Weegee clowncar, bobs, animates propeller, spins

			setupModel("block_arrow", "g3d/bre8.brres", "clowncar_luigi"); 
			SetupTextures_MapObj(&bodyModel, 0);
			this->pos.z = 3300.0;

			setupAnim("anim08", 2.0); 
			break;	
			
		case 9:		// Toad Yellow clowncar, bobs, animates propeller

			setupModel("block_arrow", "g3d/bre9.brres", "clowncar_toady"); 
			SetupTextures_MapObj(&bodyModel, 0);
			this->pos.z = 3300.0;

			setupAnim("anim09", 1.0); 
			break;	
			
		case 10:	// Toad Blue, bobs, animates propeller

			setupModel("block_arrow", "g3d/bre10.brres", "clowncar_toadb"); 
			SetupTextures_MapObj(&bodyModel, 0);
			this->pos.z = 3300.0;

			setupAnim("anim10", 1.0); 
			break;	
		
		case 11:	// Peach clowncar, bobs, animates propeller

			setupModel("block_arrow", "g3d/bre11.brres", "clowncar_peach"); 
			SetupTextures_MapObj(&bodyModel, 0);
			this->pos.z = 3300.0;

			setupAnim("anim11", 1.0); 
			break;	
	
		case 12:	// Mario in a clowncar, bobbing, with fist outstretched.
		
			setupModel("block_arrow", "g3d/bre12.brres", "mario_end"); 
			SetupTextures_Player(&bodyModel, 0);
			this->pos.z = 3000.0;

			setupAnim("anim12", 1.0); 
			break;	
			
		case 13:	// Weegee failing

			setupModel("block_arrow", "g3d/bre13.brres", "luigi_end"); 
			SetupTextures_Player(&bodyModel, 0);
			this->pos.z = 3000.0;

			setupAnim("anim13", 2.0); 
			break;	
			
		case 14:		// Toad Yellow, bobs head, bobs

			setupModel("block_arrow", "g3d/bre14.brres", "toady_end"); 
			SetupTextures_Player(&bodyModel, 0);
			this->pos.z = 3000.0;

			setupAnim("anim14", 1.0);
			break;	
			
		case 15:		// Blue Toad, bobs head, bobs

			setupModel("block_arrow", "g3d/bre15.brres", "toadb_end"); 
			SetupTextures_Player(&bodyModel, 0);
			this->pos.z = 3000.0;

			setupAnim("anim15", 1.0); 
			break;	
			
		case 16:		// Peach laughing, bobbing

			setupModel("block_arrow", "g3d/bre16.brres", "peach_end"); 
			SetupTextures_Enemy(&bodyModel, 0);
			this->pos.z = 3000.0;

			setupAnim("anim16", 1.0); 
			break;	
			
	//PERFECT FILE TS
			
		case 17:		// This is the peach castle backdrop

			setupModel("arrow", "g3d/bre17.brres", "ground_perfect"); 
			SetupTextures_Map(&bodyModel, 0);
			SetupTextures_MapObj(&bodyModel, 0);
			this->pos.z = 3300.0;

			setupAnim("anim17", 1.0); 
			break;	
			
		case 18:		// Mario very small, looking up.

			setupModel("arrow", "g3d/bre18.brres", "mario_perfect"); 
			SetupTextures_Player(&bodyModel, 0);
			this->pos.z = 3300.0;

			setupAnim("anim18", 1.0);
			break;	
			
		case 19:		// Weegee very small, looking up.

			setupModel("arrow", "g3d/bre19.brres", "luigi_perfect"); 
			SetupTextures_Player(&bodyModel, 0);
			this->pos.z = 3000.0;

			setupAnim("anim19", 1.0); 
			break;	
			
		case 20:		// Yellow Toad, very small, looking up.

			setupModel("arrow", "g3d/bre16.brres", "toady_perfect"); 
			SetupTextures_Player(&bodyModel, 0);
			this->pos.z = 3000.0;

			setupAnim("anim20", 1.0); 
			break;	
			
		case 21:		// Blue Toad, very small, looking up.

			setupModel("arrow", "g3d/bre16.brres", "toadb_perfect"); 
			SetupTextures_Player(&bodyModel, 0);
			this->pos.z = 3000.0;

			setupAnim("anim21", 1.0); 
			break;	
			
		case 22:		// I don't think this is used, actually :|

			setupModel("arrow", "g3d/bre22.brres", "peach_perfect"); 
			SetupTextures_Enemy(&bodyModel, 0);
			this->pos.z = 3000.0;

			setupAnim("anim22", 1.0); 
			break;	
			
		case 23:		// I don't think this is used, actually :|

			setupModel("arrow", "g3d/bre23.brres", "backdrop"); 
			SetupTextures_Map(&bodyModel, 0);
			this->pos.z = 3000.0;

			setupAnim("anim23", 1.0); 
			break;	
			
	// A level thing
			
		case 24:		// Small cloud, bobs up and down

			setupModel("arrow", "g3d/bre24.brres", "cloud"); 
			SetupTextures_Item(&bodyModel, 0);
			this->pos.z = -3300.0;

			setupAnim("anim24", 1.0); 

			break;	
	
	// Here begins the ending crap 
	
		case 25:		// Ship fallen, with broken propellers and cannons.

			setupModel("cage_boss_koopa", "g3d/ShipFallen.brres", "KoopaShip"); 
			SetupTextures_MapObj(&bodyModel, 0);
			this->pos.z = 3000.0;

			setupAnim("animation", 1.0);

			break;	
			
		case 26:		// A tree. From the ghost bg.

			setupModel("cage_boss_koopa", "g3d/tree_end.brres", "tree"); 
			SetupTextures_Map(&bodyModel, 0);
			this->pos.z = 3000.0;

			setupAnim("animation", 1.0); 
			
			break;	
			
		case 27:		// Bowser, laying down, eyes closed. Medic? Medic!

			setupModel("cage_boss_koopa", "g3d/bowser_dead.brres", "koopa"); 
			SetupTextures_Boss(&bodyModel, 0);
			this->pos.z = 3000.0;

			setupAnim("animation", 1.0);

			break;	
		
		case 28:		// A car. The animation has it tilted slightly. It's a bit darker than usual.

			setupModel("cage_boss_koopa", "g3d/clown_car_end.brres", "car"); 
			SetupTextures_MapObj(&bodyModel, 0);
			this->pos.z = 3000.0;

			setupAnim("animation", 1.0); 

			break;	
	//CREDITS SHIT
		case 29:		// Mario's clowncar, bobs, animates propeller

			setupModel("kameck_princess", "g3d/bre29.brres", "clowncar_mario"); 
			SetupTextures_MapObj(&bodyModel, 0);
			this->pos.z = 0.0;

			setupAnim("anim29", 1.0); 
			break;	
			
		case 30:		// Weegee clowncar, bobs, animates propeller

			setupModel("kameck_princess", "g3d/bre30.brres", "clowncar_luigi"); 
			SetupTextures_MapObj(&bodyModel, 0);
			this->pos.z = 0.0;

			setupAnim("anim30", 1.0); 
			break;	
			
		case 31:		// Toad Yellow clowncar, bobs, animates propeller

			setupModel("kameck_princess", "g3d/bre31.brres", "clowncar_toady"); 
			SetupTextures_MapObj(&bodyModel, 0);
			this->pos.z = 0.0;

			setupAnim("anim31", 1.0); 
			break;	
			
		case 32:	// Toad Blue, bobs, animates propeller

			setupModel("kameck_princess", "g3d/bre32.brres", "clowncar_toadb"); 
			SetupTextures_MapObj(&bodyModel, 0);
			this->pos.z = 0.0;

			setupAnim("anim32", 1.0); 
			break;	
		
		case 33:	// Peach clowncar, bobs, animates propeller

			setupModel("kameck_princess", "g3d/bre33.brres", "clowncar_peach"); 
			SetupTextures_MapObj(&bodyModel, 0);
			this->pos.z = 0.0;

			setupAnim("anim33", 1.0); 
			break;	
	
		case 34:	// Mario in a clowncar, bobbing, with fist outstretched.
		
			setupModel("kameck_princess", "g3d/bre34.brres", "mario_end"); 
			SetupTextures_Player(&bodyModel, 0);
			this->pos.z = 0;

			setupAnim("anim34", 1.0); 
			break;	
			
		case 35:	// Weegee 

			setupModel("kameck_princess", "g3d/bre35.brres", "luigi_end"); 
			SetupTextures_Player(&bodyModel, 0);
			this->pos.z = 0.0;

			setupAnim("anim35", 1.0); 
			break;	
			
		case 36:		// Toad Yellow, bobs head, bobs

			setupModel("kameck_princess", "g3d/bre36.brres", "toady_end"); 
			SetupTextures_Player(&bodyModel, 0);
			this->pos.z = 0.0;

			setupAnim("anim36", 1.0);
			break;	
			
		case 37:		// Blue Toad, bobs head, bobs

			setupModel("kameck_princess", "g3d/bre37.brres", "toadb_end"); 
			SetupTextures_Player(&bodyModel, 0);
			this->pos.z = 0.0;

			setupAnim("anim37", 1.0); 
			break;	
			
		case 38:		// Peach laughing, bobbing

			setupModel("kameck_princess", "g3d/bre38.brres", "peach_end"); 
			SetupTextures_Enemy(&bodyModel, 0);
			this->pos.z = 0.0;

			setupAnim("anim38", 1.0); 
			break;	
			
		case 39:		// PC Backdrop again

			setupModel("CreditsBG", "g3d/dupa.brres", "ground_perfect"); 
			SetupTextures_Map(&bodyModel, 0);
			this->pos.z = 0.0;

			setupAnim("anim38", 1.0); 
			break;	

		case 40:		// Chestnut Canopy

			setupModel("chestnut", "g3d/canopy.brres", "canopy"); 
			SetupTextures_MapObj(&bodyModel, 0);
			this->pos.z = 0.0;

			// setupAnim("anim38", 1.0); 
			break;	

		case 41:		// Chestnut Canopy

			setupModel("chestnut", "g3d/canopy_1.brres", "canopy_1"); 
			SetupTextures_MapObj(&bodyModel, 0);
			this->pos.z = 0.0;

			// setupAnim("anim38", 1.0); 
			break;	

		case 42:		// Chestnut Canopy

			setupModel("chestnut", "g3d/canopy_2.brres", "canopy_2"); 
			SetupTextures_MapObj(&bodyModel, 0);
			this->pos.z = 0.0;

			// setupAnim("anim38", 1.0); 
			break;	
		
		case 43:		// BallonR

			setupModel("OpeningScene", "g3d/ballon.brres", "ballon_red"); 
			SetupTextures_Item(&bodyModel, 0);
			this->pos.z = 0.0;

				setupAnim("anim", 1.0); 
			break;	
			
		case 44:		// BallonG

			setupModel("OpeningScene", "g3d/ballon2.brres", "ballon_green"); 
			SetupTextures_Item(&bodyModel, 0);
			this->pos.z = 0.0;

				setupAnim("anim", 1.0); 
			break;	
			
		case 45:		// Luigi Opening

			setupModel("OpeningScene", "g3d/weeg.brres", "weeg"); 
			SetupTextures_Player(&bodyModel, 0);
			this->pos.z = 0.0;

				setupAnim("anim", 1.0); 
			break;	
			
		case 46:		// Mario Opening

			setupModel("OpeningScene", "g3d/maleo.brres", "maleo"); 
			SetupTextures_Player(&bodyModel, 0);
			this->pos.z = 0.0;

				setupAnim("anim", 1.0); 
			break;	
			
		case 47:		// ToaB

			setupModel("OpeningScene", "g3d/todb.brres", "todb"); 
			SetupTextures_Player(&bodyModel, 0);
			this->pos.z = 0.0;

				setupAnim("anim", 1.0); 
			break;	

		case 48:		// ToaY

			setupModel("OpeningScene", "g3d/tody.brres", "tody"); 
			SetupTextures_Player(&bodyModel, 0);
			this->pos.z = 0.0;

				setupAnim("anim", 1.0); 
			break;	

		case 49:		// Chestnut Canopy

			setupModel("chestnut", "g3d/canopy_3.brres", "canopy_3"); 
			SetupTextures_MapObj(&bodyModel, 0);
			this->pos.z = 0.0;

			// setupAnim("anim38", 1.0); 
			break;	
	}

	allocator.unlink();

	if (size == 0.0) {	// If the person has the size nybble at zero, make it normal sized
		this->scale = (Vec){1.0,1.0,1.0};	
	}
	else {				// Else, use our size
		this->scale = (Vec){size,size,size};	
	}
		
	this->onExecute();
	return true;
}


// YOU'RE DONE, no need to do anything below here.


int dMakeYourOwn::onDelete() {
	return true;
}

int dMakeYourOwn::onExecute() {
	if (isAnimating) {
		bodyModel._vf1C();	// Advances the animation one update

		if(this->chrAnimation.isAnimationDone()) {
			this->chrAnimation.setCurrentFrame(0.0);	// Resets the animation when it's done
		}
	}

	return true;
}

int dMakeYourOwn::onDraw() {
	if (customZ) {
		matrix.translation(pos.x, pos.y, this->zOrder); }	// Set where to draw the model : -5500.0 is the official behind layer 2, while 5500.0 is in front of layer 0.
	else {
		matrix.translation(pos.x, pos.y, pos.z - 6500.0); }	// Set where to draw the model : -5500.0 is the official behind layer 2, while 5500.0 is in front of layer 0.

	matrix.applyRotationYXZ(&rot.x, &rot.y, &rot.z);	// Set how to rotate the drawn model 

	bodyModel.setDrawMatrix(matrix);	// Apply matrix
	bodyModel.setScale(&scale);			// Apply scale
	bodyModel.calcWorld(true);			// Do some shit

	bodyModel.scheduleForDrawing();		// Add it to the draw list for the game
	return true;
}

