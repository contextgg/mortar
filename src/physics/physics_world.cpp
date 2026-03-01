#include "physics/physics_world.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

#include <iostream>

using namespace JPH;

// Broad phase layers
namespace BroadPhaseLayers {
    static constexpr BroadPhaseLayer NON_MOVING(0);
    static constexpr BroadPhaseLayer MOVING(1);
    static constexpr uint NUM_LAYERS = 2;
}

// Object layers
namespace ObjectLayers {
    static constexpr ObjectLayer NON_MOVING = 0;
    static constexpr ObjectLayer MOVING = 1;
    static constexpr uint NUM_LAYERS = 2;
}

class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface {
public:
    uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override {
        if (layer == ObjectLayers::NON_MOVING) return BroadPhaseLayers::NON_MOVING;
        return BroadPhaseLayers::MOVING;
    }
};

class ObjVsBPLayerFilterImpl final : public ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(ObjectLayer obj, BroadPhaseLayer bp) const override {
        if (obj == ObjectLayers::NON_MOVING)
            return bp == BroadPhaseLayers::MOVING;
        return true;
    }
};

class ObjLayerPairFilterImpl final : public ObjectLayerPairFilter {
public:
    bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override {
        if (a == ObjectLayers::NON_MOVING && b == ObjectLayers::NON_MOVING) return false;
        return true;
    }
};

// Static instances for callbacks
static BPLayerInterfaceImpl s_bp_layer_interface;
static ObjVsBPLayerFilterImpl s_obj_vs_bp_filter;
static ObjLayerPairFilterImpl s_obj_layer_pair_filter;

static inline Vec3 to_jolt(glm::vec3 v) { return Vec3(v.x, v.y, v.z); }
static inline Quat to_jolt(glm::quat q) { return Quat(q.x, q.y, q.z, q.w); }
static inline glm::vec3 to_glm(Vec3 v) { return glm::vec3(v.GetX(), v.GetY(), v.GetZ()); }
static inline glm::quat to_glm(Quat q) { return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ()); }

void PhysicsWorld::init() {
    JPH::RegisterDefaultAllocator();

    Factory::sInstance = new Factory();
    RegisterTypes();

    _temp_allocator = new TempAllocatorImpl(10 * 1024 * 1024);
    _job_system = new JobSystemThreadPool(cMaxPhysicsJobs, cMaxPhysicsBarriers, 1);

    const uint max_bodies = 4096;
    const uint num_body_mutexes = 0; // auto
    const uint max_body_pairs = 4096;
    const uint max_contact_constraints = 2048;

    _physics_system = new PhysicsSystem();
    _physics_system->Init(max_bodies, num_body_mutexes, max_body_pairs, max_contact_constraints,
                          s_bp_layer_interface, s_obj_vs_bp_filter, s_obj_layer_pair_filter);

    _physics_system->SetGravity(Vec3(0, -9.81f, 0));
}

void PhysicsWorld::shutdown() {
    destroy_character();

    delete _physics_system;
    delete _job_system;
    delete _temp_allocator;

    UnregisterTypes();
    delete Factory::sInstance;
    Factory::sInstance = nullptr;
}

void PhysicsWorld::step(float dt) {
    if (_character) {
        CharacterVirtual::ExtendedUpdateSettings settings;
        BroadPhaseLayerFilter bp_filter;
        ObjectLayerFilter obj_filter;
        BodyFilter body_filter;
        ShapeFilter shape_filter;
        _character->ExtendedUpdate(dt, -_physics_system->GetGravity(),
                                    settings, bp_filter, obj_filter,
                                    body_filter, shape_filter, *_temp_allocator);
    }

    const int collision_steps = 1;
    _physics_system->Update(dt, collision_steps, _temp_allocator, _job_system);
}

uint32_t PhysicsWorld::add_box(glm::vec3 half_extents, glm::vec3 position, glm::quat rotation,
                                bool is_static, uint64_t entity_id) {
    auto& body_interface = _physics_system->GetBodyInterface();

    BoxShapeSettings box_settings(to_jolt(half_extents));
    auto shape_result = box_settings.Create();
    if (shape_result.HasError()) {
        std::cerr << "Failed to create box shape" << std::endl;
        return UINT32_MAX;
    }

    EMotionType motion = is_static ? EMotionType::Static : EMotionType::Dynamic;
    ObjectLayer layer = is_static ? ObjectLayers::NON_MOVING : ObjectLayers::MOVING;

    BodyCreationSettings body_settings(shape_result.Get(), to_jolt(position),
                                        to_jolt(rotation), motion, layer);

    Body* body = body_interface.CreateBody(body_settings);
    if (!body) return UINT32_MAX;

    body->SetUserData(entity_id);
    body_interface.AddBody(body->GetID(), is_static ? EActivation::DontActivate : EActivation::Activate);

    return body->GetID().GetIndexAndSequenceNumber();
}

