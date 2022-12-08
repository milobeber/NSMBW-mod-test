#include <common.h>
#include <game.h>
#include <g3dhax.h>
#include <sfx.h>
#include <stage.h>
#include "boss.h"


const char* CParcNameList [] = {
	"pakkun",
	NULL	
};

// Moving (Bouncing) Piranha Plant Settings
// 
// Nybble 5: Direction Moved
//		0 - Horizontal	
//		1 - Vertical
//		2 - No Movement
//
// Nybble 6: Distance Moved
//		# - Distance that the Piranha Moves
//
// Nybble 7: Movement Speed
//		10- Slow++
//    	9 - Slow+
//		8 - Slow
// 	  	7 - Slow/Medium
//    	6 - Medium/Slow
//    	5 - Medium/Fast
//    	4 - Fast/Medium
//    	3 - Fast
//    	2 - Fast+
//    	1 - >Fast++
//
// Nybble 12.4: Bouncy
//	  	0 - False
//	  	1 - True

class dacustompakkun : public dEn_c {
	int onCreate();
	int onDelete();
	int onExecute();
	int onDraw();

	mHeapAllocator_c allocator;
	nw4r::g3d::ResFile resFile;
	m3d::mdl_c bodyModel;
	m3d::anmChr_c animationChr;
	m3d::anmChr_c chrAnimation;

	int timer;
	bool isDieing;

	char damage;
	int type;

	u8 moveDirection, moveSpeed, moveDistance, bounce;

	static dacustompakkun *build();

	void bindAnimChr_and_setUpdateRate(const char* name, int unk, float unk2, float rate);
	void setupBodyModel();
	void updateModelMatrices();

	void playerCollision(ActivePhysics *apThis, ActivePhysics *apOther);
	void yoshiCollision(ActivePhysics *apThis, ActivePhysics *apOther);

