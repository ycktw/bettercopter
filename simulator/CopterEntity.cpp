#include "Application.h"
#include "CopterEntity.h"
#include "kernel.h"

CopterIMU::CopterIMU(btDynamicsWorld *world, btRigidBody *body):mBody(body){
	World = world; 
}

glm::quat CopterIMU::getOrientation(){
	const btQuaternion& quat = mBody->getOrientation();
	return glm::quat(quat.w(), quat.x(), quat.y(), quat.z()); 
}

glm::vec3 CopterIMU::getPosition(){
	btVector3 pos = mBody->getCenterOfMassPosition();
	return glm::vec3(pos[0], pos[1], pos[2]); 
}

glm::vec3 CopterIMU::getAngularVelocity(){
	glm::quat rot = getOrientation();
	glm::quat rot_inv = glm::inverse(rot);
	
	btVector3 v = mBody->getAngularVelocity();
	glm::vec3 vel(v[0], v[1], v[2]); 

	return rot_inv * vel; 
	//btVector3 v = mBody->getAngularVelocity();
	//return glm::vec3(v[0], v[1], v[2]); 
}

glm::vec3 CopterIMU::getLinearVelocity(){
	btVector3 v = mBody->getLinearVelocity();
	return glm::vec3(v[0], v[1], v[2]); 
}

bool CopterIMU::RaycastWorld(const btVector3 &Start, btVector3 &End, btVector3 &Normal) {
	btCollisionWorld::ClosestRayResultCallback RayCallback(Start, End);
	//RayCallback.m_collisionFilterMask = FILTER_CAMERA;

	// Perform raycast
	World->rayTest(Start, End, RayCallback);
	if(RayCallback.hasHit()) {
		End = RayCallback.m_hitPointWorld;
		Normal = RayCallback.m_hitNormalWorld;
		return true;
	}
	return false;
}

void CopterIMU::update(float dt){
	if(!dt) dt = 1; // at least one millisecond

	// measure distance straight down 
	btVector3 end, normal; 
	btVector3 pos = mBody->getCenterOfMassPosition();
	const btQuaternion& quat = mBody->getOrientation(); //mBody->getWorldTransform().getRotation(); 
	btVector3 down = btTransform(quat) * btVector3(0, -1, 0); 
	btVector3 gravity = World->getGravity();
	btVector3 velocity = mBody->getLinearVelocity();
	btVector3 tmp;
	
	if(RaycastWorld(down, end, normal)){
		mDistance = (end-pos).length() * 1000;
	} else {
		mDistance = -1;
	}

	mAltitude = pos[1] * 100;
	//glm::vec3 euler = glm::eulerAngles()); 
	
	//printf("EULER: %f %f %f\n", glm::degrees(euler.x), 
	//	glm::degrees(euler.y), glm::degrees(euler.z)); 
	
	btTransform t; 
	mBody->getMotionState()->getWorldTransform(t); 
	btMatrix3x3 trans = t.getBasis().inverse(); 
	btVector3 up = (trans * btVector3(0, 1, 0)); 
	
	mAcc = glm::vec3(up[0], up[2], up[1]);
	mOldVelocity = velocity;

	tmp = mBody->getAngularVelocity(); 
	mGyro = glm::vec3(glm::degrees(tmp[0]), 
		glm::degrees(tmp[1]), glm::degrees(tmp[2]));

	//tmp = (trans * btVector3(0, 0, 1)); 
	//mCompass = glm::vec3(1, 0, 0); //glm::vec3(tmp[0] * 32768, tmp[1] * 32768, tmp[2] * 32768); 
}

