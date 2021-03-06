#include "RigidBodyDyn_vortex.h"
#include "CollShapeDyn_vortex.h"
#include "v_repLib.h"
#include "Vx/VxPart.h"
#include "Vx/VxUniverse.h"
#include "Vx/VxMassProperties.h"
#include "Vx/VxCompositeCollisionGeometry.h"
#include "Vx/VxTransform.h"
#include "Vx/VxQuaternion.h"
#include "Vx/VxFrictionDirection.h"
#include "Vx/VxMessage.h"
#include "Vx/VxFrame.h"
#include "Vx/VxIntersectFilter.h"
#include "VortexConvertUtil.h"

Vx::VxIntersectFilter* CRigidBodyDyn_vortex::_sVortexIntersectFilter = NULL;
/* Returns true if the friction is same along/around primary and secondary axes.
 **/
static bool isIsotropicMaterial(Vx::VxCollisionGeometry* vortexGeomID)
{
    bool isotropicMaterial = false;
    Vx::VxMaterial* m = vortexGeomID->getMaterial();

    if (m == NULL)
    {
        Vx::VxCompositeCollisionGeometry* ccg = dynamic_cast<Vx::VxCompositeCollisionGeometry*>(vortexGeomID);
        if (ccg && ccg->getCollisionGeometries().size() > 0)
        {
            m = (*ccg->getCollisionGeometries().begin())->getMaterial();
        }
    }

    if (m && areEquals(m->getSlide(Vx::VxMaterial::kFrictionAxisLinearPrimary), m->getSlide(Vx::VxMaterial::kFrictionAxisLinearSecondary), 0.00001) &&
        m->getFrictionModel(Vx::VxMaterial::kFrictionAxisLinearPrimary) == m->getFrictionModel(Vx::VxMaterial::kFrictionAxisLinearSecondary) )
    {
        switch (m->getFrictionModel(Vx::VxMaterial::kFrictionAxisLinearPrimary))
        {
        case Vx::VxMaterial::kFrictionModelBox:
        case Vx::VxMaterial::kFrictionModelBoxProportionalHigh:
        case Vx::VxMaterial::kFrictionModelBoxProportionalLow:
            if (areEquals(m->getBoxFrictionForce(Vx::VxMaterial::kFrictionAxisLinearPrimary), m->getBoxFrictionForce(Vx::VxMaterial::kFrictionAxisLinearSecondary), 0.001))
            {
                isotropicMaterial = true;
            }
            break;
        case Vx::VxMaterial::kFrictionModelScaledBox:
        case Vx::VxMaterial::kFrictionModelScaledBoxFast:
            if (areEquals(m->getFrictionCoefficient(Vx::VxMaterial::kFrictionAxisLinearPrimary), m->getFrictionCoefficient(Vx::VxMaterial::kFrictionAxisLinearSecondary), 0.001))
            {
                isotropicMaterial = true;
            }
            break;
        default:
            // isotropicMaterial remains false
            break;
        }
    }
    if (isotropicMaterial)
    {
        // check the angular domain as well
        if (m->getFrictionModel(Vx::VxMaterial::kFrictionAxisAngularPrimary) == m->getFrictionModel(Vx::VxMaterial::kFrictionAxisAngularSecondary) )
        {
            switch (m->getFrictionModel(Vx::VxMaterial::kFrictionAxisAngularPrimary))
            {
            case Vx::VxMaterial::kFrictionModelBox:
            case Vx::VxMaterial::kFrictionModelBoxProportionalHigh:
            case Vx::VxMaterial::kFrictionModelBoxProportionalLow:
                if (!areEquals(m->getBoxFrictionForce(Vx::VxMaterial::kFrictionAxisAngularPrimary), m->getBoxFrictionForce(Vx::VxMaterial::kFrictionAxisAngularSecondary), 0.001))
                {
                    isotropicMaterial = false;
                }
                break;
            case Vx::VxMaterial::kFrictionModelScaledBox:
            case Vx::VxMaterial::kFrictionModelScaledBoxFast:
                if (!areEquals(m->getFrictionCoefficient(Vx::VxMaterial::kFrictionAxisAngularPrimary), m->getFrictionCoefficient(Vx::VxMaterial::kFrictionAxisAngularSecondary), 0.001))
                {
                    isotropicMaterial = false;
                }
                break;
            default:
                // isotropicMaterial remains true
                break;
            }
        }
        else
        {
            isotropicMaterial = false;
        }
    }
    return isotropicMaterial;
}