uint32_t PhysicsWorld::add_sphere(float radius, glm::vec3 position, bool is_static, uint64_t entity_id) {
    auto& body_interface = _physics_system->GetBodyInterface();

    SphereShapeSettings sphere_settings(radius);
    auto shape_result = sphere_settings.Create();
    if (shape_result.HasError()) return UINT32_MAX;

    EMotionType motion = is_static ? EMotionType::Static : EMotionType::Dynamic;
    ObjectLayer layer = is_static ? ObjectLayers::NON_MOVING : ObjectLayers::MOVING;

    BodyCreationSettings body_settings(shape_result.Get(), to_jolt(position),
                                        Quat::sIdentity(), motion, layer);

    Body* body = body_interface.CreateBody(body_settings);
    if (!body) return UINT32_MAX;

    body->SetUserData(entity_id);
    body_interface.AddBody(body->GetID(), is_static ? EActivation::DontActivate : EActivation::Activate);

    return body->GetID().GetIndexAndSequenceNumber();
}

void PhysicsWorld::remove_body(uint32_t body_id) {
    auto& body_interface = _physics_system->GetBodyInterface();
    BodyID id = BodyID(body_id);
    if (body_interface.IsAdded(id)) {
        body_interface.RemoveBody(id);
        body_interface.DestroyBody(id);
    }
}

glm::vec3 PhysicsWorld::get_position(uint32_t body_id) const {
    auto& body_interface = _physics_system->GetBodyInterface();
    return to_glm(body_interface.GetPosition(BodyID(body_id)));
}

glm::quat PhysicsWorld::get_rotation(uint32_t body_id) const {
    auto& body_interface = _physics_system->GetBodyInterface();
    return to_glm(body_interface.GetRotation(BodyID(body_id)));
}

void PhysicsWorld::set_linear_velocity(uint32_t body_id, glm::vec3 velocity) {
    auto& body_interface = _physics_system->GetBodyInterface();
    body_interface.SetLinearVelocity(BodyID(body_id), to_jolt(velocity));
}

void PhysicsWorld::create_character(glm::vec3 position, float radius, float height) {
    // Capsule shape: half-height is the cylinder portion
    float half_height = (height - 2.0f * radius) * 0.5f;
    if (half_height < 0.01f) half_height = 0.01f;

    Ref<CapsuleShape> capsule = new CapsuleShape(half_height, radius);

    CharacterVirtualSettings settings;
    settings.mShape = capsule;
    settings.mMaxSlopeAngle = DegreesToRadians(50.0f);
    settings.mMass = 70.0f;
    settings.mMaxStrength = 100.0f;
    settings.mPenetrationRecoverySpeed = 1.0f;

    _character = new CharacterVirtual(&settings, to_jolt(position), Quat::sIdentity(), 0, _physics_system);
}

void PhysicsWorld::update_character(float /*dt*/, glm::vec3 desired_velocity) {
    if (!_character) return;
    _character->SetLinearVelocity(to_jolt(desired_velocity));
}

glm::vec3 PhysicsWorld::get_character_position() const {
    if (!_character) return glm::vec3(0);
    return to_glm(_character->GetPosition());
}

bool PhysicsWorld::is_character_grounded() const {
    if (!_character) return false;
    return _character->GetGroundState() == CharacterVirtual::EGroundState::OnGround;
}

void PhysicsWorld::destroy_character() {
    delete _character;
    _character = nullptr;
}

std::optional<RaycastResult> PhysicsWorld::raycast(glm::vec3 origin, glm::vec3 direction, float max_distance) const {
    const auto& query = _physics_system->GetNarrowPhaseQuery();

    RRayCast ray(to_jolt(origin), to_jolt(direction * max_distance));
    RayCastResult hit;

    if (query.CastRay(ray, hit)) {
        auto& body_interface = _physics_system->GetBodyInterface();
        BodyID hit_body = hit.mBodyID;

        RaycastResult result;
        result.hit_position = to_glm(ray.GetPointOnRay(hit.mFraction));
        result.distance = hit.mFraction * max_distance;
        result.entity_id = body_interface.GetUserData(hit_body);
        result.hit_normal = glm::vec3(0, 1, 0); // Simplified for now
        return result;
    }

    return std::nullopt;
}