CopterEntity::CopterEntity(Application *app){
	float mass = 1000;
	mApp = app;
	
	// Create a box rigid body
	ISceneManager *irrScene = mApp->irrScene;
	
	ISceneNode *Node = irrScene->addCubeSceneNode(1.0f);
	Node->setScale(vector3df(1, 0.1, 1));
	Node->setMaterialFlag(EMF_LIGHTING, 1);
	Node->setMaterialFlag(EMF_NORMALIZE_NORMALS, true);
	Node->setMaterialTexture(0, mApp->irrDriver->getTexture("rust0.jpg"));
	mNode = Node;
	
	glm::mat4 pmat(
		glm::vec4(0.5, 0.15, 0.5, 0.0),
		glm::vec4(-0.5, 0.15, 0.5, 0.0),
		glm::vec4(0.5, 0.15, -0.5, 0.0),
		glm::vec4(-0.5, 0.15, -0.5, 0.0));
	for(int c = 0; c < 4; c++){
		glm::vec4 pos = pmat[c];
		ISceneNode *pn = irrScene->addCubeSceneNode(1.0f);
		pn->setPosition(vector3df(pos.x, pos.y, pos.z)); 
		pn->setScale(vector3df(0.5, 1.0, 0.5));
		pn->setMaterialFlag(EMF_LIGHTING, 1);
		pn->setMaterialFlag(EMF_NORMALIZE_NORMALS, true);
		pn->setMaterialTexture(0, mApp->irrDriver->getTexture("rust0.jpg"));
		Node->addChild(pn); 
	}
	// Set the initial position of the object
	btTransform Transform;
	Transform.setIdentity();
	Transform.setOrigin(btVector3(0, 10, 0));

	btDefaultMotionState *MotionState = new btDefaultMotionState(Transform);

	// Create the shape
	btVector3 HalfExtents(0.5f, 0.05f, 0.5f);
	btCollisionShape *Shape = new btBoxShape(HalfExtents);
	
	// Add mass
	btVector3 LocalInertia;
	Shape->calculateLocalInertia(mass, LocalInertia);

	// Create the rigid body object
	btRigidBody::btRigidBodyConstructionInfo info(mass, MotionState, Shape, LocalInertia); //motion state would actually be non-null in most real usages
	info.m_restitution = 4000.0f;
	info.m_friction = 1.5f;

	btRigidBody *RigidBody = new btRigidBody(info);
	RigidBody->setActivationState(DISABLE_DEACTIVATION);
	
	// Store a pointer to the irrlicht node so we can update it later
	RigidBody->setUserPointer((void *)(Node));

	// Add it to the world
	mApp->World->addRigidBody(RigidBody);
	mApp->Objects.push_back(RigidBody);

	mBody = RigidBody;

	mCamera = 0; 
	mSensors = new CopterIMU(mApp->World, RigidBody); 
	//FlightController::setSensorProvider(new CopterIMU(mApp->World, RigidBody));
}