CRigidBodyDyn_vortex::CRigidBodyDyn_vortex(CDummyShape* shape,CCollShapeDyn* collShapeDyn,bool forceStatic,bool forceNonRespondable,Vx::VxUniverse* vortexWorld,int aRandomValue)
{ // Constructor for a rigid body. The collision shape (wrapper) was already created.
    _rigidBodyID=-1;
    _shapeID=_simGetObjectID(shape);
    _shape=shape;
    _collisionShapeDyn=collShapeDyn;
    _simGetObjectLocalTransformation(shape,_originalLocalTransformation.X.data,_originalLocalTransformation.Q.data,false); // needed for the "parent follows"-thing!

    // Following parameter retrieval is OLD. Use instead following functions:
    // - simGetEngineFloatParameter
    // - simGetEngineInt32Parameter
    // - simGetEngineBoolParameter
    float floatParams[36];
    int intParams[8];
    _simGetVortexParameters(shape,3,floatParams,intParams);

//  const double frictionCoeff_primary_linearAxis=getVortexUnsignedDouble(floatParams[0]);
//  const double frictionCoeff_secondary_linearAxis=getVortexUnsignedDouble(floatParams[1]);
//  const double frictionCoeff_primary_angularAxis=getVortexUnsignedDouble(floatParams[2]);
//  const double frictionCoeff_secondary_angularAxis=getVortexUnsignedDouble(floatParams[3]);
//  const double frictionCoeff_normal_angularAxis=getVortexUnsignedDouble(floatParams[4]);
//  const double staticFrictionScale_primary_linearAxis=getVortexUnsignedDouble(floatParams[5]);
//  const double staticFrictionScale_secondary_linearAxis=getVortexUnsignedDouble(floatParams[6]);
//  const double staticFrictionScale_primary_angularAxis=getVortexUnsignedDouble(floatParams[7]);
//  const double staticFrictionScale_secondary_angularAxis=getVortexUnsignedDouble(floatParams[8]);
//  const double staticFrictionScale_normal_angularAxis=getVortexUnsignedDouble(floatParams[9]);
//  const double compliance=getVortexUnsignedDouble(floatParams[10]);
//  const double damping=getVortexUnsignedDouble(floatParams[11]);
//  const double restitution=getVortexUnsignedDouble(floatParams[12]);
//  const double restitutionThreshold=getVortexUnsignedDouble(floatParams[13]);
//  const double adhesiveForce=getVortexUnsignedDouble(floatParams[14]);
    const double linearVelocityDamping=getVortexUnsignedDouble(floatParams[15]);
    const double angularVelocityDamping=getVortexUnsignedDouble(floatParams[16]);
//  const double slide_primary_linearAxis=getVortexUnsignedDouble(floatParams[17]);
//  const double slide_secondary_linearAxis=getVortexUnsignedDouble(floatParams[18]);
//  const double slide_primary_angularAxis=getVortexUnsignedDouble(floatParams[19]);
//  const double slide_secondary_angularAxis=getVortexUnsignedDouble(floatParams[20]);
//  const double slide_normal_angularAxis=getVortexUnsignedDouble(floatParams[21]);
//  const double slip_primary_linearAxis=getVortexUnsignedDouble(floatParams[22]);
//  const double slip_secondary_linearAxis=getVortexUnsignedDouble(floatParams[23]);
//  const double slip_primary_angularAxis=getVortexUnsignedDouble(floatParams[24]);
//  const double slip_secondary_angularAxis=getVortexUnsignedDouble(floatParams[25]);
//  const double slip_normal_angularAxis=getVortexUnsignedDouble(floatParams[26]);
    const double autoSleep_linear_speed_threshold=getVortexUnsignedDouble(floatParams[27]);
    const double autoSleep_linear_accel_threshold=getVortexUnsignedDouble(floatParams[28]);
    const double autoSleep_angular_speed_threshold=getVortexUnsignedDouble(floatParams[29]);
    const double autoSleep_angular_accel_threshold=getVortexUnsignedDouble(floatParams[30]);
    const double skinThickness=getVortexUnsignedDouble(floatParams[31]);
    double autoAngularDampingTensionRatio=getVortexUnsignedDouble(floatParams[32]);

    C3Vector primLinearAxisVector(floatParams[33],floatParams[34],floatParams[35]);
    C7Vector trTmp(collShapeDyn->getLocalInertiaFrame_scaled().getInverse());
    trTmp.X.clear();
    primLinearAxisVector=trTmp*primLinearAxisVector;
    Vx::VxVector3 frictiondirection=C3Vector2VxVector3(primLinearAxisVector);
    const Vx::VxReal frictiondirectionNorm = frictiondirection.normalize();

//  Vx::VxMaterial::FrictionModel frictModels[5];
//  for (int i=0;i<5;i++)
//  {
//      switch (intParams[i])
//      {
//          case 0:
//              frictModels[i]=Vx::VxMaterial::kFrictionModelBox;
//              break;
//          case 1:
//              frictModels[i]=Vx::VxMaterial::kFrictionModelScaledBox;
//              break;
//          case 2:
//              frictModels[i]=Vx::VxMaterial::kFrictionModelBoxProportionalLow;
//              break;
//          case 3:
//              frictModels[i]=Vx::VxMaterial::kFrictionModelBoxProportionalHigh;
//              break;
//          case 4:
//              frictModels[i]=Vx::VxMaterial::kFrictionModelScaledBoxFast;
//              break;
//          case 5:
//              frictModels[i]=Vx::VxMaterial::kFrictionModelNeutral;
//              break;
//          default:
//              frictModels[i]=Vx::VxMaterial::kFrictionModelNone;
//              break;
//      }
//  }
//  const bool treatPureShapesAsConvexShapes=((intParams[5]&1)!=0);
//  const bool treatConvexShapesAsRandomShapes=((intParams[5]&2)!=0);
//  const bool treatRandomShapesAsTerrain=((intParams[5]&4)!=0);
    const bool fastMoving=((intParams[5]&8)!=0);
    const bool autoSlip=((intParams[5]&16)!=0);
    const bool autoAngularDamping=((intParams[5]&256)!=0);
    const int autoSleepStepLiveThreshold=intParams[6];

    if (!autoAngularDamping)
        autoAngularDampingTensionRatio=0.0;

    C7Vector cumulPart1_scaled;
    _simGetObjectCumulativeTransformation(shape,cumulPart1_scaled.X.data,cumulPart1_scaled.Q.data,true);

    C7Vector tr(cumulPart1_scaled*collShapeDyn->getLocalInertiaFrame_scaled());
    CDummyGeomProxy* geomData=(CDummyGeomProxy*)_simGetGeomProxyFromShape(shape);
    CDummyGeomWrap* geomInfo=(CDummyGeomWrap*)_simGetGeomWrapFromGeomProxy(geomData);
    float mass=_simGetMass(geomInfo);

    _vortexRigidBody = new Vx::VxPart(mass);
    vortexWorld->addPart(_vortexRigidBody);

    _bodyIsKinematic=(_simIsShapeDynamicallyStatic(shape)||forceStatic);

    if (!_bodyIsKinematic)
    {
        _vortexRigidBody->setControl(Vx::VxPart::kControlDynamic);
        C3Vector im;
        _simGetPrincipalMomentOfInertia(geomInfo,im.data);

        Vx::VxMassProperties mp;
        mp.setMass(mass);
        Vx::VxReal33 inertia = { {im(0)*mass, 0, 0}, {0, im(1)*mass, 0}, {0, 0, im(2)*mass} };
        mp.setInertiaTensorLocal(inertia);
        mp.setCOMPositionLocal(Vx::VxVector3(0,0,0));
        _vortexRigidBody->setMassProperties(mp);

        C3Vector v;
        _simGetInitialDynamicVelocity(shape,v.data);
        if (v.getLength()>0.0f)
        {
            _vortexRigidBody->setLinearVelocity(C3Vector2VxVector3(v));
            _simSetInitialDynamicVelocity(shape,C3Vector::zeroVector.data); // important to reset it
        }
        _simGetInitialDynamicAngVelocity(shape,v.data);
        if (v.getLength()>0.0f)
        {
            _vortexRigidBody->setAngularVelocity(C3Vector2VxVector3(v));
            _simSetInitialDynamicAngVelocity(shape,C3Vector::zeroVector.data); // important to reset it
        }
        // Kinematic objects are handled elsewhere (at the end of the file) 

        // more robust for collision detection to avoid tunnel effect with fast moving objects.
#ifndef NEW_VORTEX_SOURCE
        _vortexRigidBody->setFastMoving(fastMoving);
#endif
    }
    else
    {
        //_vortexRigidBody->setControl(Vx::VxPart::kControlStatic); // part cannot move, optimize collision detection, good for walls, floors etc.
        _vortexRigidBody->setControl(Vx::VxPart::kControlAnimated); // part not dynamics but can move, ie has velocity, good for kinematic object
    }

    _vortexRigidBody->setPosition(C3Vector2VxVector3(tr.X));
    _vortexRigidBody->setOrientationQuaternion(tr.Q(0), tr.Q(1), tr.Q(2), tr.Q(3));

    vortex_skinThickness=skinThickness;
    vortex_autoSlip=autoSlip;
    vortex_angularVelocityDamping=angularVelocityDamping;
    vortex_autoAngularDampingTensionRatio=autoAngularDampingTensionRatio;

    _vortexRigidBody->userData().setData(Vx::VxUserData("vrep",_shapeID));
    _vortexRigidBody->userData().setData(Vx::VxUserData("vrep1",this));

    int index=0;
    while (true)
    { // add the individual collision shapes:
        Vx::VxCollisionGeometry* vortexGeomID=((CCollShapeDyn_vortex*)collShapeDyn)->getVortexGeoms(index++);
        if (vortexGeomID==NULL)
            break;
        vortexGeomID->setIntersectFilter(_sVortexIntersectFilter);
#ifdef NEW_VORTEX_SOURCE
        vortexGeomID->enableFastMoving(fastMoving);
#endif
        _vortexRigidBody->addCollisionGeometry(vortexGeomID);

        // stops as soon as a friction direction was found.
        if (_vortexRigidBody->getFrictionDirectionSubscriber() == NULL && frictiondirectionNorm > 0)
        {
            // if friction is isotropic, friction direction is useless
            if (isIsotropicMaterial(vortexGeomID) == false)
            {
                _vortexRigidBody->setFrictionDirectionSubscriber(new Vx::VxFrictionDirectionParallelToAxisSub(frictiondirection));
            }
        }
    }

    if (_vortexRigidBody->getControl() == Vx::VxPart::kControlDynamic)
    {
        _vortexRigidBody->setLinearVelocityDamping(linearVelocityDamping);
        _vortexRigidBody->setAngularVelocityDamping(angularVelocityDamping);
    }


    // In ODE and Bullet we disable the auto-sleep functionality, because with those engines,
    // if you remove the floor under disabled boxes, they will not automatically wake.
    // This is different with Vortex (correctly handled), thus, in Vortex, we
    // can keep the auto-sleep functionality!
    // Following are Vortex defaults:
    // lin. speed thresh.: 0.14
    // lin. accel. thresh.: 0.045
    // ang. speed thresh.: 0.03
    // ang. accel. thresh.: 0.045
    // step live thresh.: 10
    _vortexRigidBody->setAutoSleepLinearSpeedThreshold(autoSleep_linear_speed_threshold);
    _vortexRigidBody->setAutoSleepLinearAccelerationThreshold(autoSleep_linear_accel_threshold);
    _vortexRigidBody->setAutoSleepAngularSpeedThreshold(autoSleep_angular_speed_threshold);
    _vortexRigidBody->setAutoSleepAngularAccelerationThreshold(autoSleep_angular_accel_threshold);
    _vortexRigidBody->setAutoSleepStepLiveThreshold(autoSleepStepLiveThreshold);

    _bodyWasInitiallySleeping=false;
    if (_simGetStartSleeping(shape))
    {
        if (_simGetWasPutToSleepOnce(shape)==0) // this will set the flag to true!!
        { // Make sure that when the shape is added/removed because we are simply shifting it, that it doesn't stay deactivated!
            _bodyWasInitiallySleeping=true;
            _vortexRigidBody->wakeDynamics(false);
        }
    }
}