	bool collisionCat1_Fireball_E_Explosion(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat2_IceBall_15_YoshiIce(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat7_GroundPound(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat7_GroundPoundYoshi(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat9_RollingObject(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat13_Hammer(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCat14_YoshiFire(ActivePhysics *apThis, ActivePhysics *apOther);
	bool collisionCatA_PenguinMario(ActivePhysics *apThis, ActivePhysics *apOther);

	void _vf148();
	void _vf14C();
	bool CreateIceActors();

	USING_STATES(dacustompakkun);
	DECLARE_STATE(Attack);
	DECLARE_STATE(Outro);
};

dacustompakkun *dacustompakkun::build() {
	void *buffer = AllocFromGameHeap1(sizeof(dacustompakkun));
	return new(buffer) dacustompakkun;
}



CREATE_STATE(dacustompakkun, Attack);
CREATE_STATE(dacustompakkun, Outro);



void dacustompakkun::playerCollision(ActivePhysics *apThis, ActivePhysics *apOther) { 

	char hitType;
	hitType = usedForDeterminingStatePress_or_playerCollision(this, apThis, apOther, 0);
	dStageActor_c *player = apOther->owner;
	if(hitType == 1 || hitType == 3) {
		if(bounce == 1) {
			bouncePlayer(player, 8.0f);
	}
	if(hitType == 0) {
		this->_vf220(apOther->owner);
	}
}
void dacustompakkun::yoshiCollision(ActivePhysics *apThis, ActivePhysics *apOther) {
    	char hitType;
	hitType = usedForDeterminingStatePress_or_playerCollision(this, apThis, apOther, 0);
	dStageActor_c *player = apOther->owner;
	if(hitType == 1 || hitType == 3) {
		bouncePlayer(player, 8.0f);
	}
	if(hitType == 0) {
		this->_vf220(apOther->owner);
	}
}

bool dacustompakkun::collisionCat1_Fireball_E_Explosion(ActivePhysics *apThis, ActivePhysics *apOther) {
	this->damage += 1;

	if (this->damage > 1) {
			nw4r::snd::SoundHandle handle;
			PlaySoundWithFunctionB4(SoundRelatedClass, &handle, SE_EMY_DOWN, 1);
			StageE4::instance->spawnCoinJump(pos, 0, 1, 0);
			doStateChange(&StateID_Outro);
	}
	else {
			nw4r::snd::SoundHandle handle;
			PlaySoundWithFunctionB4(SoundRelatedClass, &handle, SE_EMY_KURIBO_L_DAMAGE_01, 1);
			doStateChange(&StateID_Attack);
	}
	doStateChange(&StateID_Outro);
	return true;
}
bool dacustompakkun::collisionCat7_GroundPound(ActivePhysics *apThis, ActivePhysics *apOther) {
	doStateChange(&StateID_Outro);
	return true;
}
bool dacustompakkun::collisionCat7_GroundPoundYoshi(ActivePhysics *apThis, ActivePhysics *apOther) {
	doStateChange(&StateID_Outro);
	return true;
}
bool dacustompakkun::collisionCat9_RollingObject(ActivePhysics *apThis, ActivePhysics *apOther) {
	doStateChange(&StateID_Outro);
	return true;
}
bool dacustompakkun::collisionCat13_Hammer(ActivePhysics *apThis, ActivePhysics *apOther) {
	doStateChange(&StateID_Outro);
	return true;
}

bool dacustompakkun::collisionCat2_IceBall_15_YoshiIce(ActivePhysics *apThis, ActivePhysics *apOther) {
	return false;
}
bool dacustompakkun::collisionCat14_YoshiFire(ActivePhysics *apThis, ActivePhysics *apOther) {
	doStateChange(&StateID_Outro);
	return true;
}
bool dacustompakkun::collisionCatA_PenguinMario(ActivePhysics *apThis, ActivePhysics *apOther) {
	this->_vf220(apOther->owner);
	return true;
}

void dacustompakkun::_vf148() {
	this->damage += 1;

	if (this->damage > 1) {
			nw4r::snd::SoundHandle handle;
			PlaySoundWithFunctionB4(SoundRelatedClass, &handle, SE_EMY_DOWN, 1);
			StageE4::instance->spawnCoinJump(pos, 0, 1, 0);
			doStateChange(&StateID_Outro);
	}
	else {
			nw4r::snd::SoundHandle handle;
			PlaySoundWithFunctionB4(SoundRelatedClass, &handle, SE_EMY_KURIBO_L_DAMAGE_01, 1);
			doStateChange(&StateID_Attack);
	}
	doStateChange(&StateID_Outro);
}

void dacustompakkun::_vf14C() {
	this->damage += 1;

	if (this->damage > 1) {
			nw4r::snd::SoundHandle handle;
			PlaySoundWithFunctionB4(SoundRelatedClass, &handle, SE_EMY_DOWN, 1);
			StageE4::instance->spawnCoinJump(pos, 0, 1, 0);
			doStateChange(&StateID_Outro);
	}
	else {
			nw4r::snd::SoundHandle handle;
			PlaySoundWithFunctionB4(SoundRelatedClass, &handle, SE_EMY_KURIBO_L_DAMAGE_01, 1);
			doStateChange(&StateID_Attack);
	}
	doStateChange(&StateID_Outro);
}

void dacustompakkun::bindAnimChr_and_setUpdateRate(const char* name, int unk, float unk2, float rate) {
	nw4r::g3d::ResAnmChr anmChr = this->resFile.GetResAnmChr(name);
	this->animationChr.bind(&this->bodyModel, anmChr, unk);
	this->bodyModel.bindAnim(&this->animationChr, unk2);
	this->animationChr.setUpdateRate(rate);
}


void dacustompakkun::setupBodyModel() {
	allocator.link(-1, GameHeaps[0], 0, 0x20);

	this->resFile.data = getResource("pakkun", "g3d/pakkun.brres");
	nw4r::g3d::ResMdl mdl = this->resFile.GetResMdl("pakkun");
	bodyModel.setup(mdl, &allocator, 0x224, 1, 0);
	SetupTextures_Enemy(&bodyModel, 0);

	bool ret;
	nw4r::g3d::ResAnmChr anmChr = this->resFile.GetResAnmChr("attack");
	ret = this->animationChr.setup(mdl, anmChr, &this->allocator, 0);

	allocator.unlink();
}


int dacustompakkun::onCreate() {

	this->moveDirection = this->settings >> 28 & 0xF;
	this->moveSpeed = this->settings >> 20 & 0xF;
	this->moveDistance = this->settings >> 4 & 0x1;
	this->bounce = this->settings >> 0xF;
	
	setupBodyModel();


	this->scale = (Vec){1.0, 1.0, 1.0};
	this->rot.x = 0; // X is vertical axis
	this->rot.y = 0xD800; // Y is horizontal axis
	this->rot.z = 0; // Z is ... an axis >.>
	this->direction = 1; // Heading left.

	ActivePhysics::Info HitMeBaby;
	
	HitMeBaby.xDistToCenter = 0.0;
	HitMeBaby.yDistToCenter = 0.0;

	HitMeBaby.xDistToEdge = 15.0;
	HitMeBaby.yDistToEdge = 15.0; 

	HitMeBaby.category1 = 0x3;
	HitMeBaby.category2 = 0x0;
	HitMeBaby.bitfield1 = 0x4F;
	HitMeBaby.bitfield2 = 0x8028E;
	HitMeBaby.unkShort1C = 0;
	HitMeBaby.callback = &dEn_c::collisionCallback;

	this->aPhysics.initWithStruct(this, &HitMeBaby);
	this->aPhysics.addToList();


	bindAnimChr_and_setUpdateRate("attack", 1, 0.0, 1.0);

	doStateChange(&StateID_Attack);

	this->onExecute();
	return true;
}

int dacustompakkun::onDelete() {
	return true;
}

int dacustompakkun::onExecute() {
	acState.execute();
	updateModelMatrices();

	if(this->animationChr.isAnimationDone()) {
		this->animationChr.setCurrentFrame(0.0);
	}

	return true;
}


int dacustompakkun::onDraw() {
	bodyModel.scheduleForDrawing();
	bodyModel._vf1C();
	return true;
}


void dacustompakkun::updateModelMatrices() {
	matrix.translation(pos.x, pos.y, pos.z);
	matrix.applyRotationYXZ(&rot.x, &rot.y, &rot.z);

	bodyModel.setDrawMatrix(matrix);
	bodyModel.setScale(&scale);
	bodyModel.calcWorld(false);
}


// Attack State

void dacustompakkun::beginState_Attack() {

}
void dacustompakkun::executeState_Attack() {
	this->timer++;
	
	if(this->moveDirection == 0) {
		if(this->timer < moveSpeed*5) {
		this-> pos.x += moveDistance;
		}
		if(this->timer > moveSpeed*5 && this->timer < moveSpeed*10) {
		this->pos.x -= moveDistance;
		}
		if(this->timer == moveSpeed*10) {
		this->timer = 0;
		}
	}
	else if(this->moveDirection == 1) {
		if(this->timer < moveSpeed*5) {
		this-> pos.y += moveDistance;
		}
		if(this->timer > moveSpeed*5 && this->timer < moveSpeed*10) {
		this->pos.y -= moveDistance;
		}
		if(this->timer = moveSpeed*10) {
		this->timer = 0;
		}
	}
	else if(this-> == 2) {

	}
}
void dacustompakkun::endState_Attack() {

}

// Outro State

void dacustompakkun::beginState_Outro() {
	bindAnimChr_and_setUpdateRate("dead", 1, 0.0, 1.0);
	this->removeMyActivePhysics();
}
void dacustompakkun::executeState_Outro() {
	if(this->animationChr.isAnimationDone()) {
		this->Delete(1);
	}

}
void dacustompakkun::endState_Outro() { 

}