void CopterEntity::render(irr::video::IVideoDriver *drv){
	//mBody->getMotionState()->getWorldTransform(trans);
	btTransform trans; 
	mBody->getMotionState()->getWorldTransform(trans); 
	btQuaternion qrot = trans.getRotation(); 
	btMatrix3x3 mat = trans.getBasis(); 
	btVector3 p = trans.getOrigin(); 
	glm::quat rot = glm::quat(qrot[0], qrot[1], qrot[2], qrot[3]);
	glm::vec3 pos = glm::vec3(p[0], p[1], p[2]);

	drv->setTransform(irr::video::ETS_WORLD, irr::core::IdentityMatrix);

	glm::vec3 front = rot * glm::vec3(0.0, 0.0, 1.0); 
	drv->draw3DLine(vector3df(pos.x, pos.y, pos.z),
		vector3df(pos.x + front.x, pos.y + front.y, pos.z + front.z), 
		SColor( 0, 255, 0, 0 ));
	
	//glm::vec3 front = rot * glm::vec3(0.0, 0.0, 1.0); 
	glm::vec3 nac = glm::normalize(mSensors->getAccelerometer()); 
	btVector3 acc = (mat * btVector3(nac.x, nac.y, nac.z)); 
	drv->draw3DLine(vector3df(pos.x, pos.y, pos.z),
		vector3df(pos.x + acc.x(), pos.y + acc.y(), pos.z + acc.z()), 
		SColor( 255, 255, 0, 0 ));
	/*drv->draw3DLine(vector3df(pos.x, pos.y, pos.z),
		vector3df(pos.x, pos.y + 10, pos.z), 
		SColor( 255, 255, 0, 0 ));*/
	i16vec4 ithrottle; // = FlightController::getMotorThrust();;
	ithrottle[0] = get_pin(PWM_FRONT); 
	ithrottle[2] = get_pin(PWM_BACK); 
	ithrottle[1] = get_pin(PWM_RIGHT); 
	ithrottle[3] = get_pin(PWM_LEFT); 
	glm::vec4 F = glm::vec4(ithrottle) / glm::vec4(5000.0);
	
	btTransform tr = btTransform(qrot); 
	
	btVector3 PX = tr * btVector3(0, 0.05, -0.5) + btVector3(pos.x, pos.y, pos.z);
	btVector3 PY = tr * btVector3(0, 0.05, 0.5) + btVector3(pos.x, pos.y, pos.z);
	btVector3 PZ = tr * btVector3(-0.5, 0.05,0) + btVector3(pos.x, pos.y, pos.z);
	btVector3 PW = tr * btVector3(0.5, 0.05,0) + btVector3(pos.x, pos.y, pos.z);
	
	btVector3 TX = tr * btVector3(0, F.x, 0);
	btVector3 TY = tr * btVector3(0, F.y, 0);
	btVector3 TZ = tr * btVector3(0, F.z, 0);
	btVector3 TW = tr * btVector3(0, F.w, 0);

	btVector3 OX = tr * btVector3(0, 0, -F.x);
	btVector3 OY = tr * btVector3(0, 0, F.y);
	btVector3 OZ = tr * btVector3(-F.z, 0, 0);
	btVector3 OW = tr * btVector3(F.w, 0, 0);

	drv->draw3DLine(vector3df(PX[0], PX[1], PX[2]),
		vector3df(PX[0] + TX[0], PX[1] + TX[1], PX[2] + TX[2]));
	drv->draw3DLine(vector3df(PY[0], PY[1], PY[2]),
		vector3df(PY[0] + TY[0], PY[1] + TY[1], PY[2] + TY[2]));
	drv->draw3DLine(vector3df(PZ[0], PZ[1], PZ[2]),
		vector3df(PZ[0] + TZ[0], PZ[1] + TZ[1], PZ[2] + TZ[2]));
	drv->draw3DLine(vector3df(PW[0], PW[1], PW[2]),
		vector3df(PW[0] + TW[0], PW[1] + TW[1], PW[2] + TW[2]));

	drv->draw3DLine(vector3df(PX[0], PX[1], PX[2]),
		vector3df(PX[0] + OX[0], PX[1] + OX[1], PX[2] + OX[2]));
	drv->draw3DLine(vector3df(PY[0], PY[1], PY[2]),
		vector3df(PY[0] + OY[0], PY[1] + OY[1], PY[2] + OY[2]));
	drv->draw3DLine(vector3df(PZ[0], PZ[1], PZ[2]),
		vector3df(PZ[0] + OZ[0], PZ[1] + OZ[1], PZ[2] + OZ[2]));
	drv->draw3DLine(vector3df(PW[0], PW[1], PW[2]),
		vector3df(PW[0] + OW[0], PW[1] + OW[1], PW[2] + OW[2]));

	/*
	btVector3 a = mBody->getAngularVelocity(); 
	glm::vec3 vel = glm::vec3(a[0], a[1], a[2]) * glm::vec3(10.0);
	drv->draw3DLine(vector3df(pos.x, pos.y, pos.z),
		vector3df(pos.x + vel.x, pos.y + vel.y, pos.z + vel.z));*/
}