CRigidBodyDyn_vortex::~CRigidBodyDyn_vortex()
{
    if (_vortexRigidBody->getUniverse())
    {
        _vortexRigidBody->getUniverse()->removePart(_vortexRigidBody);
    }
    Vx::VxAssert (_vortexRigidBody->getConstraintCount()==0, "VxPart must be disconnected from all constraint before deleting");
    // the friction direction sub is deleted in the destructor
    delete _vortexRigidBody;
}

C7Vector CRigidBodyDyn_vortex::getInertiaFrameTransformation()
{ // return value is unscaled (relative to V-REP)
    C7Vector tr;

    Vx::VxReal4 quat;
    _vortexRigidBody->getOrientationQuaternion(quat);
    const Vx::VxTransform& tm = _vortexRigidBody->getTransform();
    tr.X(0)=(float)tm.t()[0];
    tr.X(1)=(float)tm.t()[1];
    tr.X(2)=(float)tm.t()[2];
    tr.Q(0)=(float)quat[0];
    tr.Q(1)=(float)quat[1];
    tr.Q(2)=(float)quat[2];
    tr.Q(3)=(float)quat[3];

    return(tr);
}

C7Vector CRigidBodyDyn_vortex::getShapeFrameTransformation()
{ // return value is unscaled (relative to V-REP)
    C7Vector tr;
    if (_collisionShapeDyn!=NULL)
    {

        Vx::VxReal4 quat;
        _vortexRigidBody->getOrientationQuaternion(quat);
        const Vx::VxTransform& tm = _vortexRigidBody->getTransform();
        tr.X(0)=(float)tm.t()[0];
        tr.X(1)=(float)tm.t()[1];
        tr.X(2)=(float)tm.t()[2];
        tr.Q(0)=(float)quat[0];
        tr.Q(1)=(float)quat[1];
        tr.Q(2)=(float)quat[2];
        tr.Q(3)=(float)quat[3];

        tr*=_collisionShapeDyn->getInverseLocalInertiaFrame_scaled();
    }
    else
        tr.setIdentity();
    return(tr);
}

Vx::VxPart* CRigidBodyDyn_vortex::getVortexRigidBody()
{
    return(_vortexRigidBody);
}

void CRigidBodyDyn_vortex::reportVelocityToShape(CDummyShape* shape)
{
    C3Vector lv,av;

    Vx::VxVector3 vlv=_vortexRigidBody->getLinearVelocity();
    Vx::VxVector3 vav=_vortexRigidBody->getAngularVelocity();
    lv.set((float)vlv[0],(float)vlv[1],(float)vlv[2]);
    av.set((float)vav[0], (float)vav[1], (float)vav[2]);

    _simSetShapeDynamicVelocity(shape,lv.data,av.data);
}

void CRigidBodyDyn_vortex::handleAdditionalForcesAndTorques(CDummyShape* shape)
{
    C3Vector vf,vt;
    _simGetAdditionalForceAndTorque(shape,vf.data,vt.data);

    if ((vf.getLength()!=0.0f)||(vt.getLength()!=0.0f))
    { // We should wake the body!!
        if (_vortexRigidBody->getControl()==Vx::VxPart::kControlDynamic)
        {
            _bodyWasInitiallySleeping=false;
            _vortexRigidBody->wakeDynamics(true);
            _vortexRigidBody->addForce(C3Vector2VxVector3(vf));
            _vortexRigidBody->addTorque(C3Vector2VxVector3(vt));
        }
    }
}