void CopterEntity::update(
		int16_t thr, 
		int16_t yaw, 
		int16_t pitch, 
		int16_t roll, 
		int16_t mode, uint32_t _dt){
	float dt = _dt * 0.001; 
	
	//float dt = (float)idt / MAX_UIVALUE;
	//printf("DT: %f, RC: Thr: %05d, Yaw: %05d, Pitch: %05d, Roll: %05d\n", dt, thr, yaw, pitch, roll); 
	
	mSensors->update(dt); 
	//FlightController::update(thr, yaw, pitch, roll, 0, _dt);
	
	i16vec4 ithrottle; //FlightController::getMotorThrust();
	ithrottle[0] = get_pin(PWM_FRONT); 
	ithrottle[1] = get_pin(PWM_BACK); 
	ithrottle[2] = get_pin(PWM_RIGHT); 
	ithrottle[3] = get_pin(PWM_LEFT); 
	
	//printf("Thrust: %05d %05d %05d %05d\n", 
	//	ithrottle[0], ithrottle[1], ithrottle[2], ithrottle[3]); 
		
	glm::vec4 F = (glm::vec4(ithrottle) * dt) * 100.0f;
	
	//btQuaternion tmp = mBody->getOrientation(); 
	//glm::quat rot = glm::quat(tmp[0], tmp[1], tmp[2], tmp[3]) ;
	btTransform trans; 
	mBody->getMotionState()->getWorldTransform(trans); 
	btMatrix3x3 tr = trans.getBasis(); 
	//btTransform tr(qrot);
	
	//glm::vec3 up = -glm::normalize(glm::vec3(mSensors->readAccelerometer())); 
	//glm::vec3 up = glm::vec3(0, 1, 0);
	
	btVector3 PX = tr * btVector3( 0.0, 0.05,-1.0);
	btVector3 PY = tr * btVector3( 0.0, 0.05, 1.0);
	btVector3 PZ = tr * btVector3(-1.0, 0.05, 0.0);
	btVector3 PW = tr * btVector3( 1.0, 0.05, 0.0);
	
	btVector3 TX = tr * btVector3(0, F.x, 0);
	btVector3 TY = tr * btVector3(0, F.y, 0);
	btVector3 TZ = tr * btVector3(0, F.z, 0);
	btVector3 TW = tr * btVector3(0, F.w, 0);

	btVector3 OX = tr * btVector3(-F.x, 0,0 );
	btVector3 OY = tr * btVector3(F.y, 0, 0);
	btVector3 OZ = tr * btVector3(0, 0, -F.z);
	btVector3 OW = tr * btVector3(0, 0, F.w);
	
	// throttle
	mBody->applyForce(TX, PX); 
	mBody->applyForce(TY, PY); 
	mBody->applyForce(TZ, PZ); 
	mBody->applyForce(TW, PW);

	// force from the motors sideways
	mBody->applyForce(OX, PX); 
	mBody->applyForce(OY, PY); 
	mBody->applyForce(OZ, PZ); 
	mBody->applyForce(OW, PW);

	//mBody->getWorldTransform().setOrigin(btVector3(0, 2, 0)); 
	if(mCamera){
		vector3df Euler;
		const btQuaternion& TQuat = mBody->getOrientation();
		btVector3 pos = mBody->getCenterOfMassPosition();
		quaternion q(TQuat.getX(), TQuat.getY(), TQuat.getZ(), TQuat.getW());
		btVector3 look = btTransform(TQuat) * btVector3(0, 0, 1) + pos; 
		q.toEuler(Euler);
		Euler *= RADTODEG;
		Euler.X = -45;
		//printf("EULER: %f %f %f\n", Euler.X, Euler.Y, Euler.Z); 
		mCamera->setTarget(vector3df(look[0], look[1], look[2]));
		//mCamera->setPosition(vector3df(pos[0], pos[1], pos[2])); 
	}
/*
	char str[255];
	sprintf(str, "F: %f %f %f %f, DIST: %d\n", F.x, F.y, F.z, F.w, mSensors->readDistance()); 
	stringw s = str;
	irrStatusText->setText(s.c_str()); */
}