void CRigidBodyDyn_vortex::reportShapeConfigurationToRigidBody_forKinematicBody(CDummyShape* shape,float t,float cumulatedTimeStep)
{
    if (_bodyIsKinematic&&_applyBodyToShapeTransf_kinematicBody)
    { // static & moved (the desired movement was large enough)
        C7Vector tr;
        tr.buildInterpolation(_bodyStart_kinematicBody,_bodyEnd_kinematicBody,t);

        _vortexRigidBody->setPosition(C3Vector2VxVector3(tr.X));
        _vortexRigidBody->setOrientationQuaternion(tr.Q(0), tr.Q(1), tr.Q(2), tr.Q(3));
        //dBodyEnable(_vortexRigidBody);
        // Important to set the velocity here, so that collisions react correctly:
        C3Vector dx((_bodyEnd_kinematicBody.X-_bodyStart_kinematicBody.X));
        _vortexRigidBody->setLinearVelocity(C3Vector2VxVector3(dx)/cumulatedTimeStep);

        C4Vector q(_bodyEnd_kinematicBody.Q*_bodyStart_kinematicBody.Q.getInverse());
        C3Vector dEuler(q.getEulerAngles());
        _vortexRigidBody->setAngularVelocity(C3Vector2VxVector3(dEuler)/cumulatedTimeStep);
    }
}

void CRigidBodyDyn_vortex::applyCorrectEndConfig_forKinematicBody()
{
    if (_bodyIsKinematic&&_applyBodyToShapeTransf_kinematicBody)
    { // static & moved (the desired movement was large enough)
        C7Vector tr(_bodyEnd_kinematicBody);

        _vortexRigidBody->setPosition(C3Vector2VxVector3(tr.X));
        _vortexRigidBody->setOrientationQuaternion(tr.Q(0), tr.Q(1), tr.Q(2), tr.Q(3));
        //dBodyEnable(_vortexRigidBody);
        // Important to set the velocity here, so that collisions react correctly:
        _vortexRigidBody->resetDynamics();
        _vortexRigidBody->wakeDynamics();
    }
}

